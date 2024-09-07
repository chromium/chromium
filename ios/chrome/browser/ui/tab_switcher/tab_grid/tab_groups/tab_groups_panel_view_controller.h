// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_item_data_source.h"

@protocol TabGroupsPanelMutator;
@class TabGroupsPanelViewController;

// Protocol used to relay relevant user interactions from the Tab Groups panel
// in Tab Grid.
@protocol TabGroupsPanelViewControllerUIDelegate

// Tells the delegate that the scroll view scrolled.
- (void)tabGroupsPanelViewControllerDidScroll:
    (TabGroupsPanelViewController*)tabGroupsPanelViewController;

@end

// TabGroupsPanelViewController is the Tab Groups panel in Tab Grid.
@interface TabGroupsPanelViewController
    : UIViewController <TabGroupsPanelConsumer>

// Mutator is informed when the model should be updated after user interaction.
@property(nonatomic, weak) id<TabGroupsPanelMutator> mutator;

// Data source to query TabGroupsPanelItem properties, to configure cells.
@property(nonatomic, weak) id<TabGroupsPanelItemDataSource> itemDataSource;

// UI Delegate is informed of user interactions.
@property(nonatomic, weak) id<TabGroupsPanelViewControllerUIDelegate>
    UIDelegate;

// Whether the collection view is scrolled to the top.
@property(nonatomic, readonly, getter=isScrolledToTop) BOOL scrolledToTop;

// Whether the collection view is scrolled to the bottom.
@property(nonatomic, readonly, getter=isScrolledToBottom) BOOL scrolledToBottom;

// The insets to set on the collection view.
// Setting content insets on the collection view is a workaround. Indeed,
// ideally, grids would just honor the safe area. But since the 3 panes are part
// of a scrollview, they don't all get the correct information when being laid
// out. To that end, contentInsets are manually added.
@property(nonatomic, assign) UIEdgeInsets contentInsets;

// Updates the visible cells to make sure that the interval since creation is
// updated.
- (void)prepareForAppearance;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_VIEW_CONTROLLER_H_
