// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_ACTIONS_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_ACTIONS_DATA_SOURCE_H_

#import <Foundation/Foundation.h>

@class TabItem;

// Protocol that is used to pull the data required to execute the tab menus
// actions.
@protocol TabMenuActionsDataSource

// Asks the delegate for the TabItem object representing the tab cell with
// `identifier`.
- (TabItem*)tabItemForCellIdentifier:(NSString*)identifier;

// Asks the delegate if the tab `item` is already bookmarked.
- (BOOL)isTabItemBookmarked:(TabItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_CONTEXT_MENU_TAB_CONTEXT_MENU_ACTIONS_DATA_SOURCE_H_
