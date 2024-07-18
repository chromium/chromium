// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_

#import <set>

class TabGroup;
namespace web {
class WebStateID;
}  // namespace web

// Enum to represent an action that a tab group is going to take.
enum class TabGroupActionType {
  kUngroupTabGroup,
  kDeleteTabGroup,
};

@protocol TabGroupsCommands

// Shows tab group UI for group `tabGroup`.
- (void)showTabGroup:(const TabGroup*)tabGroup;

// Hides the currently displayed tab group.
- (void)hideTabGroup;

// Shows the tab group creation view.
- (void)showTabGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers;

// Hides the tab group creation view.
- (void)hideTabGroupCreationAnimated:(BOOL)animated;

// Shows tab group edition view.
- (void)showTabGroupEditionForGroup:(const TabGroup*)tabGroup;

// Show the current active tab.
- (void)showActiveTab;

// Displays an action sheet at `sourceView` on iPad or at the bottom on iPhone
// to confirm that selected `group` is going to take an `actionType`.
- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                    group:(const TabGroup*)tabGroup
                               sourceView:(UIView*)sourceView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_
