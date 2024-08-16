// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_

#import <Foundation/Foundation.h>

class TabGroup;

// Command protocol related to the Tab Grid.
@protocol TabGridCommands

// Brings `group` into view by making it part of the visible element of its
// grid.
- (void)bringGroupIntoView:(const TabGroup*)group animated:(BOOL)animated;

// Shows the history searching for `text`.
- (void)showHistoryForText:(NSString*)text;

// Shows a non-incognito web page searching for `text`.
- (void)showWebSearchForText:(NSString*)text;

// Shows the recent tabs panel searching for `text`.
- (void)showRecentTabsForText:(NSString*)text;

// Shows the tab groups panel.
- (void)showTabGroupsPanelAnimated:(BOOL)animated;

// Exits the tab grid, opening the selected tab of the current page (if
// relevant).
- (void)exitTabGrid;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GRID_COMMANDS_H_
