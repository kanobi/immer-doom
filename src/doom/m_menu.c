//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//    DOOM selection menu, options, episode etc.
//    Sliders and icons. Kinda widget stuff.
//


#include <stdlib.h>
#include <ctype.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"
#include "d_main.h"
#include "deh_main.h"
#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "r_local.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_controls.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "s_sound.h"
#include "doomstat.h"
// Data.
#include "sounds.h"
#include "m_menu.h"


//
// defaulted values
//
int mouseSensitivity = 5,
    showMessages = 1,       // Show messages has default, 0 = off, 1 = on
    detailLevel = 0,        // Blocky mode, has default, 0 = high, 1 = normal
    screenblocks = 9,
    screenSize,             // temp for screenblocks (0-9)
    quickSaveSlot,          // -1 = no quicksave slot picked!
    messageToPrint;         //  1 = message to be printed

const char *messageString;
int messageLastMenuActive;
void (*messageRoutine)(int response);
boolean messageNeedsInput; // timed message = no input from user

static const char line_height = 16;

// static const char *detailNames[2] = {"M_GDHIGH","M_GDLOW"};
static const char *msgToggleStrings[2] = {"M_MSGOFF", "M_MSGON"};
static const char *skullIconNames[2] = {"M_SKULL1", "M_SKULL2"};

static char tempstring[90];
char endstring[160];
char saveOldString[SAVESTRINGSIZE];  // old save description before edit
char savegamestrings[10][SAVESTRINGSIZE];

char gammamsg[5][26] =
{
    GAMMALVL0,
    GAMMALVL1,
    GAMMALVL2,
    GAMMALVL3,
    GAMMALVL4
};

int  saveStringEnter;                // we are going to be entering a savegame string
int  saveSlot;                       // which slot to save in
int  saveCharIndex;                  // which char we're editing

static boolean joypadSave = false;   // was the save action initiated by joypad?

boolean inhelpscreens;
boolean menuactive;

int    itemOn;            // menu item skull is on
short  skullAnimCounter;  // skull animation counter
short  whichSkull;        // which skull to draw
static boolean OPL_midi_debug;

//
// MENU TYPEDEFS
//
// choice = menu item #. 
// if status = 2: 
//    choice=0:leftarrow, 
//    chouce=1:rightarrow
typedef struct
{
    short status; // 0 = no cursor here, 1 = ok, 2 = arrows ok
    char  name[10];
    void  (*routine)(int choice);
    char  alphaKey; // hotkey in menu
} menuitem_t;

typedef struct menu_s
{
    short           numitems;       // # of menu items
    struct  menu_s* prevMenu;       // previous menu
    menuitem_t*     menuitems;      // menu items
    void            (*routine)();   // draw routine
    short           x;              // x, y of menu 
    short           y;              
    short           lastOn;         // last item user was on in menu
} menu_t;
menu_t* currentMenu; // current menudef

//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);
static void M_Options(int choice);
static void M_EndGame(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
static void M_QuitDOOM(int choice);

static void M_ToggleMessages(int choice);
static void M_ChangeSensitivity(int choice);
static void M_SfxVol(int choice);
static void M_MusicVol(int choice);
static void M_Sound(int choice);
static void M_FinishReadThis(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);

static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawOptions(void);
static void M_DrawSound(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

static void M_DrawSaveLoadBorder(int x, int y);
static void M_SetupNextMenu(menu_t *menudef);
static void M_DrawThermo(int x, int y, int thermWidth, int thermDot);
static void M_WriteText(int x, int y, const char *string);
static int  M_StringWidth(const char *string);
static int  M_StringHeight(const char *string);
static void M_StartMessage(const char *string, void *routine, boolean input);
static void M_ClearMenus (void);

// static void M_ChangeDetail(int choice);
// static void M_SizeDisplay(int choice);


//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

menuitem_t MainMenu[]=
{
    {1,"M_NGAME",M_NewGame,'n'},
    {1,"M_OPTION",M_Options,'o'},
    {1,"M_LOADG",M_LoadGame,'l'},
    {1,"M_SAVEG",M_SaveGame,'s'},
    {1,"M_RDTHIS",M_ReadThis,'r'},
    {1,"M_QUITG",M_QuitDOOM,'q'}
};

menu_t MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    SCREENWIDTH / 2 - 63,
    68,
    0
};


//
// EPISODE SELECT
//
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

menuitem_t EpisodeMenu[]=
{
    {1,"M_EPI1", M_Episode,'k'},
    {1,"M_EPI2", M_Episode,'t'},
    {1,"M_EPI3", M_Episode,'i'},
    {1,"M_EPI4", M_Episode,'t'}
};

menu_t EpiDef =
{
    ep_end,          // # of menu items
    &MainDef,        // previous menu
    EpisodeMenu,    // menuitem_t ->
    M_DrawEpisode,     // drawing routine ->
    SCREENWIDTH / 2 - 112, // default x=48
    63,                    // default y=63
    ep1                // lastOn
};

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

menuitem_t NewGameMenu[]=
{
    {1,"M_JKILL",    M_ChooseSkill, 'i'},
    {1,"M_ROUGH",    M_ChooseSkill, 'h'},
    {1,"M_HURT",     M_ChooseSkill, 'h'},
    {1,"M_ULTRA",    M_ChooseSkill, 'u'},
    {1,"M_NMARE",    M_ChooseSkill, 'n'}
};

menu_t  NewDef =
{
    newg_end,              // # of menu items
    &EpiDef,               // previous menu
    NewGameMenu,           // menuitem_t ->
    M_DrawNewGame,         // drawing routine ->
    SCREENWIDTH / 2 - 112, // 48,
    63,              
    hurtme                 // lastOn
};


//
// OPTIONS MENU
//
enum
{
    endgame,
    messages,
    // detail,
    // scrnsize,
    option_empty1,
    mousesens,
    option_empty2,
    soundvol,
    opt_end
} options_e;

menuitem_t OptionsMenu[]=
{
    {1, "M_ENDGAM", M_EndGame,           'e'},
    {1, "M_MESSG",  M_ToggleMessages,    'm'},
    // {1, "M_DETAIL", M_ChangeDetail,         'g'},
    // {2, "M_SCRNSZ", M_SizeDisplay,       's'},
    {-1,"",         0,                   '\0'},
    {2, "M_MSENS",  M_ChangeSensitivity, 'm'},
    {-1, "",        0,                   '\0'},
    {1, "M_SVOL",   M_Sound,             's'}
};

menu_t OptionsDef =
{
    opt_end,
    &MainDef,
    OptionsMenu,
    M_DrawOptions,
    SCREENWIDTH / 2 - 100, // default 60
    37,
    0
};

//
// Read This! MENU 1 & 2
//
enum
{
    rdthsempty1,
    read1_end
} read_e;

menuitem_t ReadMenu1[] =
{
    {
        1,
        "",
        M_ReadThis2,
        0
    }
};

menu_t ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,
    185,
    0
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

menuitem_t ReadMenu2[]=
{
    {
        1,
        "",
        M_FinishReadThis,
        0
    }
};

menu_t ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330,
    175,
    0
};

//
// SOUND VOLUME MENU
//
enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
} sound_e;

menuitem_t SoundMenu[]=
{
    {2,"M_SFXVOL",M_SfxVol,'s'},
    {-1,"",0,'\0'},
    {2,"M_MUSVOL",M_MusicVol,'m'},
    {-1,"",0,'\0'}
};

menu_t  SoundDef =
{
    sound_end,
    &OptionsDef,
    SoundMenu,
    M_DrawSound,
    SCREENWIDTH / 2 - 80, // default 80
    64,
    0
};

//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
} load_e;

menuitem_t LoadMenu[]=
{
    {1,"", M_LoadSelect,'1'},
    {1,"", M_LoadSelect,'2'},
    {1,"", M_LoadSelect,'3'},
    {1,"", M_LoadSelect,'4'},
    {1,"", M_LoadSelect,'5'},
    {1,"", M_LoadSelect,'6'}
};

menu_t LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
     SCREENWIDTH / 2 - 90,
     54,
    0
};

//
// SAVE GAME MENU
//
menuitem_t SaveMenu[]=
{
    {1,"", M_SaveSelect,'1'},
    {1,"", M_SaveSelect,'2'},
    {1,"", M_SaveSelect,'3'},
    {1,"", M_SaveSelect,'4'},
    {1,"", M_SaveSelect,'5'},
    {1,"", M_SaveSelect,'6'}
};

menu_t SaveDef =
{
    load_end,
    &MainDef,
    SaveMenu,
    M_DrawSave,
    SCREENWIDTH / 2 - 90,
    54,
    0
};


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
    FILE *handle;
    char name[256];
    size_t return_value;

    for (int i = 0;i < load_end;i++)
    {
        return_value = 0;
        M_StringCopy(name, P_SaveGameFile(i), sizeof(name));
        handle = M_fopen(name, "rb");
        if (handle == NULL)
        {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
            continue;
        }
        return_value = fread(&savegamestrings[i], 1, SAVESTRINGSIZE, handle);
        fclose(handle);
        LoadMenu[i].status = (short) (return_value == SAVESTRINGSIZE);
    }
}


//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
    V_DrawPatchDirect(
        SCREENWIDTH / 2 - 90,
        28,  
        W_CacheLumpName(DEH_String("M_LOADG"), PU_CACHE));
    
    for (int load_slot_idx = 0; load_slot_idx < load_end; load_slot_idx++)
    {
        M_DrawSaveLoadBorder(LoadDef.x, LoadDef.y + line_height * load_slot_idx);
        M_WriteText(LoadDef.x, LoadDef.y + line_height * load_slot_idx, savegamestrings[load_slot_idx]);
    }
}


//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y)
{
    V_DrawPatchDirect(
            x - 8,
            y + 7,
            W_CacheLumpName(DEH_String("M_LSLEFT"), PU_CACHE));
    
    for (int i = 0; i < 24; i++)
    {
        V_DrawPatchDirect(x, y + 7, W_CacheLumpName(DEH_String("M_LSCNTR"), PU_CACHE));
        x += 8;
    }
    
    V_DrawPatchDirect(x, y + 7, W_CacheLumpName(DEH_String("M_LSRGHT"), PU_CACHE));
}


//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
    char name[256];
    M_StringCopy(name, P_SaveGameFile(choice), sizeof(name));
    G_LoadGame (name);
    M_ClearMenus();
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
    if (netgame)
    {
        M_StartMessage(DEH_String(LOADNET),NULL,false);
        return;
    }
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
void M_DrawSave(void)
{
    V_DrawPatchDirect(
        SCREENWIDTH / 2 - 90,
        28,
        W_CacheLumpName(DEH_String("M_SAVEG"), PU_CACHE));
    
    for (int save_slot_idx = 0; save_slot_idx < load_end; save_slot_idx++)
    {
        M_DrawSaveLoadBorder(SaveDef.x, SaveDef.y + line_height * save_slot_idx);
        M_WriteText(SaveDef.x,SaveDef.y + line_height * save_slot_idx, savegamestrings[save_slot_idx]);
    }
    
    if (saveStringEnter)
    {
        int i = M_StringWidth(savegamestrings[saveSlot]);
        M_WriteText(SaveDef.x + i, SaveDef.y + line_height * saveSlot, "_");
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
        quickSaveSlot = slot;
}

//
// Generate a default save slot name when the user saves to
// an empty slot via the joypad.
//
static void SetDefaultSaveName(int slot)
{
    // map from IWAD or PWAD?
    if (W_IsIWADLump(maplumpinfo) && strcmp(savegamedir, ""))
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE, "%s", maplumpinfo->name);
    else
    {
        char *wadname = M_StringDuplicate(W_WadNameForLump(maplumpinfo));
        char *ext = strrchr(wadname, '.');
        if (ext != NULL)
            *ext = '\0';

        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE,
                   "%s (%s)", maplumpinfo->name,
                   wadname);
        free(wadname);
    }
    M_ForceUppercase(savegamestrings[itemOn]);
    joypadSave = false;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    int x, y;
    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * line_height - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + line_height - 2);
    saveSlot = choice;
    M_StringCopy(saveOldString,savegamestrings[choice], SAVESTRINGSIZE);
    if (!strcmp(savegamestrings[choice], EMPTYSTRING))
    {
        savegamestrings[choice][0] = 0;
        if (joypadSave)
        {
            SetDefaultSaveName(choice);
        }
    }
    saveCharIndex = (int) strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
    if (!usergame)
    {
        M_StartMessage(DEH_String(SAVEDEAD),NULL,false);
        return;
    }
    if (gamestate != GS_LEVEL)
        return;
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}


//
//      M_QuickSave
//
void M_QuickSaveResponse(int key)
{
    if (key == key_menu_confirm)
    {
        M_DoSave(quickSaveSlot);
        S_StartSound(NULL,sfx_swtchx);
    }
}


void M_QuickSave(void)
{
    if (!usergame)
    {
        S_StartSound(NULL,sfx_oof);
        return;
    }

    if (gamestate != GS_LEVEL)
        return;
    
    if (quickSaveSlot < 0)
    {
        M_StartControlPanel();
        M_ReadSaveStrings();
        M_SetupNextMenu(&SaveDef);
        quickSaveSlot = -2;    // means to pick a slot now
        return;
    }
    DEH_snprintf(
        tempstring, 
        sizeof(tempstring),
        QSPROMPT,
        savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickSaveResponse, true);
}



//
// M_QuickLoad
//
void M_QuickLoadResponse(int key)
{
    if (key == key_menu_confirm)
    {
        M_LoadSelect(quickSaveSlot);
        S_StartSound(NULL,sfx_swtchx);
    }
}


void M_QuickLoad(void)
{
    if (netgame)
    {
        M_StartMessage(DEH_String(QLOADNET),NULL,false);
        return;
    }
    
    if (quickSaveSlot < 0)
    {
        M_StartMessage(DEH_String(QSAVESPOT),NULL,false);
        return;
    }
    DEH_snprintf(
        tempstring,
        sizeof(tempstring),
        QLPROMPT,
        savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickLoadResponse, true);
}


//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
    inhelpscreens = true;
    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP2"), PU_CACHE));
}


//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
    inhelpscreens = true;
    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP1"), PU_CACHE));
}


void M_DrawReadThisCommercial(void)
{
    inhelpscreens = true;
    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP"), PU_CACHE));
}


//
// Change Sfx & Music volumes
//
void M_DrawSound(void)
{
    V_DrawPatchDirect(
        SCREENWIDTH / 2 - 100, 
        38,
        W_CacheLumpName(DEH_String("M_SVOL"), PU_CACHE));

    M_DrawThermo(SoundDef.x,SoundDef.y + line_height*(sfx_vol+1),
         16,sfxVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y + line_height*(music_vol+1),
         16,musicVolume);
}

void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice)
{
    switch(choice)
    {
    case 0:
        if (sfxVolume)
            sfxVolume--;
        break;
    case 1:
        if (sfxVolume < 15)
            sfxVolume++;
        break;
    default:
        break;
    }
    S_SetSfxVolume(sfxVolume * 8);
}

void M_MusicVol(int choice)
{
    switch(choice)
    {
    case 0:
        if (musicVolume)
            musicVolume--;
        break;
    case 1:
        if (musicVolume < 15)
            musicVolume++;
        break;
    default:
        break;
    }
    S_SetMusicVolume(musicVolume * 8);
}


//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
    V_DrawPatchDirect(
        SCREENWIDTH / 2 - 66,
        2, 
        W_CacheLumpName(DEH_String("M_DOOM"), PU_CACHE));
}


//
// M_NewGame
//
void M_DrawNewGame(void)
{
    V_DrawPatchDirect(
        SCREENWIDTH / 2 - 64,
        14,
        W_CacheLumpName(DEH_String("M_NEWG"),
        PU_CACHE));
    V_DrawPatchDirect(
        SCREENWIDTH / 2 - 106,
        38,
        W_CacheLumpName(DEH_String("M_SKILL"),
        PU_CACHE));
}

void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
        M_StartMessage(DEH_String(NEWGAME),NULL,false);
        return;
    }
    
    // Chex Quest disabled the episode select screen, as did Doom II.
    if (gamemode == commercial || gameversion == exe_chex)
        M_SetupNextMenu(&NewDef);
    else
        M_SetupNextMenu(&EpiDef);
}


//
//      M_Episode
//
int epi;
void M_DrawEpisode(void)
{
    V_DrawPatchDirect(
        SCREENWIDTH / 2 - 106,
        38, 
        W_CacheLumpName(DEH_String("M_EPISOD"), 
        PU_CACHE));
}

void M_VerifyNightmare(int key)
{
    if (key != key_menu_confirm)
        return;
        
    G_DeferedInitNew(nightmare,epi+1,1);
    M_ClearMenus ();
}

void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
        M_StartMessage(DEH_String(NIGHTMARE),M_VerifyNightmare,true);
        return;
    }
    G_DeferedInitNew(choice,epi+1,1);
    M_ClearMenus ();
}

void M_Episode(int choice)
{
    if ( (gamemode == shareware) && choice)
    {
        M_StartMessage(DEH_String(SWSTRING),NULL,false);
        M_SetupNextMenu(&ReadDef1);
        return;
    }
    epi = choice;
    M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
void M_DrawOptions(void)
{
    V_DrawPatchDirect(108, 15, W_CacheLumpName(DEH_String("M_OPTTTL"), PU_CACHE));
    
    V_DrawPatchDirect(
        OptionsDef.x + 120,
        OptionsDef.y + line_height * messages,
        W_CacheLumpName(DEH_String(msgToggleStrings[showMessages]), PU_CACHE));

    M_DrawThermo(OptionsDef.x, OptionsDef.y + line_height * (mousesens + 1), 10, mouseSensitivity);

    // M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1), 9,screenSize);
}

void M_Options(int choice)
{
    M_SetupNextMenu(&OptionsDef);
}


//
//      Toggle messages on/off
//
void M_ToggleMessages(int choice)
{
    showMessages = 1 - showMessages;
    if (!showMessages)
        players[consoleplayer].message = DEH_String(MSGOFF);
    else
        players[consoleplayer].message = DEH_String(MSGON);

    message_dontfuckwithme = true;
}


//
// M_EndGame
//
void M_EndGameResponse(int key)
{
    if (key != key_menu_confirm)
        return;
        
    currentMenu->lastOn = (short) itemOn;
    M_ClearMenus();
    D_StartTitle();
}

void M_EndGame(int choice)
{
    if (!usergame)
    {
        S_StartSound(NULL, sfx_oof);
        return;
    }
    
    if (netgame)
    {
        M_StartMessage(DEH_String(NETEND), NULL, false);
        return;
    }
    
    M_StartMessage(DEH_String(ENDGAME), M_EndGameResponse, true);
}


//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
    M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
    M_SetupNextMenu(&MainDef);
}

//
// M_QuitDOOM
//
int quitsounds[8] =
{
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

int quitsounds2[8] =
{
    sfx_vilact,
    sfx_getpow,
    sfx_boscub,
    sfx_slop,
    sfx_skeswg,
    sfx_kntdth,
    sfx_bspact,
    sfx_sgtatk
};


void M_QuitResponse(int key)
{
    if (key != key_menu_confirm)
        return;

    if (!netgame)
    {
        if (gamemode == commercial)
            S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
        else
            S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
        I_WaitVBL(105);
    }

    I_Quit ();
}


static const char *M_SelectEndMessage(void)
{
    const char **endmsg = logical_gamemission == doom ? doom1_endmsg : doom2_endmsg;
    return endmsg[gametic % NUM_QUITMESSAGES];
}


void M_QuitDOOM(int choice)
{
    DEH_snprintf(endstring, sizeof(endstring), "%s\n\n" DOSY, DEH_String(M_SelectEndMessage()));
    M_StartMessage(endstring,M_QuitResponse,true);
}


void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
    case 0:
        if (mouseSensitivity)
            mouseSensitivity--;
        break;
    case 1:
        if (mouseSensitivity < 9)
            mouseSensitivity++;
        break;
    default:
        break;
    }
}

//void M_ChangeDetail(int choice)
//{
//    detailLevel = 1 - detailLevel;
//    R_SetViewSize (screenblocks, detailLevel);
//    players[consoleplayer].message = !detailLevel ? DEH_String(DETAILHI) : DEH_String(DETAILLO);
//}

//void M_SizeDisplay(int choice)
//{
//    switch(choice) // NOLINT(hicpp-multiway-paths-covered)
//    {
//    case 0:
//        if (screenSize > 0)
//        {
//            screenblocks--;
//            screenSize--;
//        }
//        break;
//    case 1:
//        if (screenSize < 8)
//        {
//            screenblocks++;
//            screenSize++;
//        }
//    }
//    R_SetViewSize (screenblocks, detailLevel);
//}


//
//  Draw thermostat menu item
//
void M_DrawThermo (int x, int y, int thermWidth, int thermDot)
{
    int xx = x;
    V_DrawPatchDirect(xx, y, W_CacheLumpName(DEH_String("M_THERML"), PU_CACHE));
    xx += 8;
    for (int i=0;i<thermWidth;i++)
    {
        V_DrawPatchDirect(xx, y, W_CacheLumpName(DEH_String("M_THERMM"), PU_CACHE));
        xx += 8;
    }
    V_DrawPatchDirect(xx, y, W_CacheLumpName(DEH_String("M_THERMR"), PU_CACHE));
    V_DrawPatchDirect((x + 8) + thermDot * 8, y, W_CacheLumpName(DEH_String("M_THERMO"), PU_CACHE));
}


void M_StartMessage ( const char *string, void* routine, boolean input)
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
}


//
// Find string width from hu_font chars
//
int M_StringWidth(const char *string)
{
    int character_i, string_width = 0;
    for (size_t i = 0; i < strlen(string); i++)
    {
        character_i = toupper(string[i]) - HU_FONTSTART;
        string_width += (character_i < 0 || character_i >= HU_FONTSIZE) ? 4 : SHORT (hu_font[character_i]->width);
    }   
    return string_width;
}


//
// Find string height from hu_font chars
//
int M_StringHeight (const char* string)
{
    int height = SHORT(hu_font[0]->height);   
    int return_height = height;
    for (size_t i = 0; i < strlen(string); i++)
    {
        if (string[i] == '\n')
            return_height += height;
    }
    return return_height;
}


//
//  Write a string using the hu_font
//
void M_WriteText (int x, int y, const char *string)
{
    int character_x_pos = x,
        character_y_pos = y,
        character_width,
        current_character;
    const char *ch = string;

    while(1)
    {
        current_character = (int) *ch++;
        if (!current_character)
            break;

        if (current_character == '\n')
        {
            character_x_pos = x;
            character_y_pos += 12;
            continue;
        }

        current_character = toupper(current_character) - HU_FONTSTART;
        if (current_character < 0 || current_character >= HU_FONTSIZE)
        {
            character_x_pos += 4;
            continue;
        }

        character_width = SHORT (hu_font[current_character]->width);
        if (character_x_pos + character_width > SCREENWIDTH)
            break;

        V_DrawPatchDirect(character_x_pos, character_y_pos, hu_font[current_character]);
        character_x_pos += character_width;
    }
}

// These keys evaluate to a "null" key in Vanilla Doom that allows weird
// jumping in the menus. Preserve this behavior for accuracy.
static boolean IsNullKey(int key)
{
    return  key == KEY_PAUSE ||
            key == KEY_CAPSLOCK ||
            key == KEY_SCRLCK ||
            key == KEY_NUMLOCK;
}


boolean IsJoystickButtonPressed(int button, int data)
{
    if (!button)
        return false;

    if ((data & 1 << button) != 0)
        return true;
    else
        return false;
}

//
// CONTROL PANEL
//

//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int character_typed, key;

    static int
        mousewait = 0,
        mousey = 0,
        lasty = 0,
        mousex = 0,
        lastx = 0;

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.
    if (testcontrols)
    {
        if (ev->type == ev_quit ||
                (ev->type == ev_keydown && (ev->data1 == key_menu_activate || ev->data1 == key_menu_quit)))
        {
            I_Quit();
        }
        return false;
    }

    // "close" button pressed on window?
    if (ev->type == ev_quit)
    {
        // First click on close button = bring up quit confirm message.
        // Second click on close button = confirm quit
        if (menuactive && messageToPrint && messageRoutine == M_QuitResponse)
            M_QuitResponse(key_menu_confirm);
        else
        {
            S_StartSound(NULL,sfx_swtchn);
            M_QuitDOOM(0);
        }
        return true;
    }

    // key is the key pressed, character_typed is the actual character typed
    character_typed = 0;
    key = -1;
    if (ev->type == ev_joystick)
    {
        if (menuactive)
        {
            // Simulate key presses from joystick events to interact with the menu.
            if (ev->data3 < 0)
            {
                key = key_menu_up;
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 > 0)
            {
                key = key_menu_down;
                joywait = I_GetTime() + 5;
            }

            if (ev->data2 < 0)
            {
                key = key_menu_left;
                joywait = I_GetTime() + 2;
            }
            else if (ev->data2 > 0)
            {
                key = key_menu_right;
                joywait = I_GetTime() + 2;
            }

            if (IsJoystickButtonPressed(joybfire, ev->data1))
            {
                if (messageToPrint && messageNeedsInput)
                {
                    // Simulate a 'Y' keypress when Doom show a Y/N dialog with Fire button.
                    key = key_menu_confirm;
                }
                else if (saveStringEnter)
                {
                    // Simulate pressing "Enter" when we are supplying a save slot name
                    key = KEY_ENTER;
                }
                else
                {
                    if (currentMenu == &SaveDef)
                    {
                        // if selecting a save slot via joypad, set a flag
                        joypadSave = true;
                    }
                    key = key_menu_forward;
                }
                joywait = I_GetTime() + 5;
            }
            if (IsJoystickButtonPressed(joybuse, ev->data1))
            {
                if (messageToPrint && messageNeedsInput)
                {
                    // Simulate a 'N' keypress when Doom show a Y/N dialog with Use button.
                    key = key_menu_abort;
                }
                else if (saveStringEnter)
                {
                    // If user was entering a save name, back out
                    key = KEY_ESCAPE;
                }
                else
                {
                    key = key_menu_back;
                }
                joywait = I_GetTime() + 5;
            }
        }
        if (IsJoystickButtonPressed(joybmenu, ev->data1))
        {
            key = key_menu_activate;
            joywait = I_GetTime() + 5;
        }
    }
    else
    {
        if (ev->type == ev_mouse && mousewait < I_GetTime())
        {
            mousey += ev->data3;
            if (mousey < lasty-30)
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 5;
                mousey = lasty -= 30;
            }
            else if (mousey > lasty+30)
            {
                key = key_menu_up;
                mousewait = I_GetTime() + 5;
                mousey = lasty += 30;
            }

            mousex += ev->data2;
            if (mousex < lastx-30)
            {
                key = key_menu_left;
                mousewait = I_GetTime() + 5;
                mousex = lastx -= 30;
            }
            else if (mousex > lastx+30)
            {
                key = key_menu_right;
                mousewait = I_GetTime() + 5;
                mousex = lastx += 30;
            }

            if (ev->data1 & 1)
            {
                key = key_menu_forward;
                mousewait = I_GetTime() + 15;
            }

            if (ev->data1 & 2)
            {
                key = key_menu_back;
                mousewait = I_GetTime() + 15;
            }
        }
        else
        {
            if (ev->type == ev_keydown)
            {
                key = ev->data1;
                character_typed = ev->data2;
            }
        }
    }
    
    if (key == -1)
        return false;

    // Save Game string input
    if (saveStringEnter)
    {
        switch(key)
        {
        case KEY_BACKSPACE:
            if (saveCharIndex > 0)
            {
                saveCharIndex--;
                savegamestrings[saveSlot][saveCharIndex] = 0;
            }
            break;
        case KEY_ESCAPE:
            saveStringEnter = 0;
            I_StopTextInput();
            M_StringCopy(savegamestrings[saveSlot],
                         saveOldString,
                     SAVESTRINGSIZE);
            break;
        case KEY_ENTER:
            saveStringEnter = 0;
            I_StopTextInput();
            if (savegamestrings[saveSlot][0])
                M_DoSave(saveSlot);
            break;
        default:
            // Savegame name entry. This is complicated.
            // Vanilla has a bug where the shift key is ignored when entering
            // a savegame name. If vanilla_keyboard_mapping is on, we want
            // to emulate this bug by using ev->data1. But if it's turned off,
            // it implies the user doesn't care about Vanilla emulation:
            // instead, use ev->data3 which gives the fully-translated and
            // modified key input.

            if (ev->type != ev_keydown)
                break;

            if (vanilla_keyboard_mapping)
                character_typed = ev->data1;
            else
                character_typed = ev->data3;

            character_typed = toupper(character_typed);

            if (character_typed != ' ' &&
                (character_typed - HU_FONTSTART < 0 || character_typed - HU_FONTSTART >= HU_FONTSIZE))
                break;

            if (character_typed >= 32 &&
                character_typed <= 127 &&
                saveCharIndex < SAVESTRINGSIZE-1 &&
                M_StringWidth(savegamestrings[saveSlot]) < (SAVESTRINGSIZE-2)*8)
            {
                savegamestrings[saveSlot][saveCharIndex++] = (char) character_typed;
                savegamestrings[saveSlot][saveCharIndex] = 0;
            }
            break;
        }
        return true;
    }

    // Take care of any messages that need input
    if (messageToPrint)
    {
        if (messageNeedsInput)
        {
            if (key != ' ' &&
                key != KEY_ESCAPE &&
                key != key_menu_confirm &&
                key != key_menu_abort)
            {
                return false;
            }
        }

        menuactive = messageLastMenuActive;
        messageToPrint = 0;

        if (messageRoutine)
            messageRoutine(key);

        menuactive = false;
        S_StartSound(NULL,sfx_swtchx);

        return true;
    }

    if ((devparm && key == key_menu_help) || (key != 0 && key == key_menu_screenshot))
    {
        G_ScreenShot ();
        return true;
    }

    // F-Keys
    if (!menuactive)
    {
//        if (key == key_menu_decscreen)
//        {
//            // Screen size down
//            if (automapactive || chat_on)
//                return false;
//            M_SizeDisplay(0);
//            S_StartSound(NULL,sfx_stnmov);
//            return true;
//        }
//        else if (key == key_menu_incscreen) // Screen size up
//        {
//            if (automapactive || chat_on)
//                return false;
//            M_SizeDisplay(1);
//            S_StartSound(NULL,sfx_stnmov);
//            return true;
//        }
        if (key == key_menu_help)
        {
            M_StartControlPanel ();

            if (gameversion >= exe_ultimate)
              currentMenu = &ReadDef2;
            else
              currentMenu = &ReadDef1;

            itemOn = 0;
            S_StartSound(NULL,sfx_swtchn);
            return true;
        }
        else if (key == key_menu_save)
        {
            M_StartControlPanel();
            S_StartSound(NULL,sfx_swtchn);
            M_SaveGame(0);
            return true;
        }
        else if (key == key_menu_load)
        {
            M_StartControlPanel();
            S_StartSound(NULL,sfx_swtchn);
            M_LoadGame(0);
            return true;
        }
        else if (key == key_menu_volume)   // Sound Volume
        {
                M_StartControlPanel ();
                currentMenu = &SoundDef;
                itemOn = sfx_vol;
                S_StartSound(NULL,sfx_swtchn);
                return true;
        }
//        else if (key == key_menu_detail)   // Detail toggle
//        {
//            // M_ChangeDetail(0);
//            S_StartSound(NULL,sfx_swtchn);
//            return true;
//        }
        else if (key == key_menu_qsave)
        {
            S_StartSound(NULL,sfx_swtchn);
            M_QuickSave();
            return true;
        }
        else if (key == key_menu_endgame)
        {
            S_StartSound(NULL,sfx_swtchn);
            M_EndGame(0);
            return true;
        }
        else if (key == key_menu_messages)
        {
            M_ToggleMessages(0);
            S_StartSound(NULL,sfx_swtchn);
            return true;
        }
        else if (key == key_menu_qload)
            {
            S_StartSound(NULL,sfx_swtchn);
            M_QuickLoad();
            return true;
        }
        else if (key == key_menu_quit)
        {
            S_StartSound(NULL,sfx_swtchn);
            M_QuitDOOM(0);
            return true;
        }
        else if (key == key_menu_gamma)
        {
            usegamma++;
            if (usegamma > 4)
                usegamma = 0;
            players[consoleplayer].message = DEH_String(gammamsg[usegamma]);
            I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));
            return true;
        }
    }

    // Pop-up menu?
    if (!menuactive)
    {
        if (key == key_menu_activate)
        {
            M_StartControlPanel ();
            S_StartSound(NULL,sfx_swtchn);
            return true;
        }
        return false;
    }

    // Keys usable within menu
    if (key == key_menu_down)
    {
        // Move down to next item
        do
        {
            itemOn = (itemOn+1 > currentMenu->numitems-1) ? 0 : itemOn+1;
            S_StartSound(NULL,sfx_pstop);
        } while(currentMenu->menuitems[itemOn].status==-1);
        return true;
    }
    else if (key == key_menu_up)
    {
        // Move back up to previous item
        do
        {
            itemOn = !itemOn ? currentMenu->numitems-1 : itemOn-1;
            S_StartSound(NULL, sfx_pstop);
        } while(currentMenu->menuitems[itemOn].status==-1);
            return true;
        }
    else if (key == key_menu_left)
    {
        // Slide slider left
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL,sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(0);
        }
        return true;
    }
    else if (key == key_menu_right)
    {
        // Slide slider right
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status == 2)
        {
            S_StartSound(NULL,sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(1);
        }
        return true;
    }
    else if (key == key_menu_forward)
    {
        // Activate menu item
        if (currentMenu->menuitems[itemOn].routine &&
            currentMenu->menuitems[itemOn].status)
        {
            currentMenu->lastOn = (short) itemOn;
            if (currentMenu->menuitems[itemOn].status == 2)
            {
                // right arrow
                currentMenu->menuitems[itemOn].routine(1);
                S_StartSound(NULL,sfx_stnmov);
            }
            else
            {
                currentMenu->menuitems[itemOn].routine(itemOn);
                S_StartSound(NULL,sfx_pistol);
            }
        }
        return true;
    }
    else if (key == key_menu_activate)
    {
        // Deactivate menu
        currentMenu->lastOn = (short) itemOn;
        M_ClearMenus ();
        S_StartSound(NULL,sfx_swtchx);
        return true;
    }
    else if (key == key_menu_back)
    {
        // Go back to previous menu
        currentMenu->lastOn = (short) itemOn;
        if (currentMenu->prevMenu)
        {
            currentMenu = currentMenu->prevMenu;
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL,sfx_swtchn);
        }
        return true;
    }
    else if (character_typed != 0 || IsNullKey(key))
    {
        // Keyboard shortcut?
        // Vanilla Doom has a weird behavior where it jumps to the scroll bars
        // when the certain keys are pressed, so emulate this.
        for (int i = itemOn+1; i < currentMenu->numitems; i++)
            {
                if (currentMenu->menuitems[i].alphaKey == character_typed)
                {
                    itemOn = i;
                    S_StartSound(NULL,sfx_pstop);
                    return true;
                }
            }
        for (int i = 0; i <= itemOn; i++)
        {
            if (currentMenu->menuitems[i].alphaKey == character_typed)
            {
                itemOn = i;
                S_StartSound(NULL,sfx_pstop);
                return true;
            }
        }
    }
    return false;
}


//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
        return;
    
    menuactive = 1;
    currentMenu = &MainDef;         // JDC
    itemOn = currentMenu->lastOn;   // JDC
}

// Display OPL debug messages - hack for GENMIDI development.
static void M_DrawOPLDev(void)
{
    char debug[1024];
    char *curr, *p;
    int line;

    I_OPL_DevMessages(debug, sizeof(debug));
    curr = debug;
    line = 0;

    for (;;)
    {
        p = strchr(curr, '\n');

        if (p != NULL)
            *p = '\0';

        M_WriteText(0, line * 8, curr);
        ++line;

        if (p == NULL)
            break;

        curr = p + 1;
    }
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static int   x, y;
    unsigned int start;
    char         string[80];
    const char   *name;

    inhelpscreens = false;
    
    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
        start = 0;
        y = SCREENHEIGHT/2 - M_StringHeight(messageString) / 2;
        while (messageString[start] != '\0')
        {
            boolean foundnewline = false;
            for (unsigned int i = 0; messageString[start + i] != '\0'; i++)
            {
                if (messageString[start + i] == '\n')
                {
                    M_StringCopy(
                            string,
                            messageString + start,
                            sizeof(string));

                    if (i < sizeof(string))
                        string[i] = '\0';

                    foundnewline = true;
                    start += i + 1;
                    break;
                }
            }

            if (!foundnewline)
            {
                M_StringCopy(string, messageString + start, sizeof(string));
                start += strlen(string);
            }

            x = SCREENWIDTH / 2 - M_StringWidth(string) / 2;
            M_WriteText(x, y, string);
            y += hu_font[0]->height;
        }
        return;
    }

    if (OPL_midi_debug)
        M_DrawOPLDev();

    if (!menuactive)
        return;

    if (currentMenu->routine)
        currentMenu->routine(); // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;


    for (int menu_item_idx = 0; menu_item_idx < currentMenu->numitems; menu_item_idx++)
    {
        name = DEH_String(currentMenu->menuitems[menu_item_idx].name);
        if (name[0] && W_CheckNumForName(name) > 0)
            V_DrawPatchDirect(x, y, W_CacheLumpName(name, PU_CACHE));
        y += line_height;
    }

    // DRAW SKULL
    V_DrawPatchDirect(
        x - 32,
        currentMenu->y - 5 + itemOn * line_height,
        W_CacheLumpName(DEH_String(skullIconNames[whichSkull]), PU_CACHE));
}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
    menuactive = 0;
}


//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker (void)
{
    if (--skullAnimCounter <= 0)
    {
        whichSkull ^= 1;
        skullAnimCounter = 8;
    }
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    screenSize = screenblocks - 3;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.
    // The same hacks were used in the original Doom EXEs.
    if (gameversion >= exe_ultimate)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    if (gameversion >= exe_final && gameversion <= exe_final2)
        ReadDef2.routine = M_DrawReadThisCommercial;

    if (gamemode == commercial)
    {
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8;
        NewDef.prevMenu = &MainDef;
        ReadDef1.routine = M_DrawReadThisCommercial;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[rdthsempty1].routine = M_FinishReadThis;
    }

    // Versions of doom.exe before the Ultimate Doom release only had
    // three episodes; if we're emulating one of those then don't try
    // to show episode four. If we are, then do show episode four
    // (should crash if missing).
    if (gameversion < exe_ultimate)
        EpiDef.numitems--;
    else if (gameversion == exe_chex)
        EpiDef.numitems = 1; // chex.exe shows only one episode.

    OPL_midi_debug = M_CheckParm("-OPL_midi_debug") > 0;
}

