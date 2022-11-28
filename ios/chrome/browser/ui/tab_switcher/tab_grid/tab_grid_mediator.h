// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_image_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_menu_actions_data_source.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_shareable_items_provider.h"

class Browser;
@protocol TabCollectionConsumer;
@class TabGridMediator;
@class URLWithTitle;

namespace sessions {
class TabRestoreService;
}  // namespace sessions

// Delegate protocol for an object that can handle the action sheet that asks
// for confirmation from the tab grid.
@protocol TabGridMediatorDelegate <NSObject>

- (void)
    showCloseItemsConfirmationActionSheetWithTabGridMediator:
        (TabGridMediator*)tabGridMediator
                                                       items:
                                                           (NSArray<NSString*>*)
                                                               items
                                                      anchor:(UIBarButtonItem*)
                                                                 buttonAnchor;

- (void)tabGridMediator:(TabGridMediator*)tabGridMediator
              shareURLs:(NSArray<URLWithTitle*>*)items
                 anchor:(UIBarButtonItem*)buttonAnchor;

// Dismissed presented popovers, if any.
- (void)dismissPopovers;

@end

// Mediates between model layer and tab grid UI layer.
@interface TabGridMediator : NSObject <GridCommands,
                                       GridImageDataSource,
                                       GridMenuActionsDataSource,
                                       GridShareableItemsProvider,
                                       TabCollectionDragDropHandler>

// The source browser.
@property(nonatomic, assign) Browser* browser;
// TabRestoreService holds the recently closed tabs.
@property(nonatomic, assign) sessions::TabRestoreService* tabRestoreService;
// Delegate to handle presenting the action sheet.
@property(nonatomic, weak) id<TabGridMediatorDelegate> delegate;

// Initializer with `consumer` as the receiver of model layer updates.
- (instancetype)initWithConsumer:(id<TabCollectionConsumer>)consumer
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Called when then tab grid is about to be shown.
- (void)prepareToShowTabGrid;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GRID_MEDIATOR_H_
