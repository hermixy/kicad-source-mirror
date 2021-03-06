/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2014 CERN
 * @author Maciej Suminski <maciej.suminski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <boost/bind.hpp>

#include "pcb_editor_control.h"
#include "common_actions.h"
#include <tool/tool_manager.h>

#include "selection_tool.h"

#include <project.h>
#include <pcbnew_id.h>
#include <wxPcbStruct.h>
#include <class_board.h>
#include <class_zone.h>
#include <class_draw_panel_gal.h>
#include <class_module.h>
#include <class_mire.h>
#include <ratsnest_data.h>

#include <view/view_group.h>
#include <view/view_controls.h>


class ZONE_CONTEXT_MENU : public CONTEXT_MENU
{
public:
    ZONE_CONTEXT_MENU()
    {
        SetIcon( add_zone_xpm );
        Add( COMMON_ACTIONS::zoneFill );
        Add( COMMON_ACTIONS::zoneFillAll );
        Add( COMMON_ACTIONS::zoneUnfill );
        Add( COMMON_ACTIONS::zoneUnfillAll );
    }
};


PCB_EDITOR_CONTROL::PCB_EDITOR_CONTROL() :
    TOOL_INTERACTIVE( "pcbnew.EditorControl" ), m_frame( NULL )
{
}


void PCB_EDITOR_CONTROL::Reset( RESET_REASON aReason )
{
    m_frame = getEditFrame<PCB_EDIT_FRAME>();
}


bool PCB_EDITOR_CONTROL::Init()
{
    SELECTION_TOOL* selTool = m_toolMgr->GetTool<SELECTION_TOOL>();

    if( selTool )
    {
        selTool->GetMenu().AddMenu( new ZONE_CONTEXT_MENU, _( "Zones" ), false,
                                    SELECTION_CONDITIONS::OnlyType( PCB_ZONE_AREA_T ) );
    }

    return true;
}


// Track & via size control
int PCB_EDITOR_CONTROL::TrackWidthInc( const TOOL_EVENT& aEvent )
{
    BOARD* board = getModel<BOARD>();
    int widthIndex = board->GetDesignSettings().GetTrackWidthIndex() + 1;

    if( widthIndex >= (int) board->GetDesignSettings().m_TrackWidthList.size() )
        widthIndex = board->GetDesignSettings().m_TrackWidthList.size() - 1;

    board->GetDesignSettings().SetTrackWidthIndex( widthIndex );
    board->GetDesignSettings().UseCustomTrackViaSize( false );

    wxUpdateUIEvent dummy;
    m_frame->OnUpdateSelectTrackWidth( dummy );
    m_toolMgr->RunAction( COMMON_ACTIONS::trackViaSizeChanged );

    return 0;
}


int PCB_EDITOR_CONTROL::TrackWidthDec( const TOOL_EVENT& aEvent )
{
    BOARD* board = getModel<BOARD>();
    int widthIndex = board->GetDesignSettings().GetTrackWidthIndex() - 1;

    if( widthIndex < 0 )
        widthIndex = 0;

    board->GetDesignSettings().SetTrackWidthIndex( widthIndex );
    board->GetDesignSettings().UseCustomTrackViaSize( false );

    wxUpdateUIEvent dummy;
    m_frame->OnUpdateSelectTrackWidth( dummy );
    m_toolMgr->RunAction( COMMON_ACTIONS::trackViaSizeChanged );

    return 0;
}


int PCB_EDITOR_CONTROL::ViaSizeInc( const TOOL_EVENT& aEvent )
{
    BOARD* board = getModel<BOARD>();
    int sizeIndex = board->GetDesignSettings().GetViaSizeIndex() + 1;

    if( sizeIndex >= (int) board->GetDesignSettings().m_ViasDimensionsList.size() )
        sizeIndex = board->GetDesignSettings().m_ViasDimensionsList.size() - 1;

    board->GetDesignSettings().SetViaSizeIndex( sizeIndex );
    board->GetDesignSettings().UseCustomTrackViaSize( false );

    wxUpdateUIEvent dummy;
    m_frame->OnUpdateSelectViaSize( dummy );
    m_toolMgr->RunAction( COMMON_ACTIONS::trackViaSizeChanged );

    return 0;
}


int PCB_EDITOR_CONTROL::ViaSizeDec( const TOOL_EVENT& aEvent )
{
    BOARD* board = getModel<BOARD>();
    int sizeIndex = board->GetDesignSettings().GetViaSizeIndex() - 1;

    if( sizeIndex < 0 )
        sizeIndex = 0;

    board->GetDesignSettings().SetViaSizeIndex( sizeIndex );
    board->GetDesignSettings().UseCustomTrackViaSize( false );

    wxUpdateUIEvent dummy;
    m_frame->OnUpdateSelectViaSize( dummy );
    m_toolMgr->RunAction( COMMON_ACTIONS::trackViaSizeChanged );

    return 0;
}


int PCB_EDITOR_CONTROL::PlaceModule( const TOOL_EVENT& aEvent )
{
    MODULE* module = NULL;
    KIGFX::VIEW* view = getView();
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    BOARD* board = getModel<BOARD>();

    // Add a VIEW_GROUP that serves as a preview for the new item
    KIGFX::VIEW_GROUP preview( view );
    view->Add( &preview );

    m_toolMgr->RunAction( COMMON_ACTIONS::selectionClear, true );
    controls->ShowCursor( true );
    controls->SetSnapping( true );
    controls->SetAutoPan( true );
    controls->CaptureCursor( true );

    Activate();
    m_frame->SetToolID( ID_PCB_MODULE_BUTT, wxCURSOR_HAND, _( "Add module" ) );

    // Main loop: keep receiving events
    while( OPT_TOOL_EVENT evt = Wait() )
    {
        VECTOR2I cursorPos = controls->GetCursorPosition();

        if( evt->IsCancel() || evt->IsActivate() )
        {
            if( module )
            {
                board->Delete( module );  // it was added by LoadModuleFromLibrary()
                module = NULL;

                preview.Clear();
                preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
                controls->ShowCursor( true );
            }
            else
                break;

            if( evt->IsActivate() )  // now finish unconditionally
                break;
        }

        else if( module && evt->Category() == TC_COMMAND )
        {
            if( evt->IsAction( &COMMON_ACTIONS::rotate ) )
            {
                module->Rotate( module->GetPosition(), m_frame->GetRotationAngle() );
                preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
            }
            else if( evt->IsAction( &COMMON_ACTIONS::flip ) )
            {
                module->Flip( module->GetPosition() );
                preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
            }
        }

        else if( evt->IsClick( BUT_LEFT ) )
        {
            if( !module )
            {
                // Init the new item attributes
                module = m_frame->LoadModuleFromLibrary( wxEmptyString,
                                                         m_frame->Prj().PcbFootprintLibs(),
                                                         true, NULL );
                if( module == NULL )
                    continue;

                controls->ShowCursor( false );
                module->SetPosition( wxPoint( cursorPos.x, cursorPos.y ) );

                // Add all the drawable parts to preview
                preview.Add( module );
                module->RunOnChildren( boost::bind( &KIGFX::VIEW_GROUP::Add, &preview, _1 ) );

                preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
            }
            else
            {
                module->RunOnChildren( boost::bind( &KIGFX::VIEW::Add, view, _1 ) );
                view->Add( module );
                module->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );

                m_frame->OnModify();
                m_frame->SaveCopyInUndoList( module, UR_NEW );

                // Remove from preview
                preview.Remove( module );
                module->RunOnChildren( boost::bind( &KIGFX::VIEW_GROUP::Remove, &preview, _1 ) );
                module = NULL;  // to indicate that there is no module that we currently modify

                controls->ShowCursor( true );
            }
        }

        else if( module && evt->IsMotion() )
        {
            module->SetPosition( wxPoint( cursorPos.x, cursorPos.y ) );
            preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
        }
    }

    controls->ShowCursor( false );
    controls->SetSnapping( false );
    controls->SetAutoPan( false );
    controls->CaptureCursor( false );
    view->Remove( &preview );

    m_frame->SetToolID( ID_NO_TOOL_SELECTED, wxCURSOR_DEFAULT, wxEmptyString );

    return 0;
}


int PCB_EDITOR_CONTROL::PlaceTarget( const TOOL_EVENT& aEvent )
{
    KIGFX::VIEW* view = getView();
    KIGFX::VIEW_CONTROLS* controls = getViewControls();
    BOARD* board = getModel<BOARD>();
    PCB_TARGET* target = new PCB_TARGET( board );

    // Init the new item attributes
    target->SetLayer( Edge_Cuts );
    target->SetWidth( board->GetDesignSettings().m_EdgeSegmentWidth );
    target->SetSize( Millimeter2iu( 5 ) );
    VECTOR2I cursorPos = controls->GetCursorPosition();
    target->SetPosition( wxPoint( cursorPos.x, cursorPos.y ) );

    // Add a VIEW_GROUP that serves as a preview for the new item
    KIGFX::VIEW_GROUP preview( view );
    preview.Add( target );
    view->Add( &preview );
    preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );

    m_toolMgr->RunAction( COMMON_ACTIONS::selectionClear, true );
    controls->SetSnapping( true );
    controls->SetAutoPan( true );
    controls->CaptureCursor( true );

    Activate();
    m_frame->SetToolID( ID_PCB_MIRE_BUTT, wxCURSOR_PENCIL, _( "Add layer alignment target" ) );

    // Main loop: keep receiving events
    while( OPT_TOOL_EVENT evt = Wait() )
    {
        cursorPos = controls->GetCursorPosition();

        if( evt->IsCancel() || evt->IsActivate() )
            break;

        else if( evt->IsAction( &COMMON_ACTIONS::incWidth ) )
        {
            target->SetWidth( target->GetWidth() + WIDTH_STEP );
            preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
        }

        else if( evt->IsAction( &COMMON_ACTIONS::decWidth ) )
        {
            int width = target->GetWidth();

            if( width > WIDTH_STEP )
            {
                target->SetWidth( width - WIDTH_STEP );
                preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
            }
        }

        else if( evt->IsClick( BUT_LEFT ) )
        {
            assert( target->GetSize() > 0 );
            assert( target->GetWidth() > 0 );

            view->Add( target );
            board->Add( target );
            target->ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );

            m_frame->OnModify();
            m_frame->SaveCopyInUndoList( target, UR_NEW );

            preview.Remove( target );

            // Create next PCB_TARGET
            target = new PCB_TARGET( *target );
            preview.Add( target );
        }

        else if( evt->IsMotion() )
        {
            target->SetPosition( wxPoint( cursorPos.x, cursorPos.y ) );
            preview.ViewUpdate( KIGFX::VIEW_ITEM::GEOMETRY );
        }
    }

    delete target;

    controls->SetSnapping( false );
    controls->SetAutoPan( false );
    controls->CaptureCursor( false );
    view->Remove( &preview );

    m_frame->SetToolID( ID_NO_TOOL_SELECTED, wxCURSOR_DEFAULT, wxEmptyString );

    return 0;
}


// Zone actions
int PCB_EDITOR_CONTROL::ZoneFill( const TOOL_EVENT& aEvent )
{
    SELECTION_TOOL* selTool = m_toolMgr->GetTool<SELECTION_TOOL>();
    const SELECTION& selection = selTool->GetSelection();
    RN_DATA* ratsnest = getModel<BOARD>()->GetRatsnest();

    for( int i = 0; i < selection.Size(); ++i )
    {
        assert( selection.Item<BOARD_ITEM>( i )->Type() == PCB_ZONE_AREA_T );

        ZONE_CONTAINER* zone = selection.Item<ZONE_CONTAINER>( i );
        m_frame->Fill_Zone( zone );
        zone->SetIsFilled( true );
        ratsnest->Update( zone );
        zone->ViewUpdate();
    }

    ratsnest->Recalculate();

    return 0;
}


int PCB_EDITOR_CONTROL::ZoneFillAll( const TOOL_EVENT& aEvent )
{
    BOARD* board = getModel<BOARD>();
    RN_DATA* ratsnest = board->GetRatsnest();

    for( int i = 0; i < board->GetAreaCount(); ++i )
    {
        ZONE_CONTAINER* zone = board->GetArea( i );
        m_frame->Fill_Zone( zone );
        zone->SetIsFilled( true );
        ratsnest->Update( zone );
        zone->ViewUpdate();
    }

    ratsnest->Recalculate();

    return 0;
}


int PCB_EDITOR_CONTROL::ZoneUnfill( const TOOL_EVENT& aEvent )
{
    SELECTION_TOOL* selTool = m_toolMgr->GetTool<SELECTION_TOOL>();
    const SELECTION& selection = selTool->GetSelection();
    RN_DATA* ratsnest = getModel<BOARD>()->GetRatsnest();

    for( int i = 0; i < selection.Size(); ++i )
    {
        assert( selection.Item<BOARD_ITEM>( i )->Type() == PCB_ZONE_AREA_T );

        ZONE_CONTAINER* zone = selection.Item<ZONE_CONTAINER>( i );
        zone->SetIsFilled( false );
        zone->ClearFilledPolysList();
        ratsnest->Update( zone );
        zone->ViewUpdate();
    }

    ratsnest->Recalculate();

    return 0;
}


int PCB_EDITOR_CONTROL::ZoneUnfillAll( const TOOL_EVENT& aEvent )
{
    BOARD* board = getModel<BOARD>();
    RN_DATA* ratsnest = board->GetRatsnest();

    for( int i = 0; i < board->GetAreaCount(); ++i )
    {
        ZONE_CONTAINER* zone = board->GetArea( i );
        zone->SetIsFilled( false );
        zone->ClearFilledPolysList();
        ratsnest->Update( zone );
        zone->ViewUpdate();
    }

    ratsnest->Recalculate();

    return 0;
}


int PCB_EDITOR_CONTROL::SelectionCrossProbe( const TOOL_EVENT& aEvent )
{
    SELECTION_TOOL* selTool = m_toolMgr->GetTool<SELECTION_TOOL>();
    const SELECTION& selection = selTool->GetSelection();

    if( selection.Size() == 1 )
        m_frame->SendMessageToEESCHEMA( selection.Item<BOARD_ITEM>( 0 ) );

    return 0;
}


void PCB_EDITOR_CONTROL::SetTransitions()
{
    // Track & via size control
    Go( &PCB_EDITOR_CONTROL::TrackWidthInc,      COMMON_ACTIONS::trackWidthInc.MakeEvent() );
    Go( &PCB_EDITOR_CONTROL::TrackWidthDec,      COMMON_ACTIONS::trackWidthDec.MakeEvent() );
    Go( &PCB_EDITOR_CONTROL::ViaSizeInc,         COMMON_ACTIONS::viaSizeInc.MakeEvent() );
    Go( &PCB_EDITOR_CONTROL::ViaSizeDec,         COMMON_ACTIONS::viaSizeDec.MakeEvent() );

    // Zone actions
    Go( &PCB_EDITOR_CONTROL::ZoneFill,           COMMON_ACTIONS::zoneFill.MakeEvent() );
    Go( &PCB_EDITOR_CONTROL::ZoneFillAll,        COMMON_ACTIONS::zoneFillAll.MakeEvent() );
    Go( &PCB_EDITOR_CONTROL::ZoneUnfill,         COMMON_ACTIONS::zoneUnfill.MakeEvent() );
    Go( &PCB_EDITOR_CONTROL::ZoneUnfillAll,      COMMON_ACTIONS::zoneUnfillAll.MakeEvent() );

    // Placing tools
    Go( &PCB_EDITOR_CONTROL::PlaceTarget,        COMMON_ACTIONS::placeTarget.MakeEvent() );
    Go( &PCB_EDITOR_CONTROL::PlaceModule,        COMMON_ACTIONS::placeModule.MakeEvent() );

    Go( &PCB_EDITOR_CONTROL::SelectionCrossProbe, SELECTION_TOOL::SelectedEvent );
}


const int PCB_EDITOR_CONTROL::WIDTH_STEP = 100000;
