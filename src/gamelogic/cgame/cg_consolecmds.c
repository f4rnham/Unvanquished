/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2009 Darklegion Development

This file is part of Daemon.

Daemon is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Daemon is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// cg_consolecmds.c -- text commands typed in at the local console, or
// executed by a key binding

#include "cg_local.h"

/*
=================
CG_SizeUp_f

Keybinding command
=================
*/
static void CG_SizeUp_f( void )
{
	trap_Cvar_Set( "cg_viewsize", va( "%i", MIN( cg_viewsize.integer + 10, 100 ) ) );
}

/*
=================
CG_SizeDown_f

Keybinding command
=================
*/
static void CG_SizeDown_f( void )
{
	trap_Cvar_Set( "cg_viewsize", va( "%i", MAX( cg_viewsize.integer - 10, 30 ) ) );
}

/*
=============
CG_Viewpos_f

Debugging command to print the current position
=============
*/
static void CG_Viewpos_f( void )
{
	CG_Printf( "(%i %i %i) : %i\n", ( int ) cg.refdef.vieworg[ 0 ],
	           ( int ) cg.refdef.vieworg[ 1 ], ( int ) cg.refdef.vieworg[ 2 ],
	           ( int ) cg.refdefViewAngles[ YAW ] );
}

void CG_RequestScores( void )
{
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score\n" );
}

void CG_ClientList_f( void )
{
	clientInfo_t *ci;
	int          i;
	int          count = 0;

	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
		ci = &cgs.clientinfo[ i ];

		if ( !ci->infoValid )
		{
			continue;
		}

		switch ( ci->team )
		{
			case TEAM_ALIENS:
				Com_Printf( "%2d " S_COLOR_RED "A   " S_COLOR_WHITE "%s\n", i,
				            ci->name );
				break;

			case TEAM_HUMANS:
				Com_Printf( "%2d " S_COLOR_CYAN "H   " S_COLOR_WHITE "%s\n", i,
				            ci->name );
				break;

			default:
			case TEAM_NONE:
			case NUM_TEAMS:
				Com_Printf( "%2d S   %s\n", i, ci->name );
				break;
		}

		count++;
	}

	Com_Printf(_( "Listed %2d clients\n"), count ); // FIXME PLURAL
}

static void CG_ReloadHud_f( void )
{
	CG_Rocket_LoadHuds();
	CG_OnPlayerWeaponChange( (weapon_t) cg.snap->ps.weapon );
}

static void CG_CompleteClass( void )
{
	int i = 0;

	if ( cgs.clientinfo[ cg.clientNum ].team == TEAM_ALIENS )
	{
		// TODO: Add iterator for alien/human classes
		for ( i = PCL_ALIEN_BUILDER0; i < PCL_HUMAN_NAKED; i++ )
		{
			trap_CompleteCallback( BG_Class( i )->name );
		}
	}
	else if ( cgs.clientinfo[ cg.clientNum ].team == TEAM_HUMANS )
	{
		trap_CompleteCallback( BG_Weapon( WP_HBUILD )->name );
		trap_CompleteCallback( BG_Weapon( WP_MACHINEGUN )->name );
	}
}

static void CG_CompleteBuy_internal( qboolean negatives )
{
	int i;

	for( i = 0; i < UP_NUM_UPGRADES; i++ )
	{
		const upgradeAttributes_t *item = BG_Upgrade( i );
		if ( item->purchasable && item->team == TEAM_HUMANS )
		{
			trap_CompleteCallback( item->name );

			if ( negatives )
			{
				trap_CompleteCallback( va( "-%s", item->name ) );
			}
		}
	}

	trap_CompleteCallback( "grenade" ); // called "gren" elsewhere, so special-case it

	if ( negatives )
	{
		trap_CompleteCallback( "-grenade" );

		i = BG_GetPlayerWeapon( &cg.snap->ps );

	}

	for( i = 0; i < WP_NUM_WEAPONS; i++ )
	{
		const weaponAttributes_t *item = BG_Weapon( i );
		if ( item->purchasable && item->team == TEAM_HUMANS )
		{
			trap_CompleteCallback( item->name );

			if ( negatives )
			{
				trap_CompleteCallback( va( "-%s", BG_Weapon( i )->name ) );
			}
		}
	}
}

static void CG_CompleteBuy( void )
{
	if( cgs.clientinfo[ cg.clientNum ].team != TEAM_HUMANS )
	{
		return;
	}

	trap_CompleteCallback( "-all" );
	trap_CompleteCallback( "-weapons" );
	trap_CompleteCallback( "-upgrades" );
	CG_CompleteBuy_internal( qtrue );
}

static void CG_CompleteSell( void )
{
	if( cgs.clientinfo[ cg.clientNum ].team != TEAM_HUMANS )
	{
		return;
	}

	trap_CompleteCallback( "all" );
	trap_CompleteCallback( "weapons" );
	trap_CompleteCallback( "upgrades" );
	CG_CompleteBuy_internal( qfalse );
}


static void CG_CompleteBeacon( void )
{
	int i;

	for ( i = BCT_NONE + 1; i < NUM_BEACON_TYPES; i++ )
	{
		const beaconAttributes_t *item = BG_Beacon( i );
		if ( !( item->flags & BCF_RESERVED ) )
		{
			trap_CompleteCallback( item->name );
		}
	}
}

static void CG_CompleteBuild( void )
{
	int i;

	for ( i = 0; i < BA_NUM_BUILDABLES; i++ )
	{
		const buildableAttributes_t *item = BG_Buildable( i );
		if ( item->team == cgs.clientinfo[ cg.clientNum ].team )
		{
			trap_CompleteCallback( item->name );
		}
	}
}

static void CG_CompleteName( void )
{
	int           i;
	clientInfo_t *ci;

	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
		char name[ MAX_NAME_LENGTH ];
		ci = &cgs.clientinfo[ i ];
		strcpy( name, ci->name );

		if ( !ci->infoValid )
		{
			continue;
		}

		trap_CompleteCallback( Q_CleanStr( name ) );
	}
}

static void CG_CompleteVsay( void )
{
	voice_t     *voice = cgs.voices;
	voiceCmd_t  *voiceCmd = voice->cmds;

	while ( voiceCmd != NULL )
	{
		trap_CompleteCallback( voiceCmd->cmd );
		voiceCmd = voiceCmd->next;
	}
}

static void CG_CompleteGive( void )
{
	unsigned               i = 0;
	static const char give[][ 12 ] =
	{
		"all", "health", "funds", "stamina", "poison", "fuel", "ammo", "momentum", "bp"
	};

	for( i = 0; i < ARRAY_LEN( give ); i++ )
	{
		trap_CompleteCallback( give[i] );
	}
}

static void CG_CompleteTeamVote( void )
{
	unsigned           i = 0;
	static const char vote[][ 16 ] =
	{
		"kick", "spectate", "denybuild", "allowbuild", "admitdefeat", "poll"
	};

	for( i = 0; i < ARRAY_LEN( vote ); i++ )
	{
		trap_CompleteCallback( vote[i] );
	}
}
static void CG_CompleteVote( void )
{
	unsigned           i = 0;
	static const char vote[][ 16 ] =
	{
		"kick", "spectate", "mute", "unmute", "sudden_death", "extend",
		"draw", "map_restart", "map", "layout", "nextmap", "poll"
	};

	for( i = 0; i < ARRAY_LEN( vote ); i++ )
	{
		trap_CompleteCallback( vote[i] );
	}
}

static void CG_CompleteItem( void )
{
	int i = 0;

	if( cgs.clientinfo[ cg.clientNum ].team == TEAM_ALIENS )
	{
		return;
	}

	trap_CompleteCallback( "weapon" );

	for( i = 0; i < UP_NUM_UPGRADES; i++ )
	{
		const upgradeAttributes_t *item = BG_Upgrade( i );
		if ( item->usable )
		{
			trap_CompleteCallback( item->name );
		}
	}

	for( i = 0; i < WP_NUM_WEAPONS; i++ )
	{
		const weaponAttributes_t *item = BG_Weapon( i );
		if( item->team == TEAM_HUMANS )
		{
			trap_CompleteCallback( item->name );
		}
	}
}

static void CG_TestCGrade_f( void )
{
	qhandle_t shader = trap_R_RegisterShader(CG_Argv(1),
						 (RegisterShaderFlags_t) ( RSF_NOMIP | RSF_NOLIGHTSCALE ) );

	// override shader 0
	cgs.gameGradingTextures[ 0 ] = shader;
	cgs.gameGradingModels[ 0 ] = -1;
}

static void CG_MessageAdmin_f( void )
{
	cg.sayType = SAY_TYPE_ADMIN;
	trap_Rocket_DocumentAction( rocketInfo.menu[ ROCKETMENU_CHAT ].id, "show" );
}

static void CG_MessageCommand_f( void )
{
	cg.sayType = SAY_TYPE_COMMAND;
	trap_Rocket_DocumentAction( rocketInfo.menu[ ROCKETMENU_CHAT ].id, "show" );
}

static void CG_MessageTeam_f( void )
{
	cg.sayType = SAY_TYPE_TEAM;
	trap_Rocket_DocumentAction( rocketInfo.menu[ ROCKETMENU_CHAT ].id, "show" );
}

static void CG_MessagePublic_f( void )
{
	cg.sayType = SAY_TYPE_PUBLIC;
	trap_Rocket_DocumentAction( rocketInfo.menu[ ROCKETMENU_CHAT ].id, "show" );
}

static void CG_ToggleMenu_f( void )
{
	trap_Rocket_DocumentAction( rocketInfo.menu[ ROCKETMENU_INGAME_MENU ].id, "show" );
}

// FIXME: Don't hardcode scoreboard ID
void CG_ShowScores_f( void )
{
	if ( cg.intermissionStarted )
	{
		return;
	}

	if ( !cg.showScores )
	{
		CG_RequestScores();
		trap_PrepareKeyUp();

		trap_Rocket_ShowScoreboard( "scoreboard", qtrue );
		cg.showScores = qtrue;
	}
	else
	{
		cg.showScores = qfalse;
	}
}

void CG_HideScores_f( void )
{
	if ( cg.intermissionStarted )
	{
		return;
	}

	trap_Rocket_ShowScoreboard( "scoreboard", qfalse );
	cg.showScores = qfalse;
}

void CG_BeaconMenu_f( void )
{
	trap_Rocket_DocumentAction( rocketInfo.menu[ ROCKETMENU_BEACONS ].id, "show" );
}

static const struct
{
	const char *cmd;
	void ( *function )( void );
	void ( *completer )( void );
} commands[] =
{
	{ "+scores",           CG_ShowScores_f,        0                },
	{ "-scores",           CG_HideScores_f,        0                },
	{ "beacon",           0,                       CG_CompleteBeacon },
	{ "beaconMenu",       CG_BeaconMenu_f,         0                },
	{ "build",            0,                       CG_CompleteBuild },
	{ "buy",              0,                       CG_CompleteBuy   },
	{ "callteamvote",     0,                       CG_CompleteTeamVote },
	{ "callvote",         0,                       CG_CompleteVote  },
	{ "cgame_memory",     BG_MemoryInfo,           0                },
	{ "class",            0,                       CG_CompleteClass },
	{ "clientlist",       CG_ClientList_f,         0                },
	{ "deconstruct",      0,                       0                },
	{ "destroy",          0,                       0                },
	{ "destroyTestPS",    CG_DestroyTestPS_f,      0                },
	{ "destroyTestTS",    CG_DestroyTestTS_f,      0                },
	{ "follow",           0,                       CG_CompleteName  },
	{ "follownext",       0,                       0                },
	{ "followprev",       0,                       0                },
	{ "give",             0,                       CG_CompleteGive  },
	{ "god",              0,                       0                },
	{ "ignite",           0,                       0                },
	{ "ignore",           0,                       CG_CompleteName  },
	{ "itemact",          0,                       CG_CompleteItem  },
	{ "itemdeact",        0,                       CG_CompleteItem  },
	{ "itemtoggle",       0,                       CG_CompleteItem  },
	{ "kill",             0,                       0                },
	{ "lcp",              CG_CenterPrint_f,        0                },
	{ "m",                0,                       CG_CompleteName  },
	{ "message_admin",    CG_MessageAdmin_f,       0                },
	{ "message_command",  CG_MessageCommand_f,     0                },
	{ "message_public",   CG_MessagePublic_f,      0                },
	{ "message_team",     CG_MessageTeam_f,        0                },
	{ "mt",               0,                       CG_CompleteName  },
	{ "nextframe",        CG_TestModelNextFrame_f, 0                },
	{ "nextskin",         CG_TestModelNextSkin_f,  0                },
	{ "noclip",           0,                       0                },
	{ "notarget",         0,                       0                },
	{ "prevframe",        CG_TestModelPrevFrame_f, 0                },
	{ "prevskin",         CG_TestModelPrevSkin_f,  0                },
	{ "reload",           0,                       0                },
	{ "reloadHud",        CG_ReloadHud_f,          0                },
	{ "say",              0,                       0                },
	{ "say_team",         0,                       0                },
	{ "sell",             0,                       CG_CompleteSell  },
	{ "setviewpos",       0,                       0                },
	{ "showScores",       CG_ShowScores_f,         0                },
	{ "sizedown",         CG_SizeDown_f,           0                },
	{ "sizeup",           CG_SizeUp_f,             0                },
	{ "team",             0,                       0                },
	{ "teamvote",         0,                       0                },
	{ "testcgrade",       CG_TestCGrade_f,         0                },
	{ "testgun",          CG_TestGun_f,            0                },
	{ "testmodel",        CG_TestModel_f,          0                },
	{ "testPS",           CG_TestPS_f,             0                },
	{ "testTS",           CG_TestTS_f,             0                },
	{ "toggleMenu",       CG_ToggleMenu_f,         0                },
	{ "unignore",         0,                       CG_CompleteName  },
	{ "viewpos",          CG_Viewpos_f,            0                },
	{ "vote",             0,                       0                },
	{ "vsay",             0,                       CG_CompleteVsay  },
	{ "vsay_local",       0,                       CG_CompleteVsay  },
	{ "vsay_team",        0,                       CG_CompleteVsay  },
	{ "weapnext",         CG_NextWeapon_f,         0                },
	{ "weapon",           CG_Weapon_f,             0                },
	{ "weapprev",         CG_PrevWeapon_f,         0                },
	{ "where",            0,                       0                }
};

/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand( void )
{
	char buffer[BIG_INFO_STRING];
	consoleCommand_t *cmd;

	cmd = (consoleCommand_t*) bsearch( CG_Argv( 0 ), commands,
	               ARRAY_LEN( commands ), sizeof( commands[ 0 ] ),
	               cmdcmp );

	if ( !cmd || !cmd->function )
	{
		//This command was added to provide completion of server-side commands
		//forward it to the server
		// (see also CG_ServerCommands)
		trap_EscapedArgs( buffer, sizeof ( buffer ) );
		trap_SendClientCommand( buffer );
	}
	else
	{
		cmd->function();
	}
	return qtrue;
}

/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands( void )
{
	unsigned i;

	for ( i = 0; i < ARRAY_LEN( commands ); i++ )
	{
		//Check that the commands are in increasing order so that it can be used by bsearch
		if ( i != 0 && Q_stricmp(commands[i-1].cmd, commands[i].cmd) > 0 )
		{
			CG_Printf( "CGame command list is in the wrong order for %s and %s\n", commands[i - 1].cmd, commands[i].cmd );
		}
		trap_AddCommand( commands[ i ].cmd );
	}

	trap_RegisterButtonCommands(
	    // 0      12       3     45      6        78       9ABCDEF      <- bit nos.
	      "attack,,useitem,taunt,,sprint,activate,,attack2,,,,,,rally"
	    );
}

/*
=================
CG_CompleteCommand

The command has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/

void CG_CompleteCommand( int argNum )
{
	const char *cmd;
	unsigned i;

	Q_UNUSED(argNum);

	cmd = CG_Argv( 0 );

	while ( *cmd == '\\' || *cmd == '/' )
	{
		cmd++;
	}

	for ( i = 0; i < ARRAY_LEN( commands ); i++ )
	{
		if ( !Q_stricmp( cmd, commands[ i ].cmd ) && commands[ i ].completer )
		{
			commands[ i ].completer();
			return;
		}
	}
}
