// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_theme.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"

@class GroupGridCell;

// Informs the receiver of actions on the cell.
@protocol GroupGridCellDelegate
- (void)closeButtonTappedForGroupCell:(GroupGridCell*)cell;
@end

// A square-ish cell in a grid. Contains the group's favicon, its title and
// close button.
@interface GroupGridCell : TabCell
// Delegate to inform the grid of actions on the cell.
@property(nonatomic, weak) id<GroupGridCellDelegate> delegate;
// The look of the cell.
@property(nonatomic, assign) GridTheme theme;
// Settable UI elements of the group cell.
@property(nonatomic, copy) UIColor* groupColor;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) NSInteger tabsCount;
@property(nonatomic, assign) BOOL titleHidden;
// Sets to update and keep cell alpha in sync.
@property(nonatomic, assign) CGFloat opacity;
// The current state which the cell should display.
@property(nonatomic, assign) GridCellState state;

// Configures every tab of the group with a given snapshot/favicon pairs and
// passes the total tabs count to the bottomTrailingView.
- (void)configureWithGroupTabInfos:(NSArray<GroupTabInfo*>*)groupTabInfos
                    totalTabsCount:(NSInteger)totalTabsCount;

// Returns all tab views that compose this tab group view in the order they're
// presented.
- (NSArray<UIView*>*)allGroupTabViews;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GROUP_GRID_CELL_H_
