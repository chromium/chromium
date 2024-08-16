// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MUTATOR_H_

@class TabGroupsPanelItem;

// Mutator to communicate user actions on the Tab Groups panel in Tab Grid..
@protocol TabGroupsPanelMutator

// Tells the receiver that the user selected a tab group from the Tab Groups
// panel in Tab Grid.
- (void)selectTabGroupsPanelItem:(TabGroupsPanelItem*)item;

// Tells the receiver to close the group associated with `item`. `sourceView` is
// the view that the delete action originated from.
- (void)deleteTabGroupsPanelItem:(TabGroupsPanelItem*)item
                      sourceView:(UIView*)sourceView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MUTATOR_H_
