// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator_items_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_shareable_items_provider.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_view_controller_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_page_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_grid_delegate.h"

class Browser;
@protocol GridConsumer;
@protocol GridMediatorDelegate;
@protocol GridToolbarsConfigurationProvider;
@protocol GridToolbarsMutator;
@protocol TabCollectionConsumer;
@class TabGridToolbarsConfiguration;
@protocol TabGridToolbarsMainTabGridDelegate;
@protocol TabGroupsCommands;
@protocol TabPresentationDelegate;
class WebStateList;

// Mediates between model layer and tab grid UI layer.
@interface BaseGridMediator : NSObject <BaseGridMediatorItemProvider,
                                        GridCommands,
                                        GridViewControllerMutator,
                                        TabCollectionDragDropHandler,
                                        TabGridPageMutator,
                                        TabGridToolbarsGridDelegate>

// The source browser.
@property(nonatomic, assign) Browser* browser;
// The UI consumer to which updates are made.
@property(nonatomic, weak) id<TabCollectionConsumer> consumer;
// Delegate to handle presenting the action sheet.
@property(nonatomic, weak) id<GridMediatorDelegate> delegate;
// Mutator to handle toolbars modification.
@property(nonatomic, weak) id<GridToolbarsMutator> toolbarsMutator;
// The list from the browser.
@property(nonatomic, assign) WebStateList* webStateList;
// Contained grid which provides tab grid toolbar configuration.
@property(nonatomic, weak) id<GridToolbarsConfigurationProvider>
    containedGridToolbarsProvider;
// Action handler for the actions related to the tab grid .
@property(nonatomic, weak) id<TabGridToolbarsMainTabGridDelegate>
    toolbarTabGridDelegate;
// Grid consumer.
@property(nonatomic, weak) id<GridConsumer> gridConsumer;
// Delegate to handle presenting tab UI.
@property(nonatomic, weak) id<TabPresentationDelegate> tabPresentationDelegate;
// Tab Groups Dispatcher.
@property(nonatomic, weak) id<TabGroupsCommands> dispatcher;
@end

@interface BaseGridMediator (Subclassing)

// Current mode.
@property(nonatomic, assign) TabGridMode currentMode;

// Disconnects the mediator.
- (void)disconnect NS_REQUIRES_SUPER;

// Called when toolbars should be updated. This function should be implemented
// in a subclass.
- (void)configureToolbarsButtons;

// Called when the buttons needs to be updated for the selection mode.
- (void)configureButtonsInSelectionMode:
    (TabGridToolbarsConfiguration*)configuration;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_MEDIATOR_H_
