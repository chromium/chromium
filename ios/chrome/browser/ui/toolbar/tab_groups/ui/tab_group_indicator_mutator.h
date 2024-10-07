// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_MUTATOR_H_

// Mutator for actions happening in the TabGroupIndicatorView.
@protocol TabGroupIndicatorMutator

// Shows ShareKit UI.
- (void)showShareKitUI;

// Shows the tab group edit view.
- (void)showTabGroupEdition;

// Adds a new tab to the current group.
- (void)addNewTabInGroup;

// Closes the current group.
- (void)closeGroup;

// Ungroups the current group.
// If `confirmation` is true, shows a confirmation dialog.
- (void)unGroupWithConfirmation:(BOOL)confirmation;

// Deletes the current group.
// If `confirmation` is true, shows a confirmation dialog.
- (void)deleteGroupWithConfirmation:(BOOL)confirmation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TAB_GROUPS_UI_TAB_GROUP_INDICATOR_MUTATOR_H_
