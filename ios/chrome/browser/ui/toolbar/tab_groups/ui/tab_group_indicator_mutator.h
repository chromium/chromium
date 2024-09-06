// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_MUTATOR_H_

// Mutator for actions happening in the TabGroupIndicatorView.
@protocol TabGroupIndicatorMutator

// Shows the tab group edit view.
- (void)showTabGroupEdition;

// Adds a new tab to the current group.
- (void)addNewTabInGroup;

// Ungroups the current group.
- (void)unGroup;

// Closes the current group.
- (void)closeGroup;

// Deletes the current group.
- (void)deleteGroup;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_MUTATOR_H_
