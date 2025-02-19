// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MUTATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MUTATOR_H_

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

// Tells the receiver to leave the shared group associated with `item`.
// `sourceView` is the view that the delete action originated from.
- (void)leaveSharedTabGroupsPanelItem:(TabGroupsPanelItem*)item
                           sourceView:(UIView*)sourceView;

// Tells the receiver to delete the shared group associated with `item`.
// `sourceView` is the view that the delete action originated from.
- (void)deleteSharedTabGroupsPanelItem:(TabGroupsPanelItem*)item
                            sourceView:(UIView*)sourceView;

// Tells the receiver to remove the notifications associated with `item`.
- (void)deleteNotificationItem:(TabGroupsPanelItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_MUTATOR_H_
