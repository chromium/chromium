// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_

#import <set>

namespace web {
class WebStateID;
}  // namespace web

@protocol TabGroupsCommands

// Shows tab group UI for group ID `groupID`.
// TODO(crbug.com/1501837): Add the group ID in parameter when group ID will be
// available.
- (void)showTabGroupWithID;

// Hides the currently displayed tab group.
- (void)hideTabGroup;

// Shows the tab group creation view.
- (void)showTabGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers;

// Hides the tab group creation view.
- (void)hideTabGroupCreation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_COMMANDS_H_
