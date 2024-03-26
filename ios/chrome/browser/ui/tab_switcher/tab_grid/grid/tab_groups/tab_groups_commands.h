// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_

#import <set>

class TabGroup;
namespace web {
class WebStateID;
}  // namespace web

@protocol TabGroupsCommands

// Shows tab group UI for group `tabGroup`.
- (void)showTabGroup:(const TabGroup*)tabGroup;

// Hides the currently displayed tab group.
- (void)hideTabGroup;

// Shows the tab group creation view.
- (void)showTabGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers;

// Hides the tab group creation view.
- (void)hideTabGroupCreation;

// Shows tab group edition view.
- (void)showTabGroupEditionForGroup:(const TabGroup*)tabGroup;

// Show the current active tab.
- (void)showActiveTab;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_
