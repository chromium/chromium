// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_MUTATOR_H_

@protocol TabGroupMutator

// Tells the receiver to insert a new item at the end of the group list. Return
// YES if it inserted an element, NO otherwise.
- (BOOL)addNewItemInGroup;

// Ungroups the current group (keeps the tab).
- (void)ungroup;

// Closes the tabs and deletes the current group.
- (void)closeGroup;

// Deletes the tabs and deletes the current group.
- (void)deleteGroup;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUP_MUTATOR_H_
