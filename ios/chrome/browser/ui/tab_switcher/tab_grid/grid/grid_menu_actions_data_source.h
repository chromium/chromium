// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MENU_ACTIONS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MENU_ACTIONS_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

@class GridItem;

// Protocol that is used to pull the data required to execute the grid menus
// actions.
@protocol GridMenuActionsDataSource

// Asks the delegate for the GridItem object representing the grid cell with
// `identifier`.
- (GridItem*)gridItemForCellIdentifier:(NSString*)identifier;

// Asks the delegate if the grid `item` is already bookmarked.
- (BOOL)isGridItemBookmarked:(GridItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_MENU_ACTIONS_DATA_SOURCE_H_
