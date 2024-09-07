// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GROUPS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GROUPS_COMMANDS_H_

#import <set>

#import "base/memory/weak_ptr.h"

class TabGroup;
enum class TabGroupActionType;
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
- (void)hideTabGroupCreationAnimated:(BOOL)animated;

// Shows tab group edition view.
- (void)showTabGroupEditionForGroup:(const TabGroup*)tabGroup;

// Show the current active tab.
- (void)showActiveTab;

// Displays a confirmation dialog anchoring to `sourceView` on iPad or at the
// bottom on iPhone to confirm that selected `group` is going to take an
// `actionType`.
- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                    group:
                                        (base::WeakPtr<const TabGroup>)tabGroup
                               sourceView:(UIView*)sourceView;

// Displays a confirmation dialog anchoring to `sourceButtonItem` on iPad or at
// the bottom on iPhone to confirm that selected `group` is going to take an
// `actionType`.
- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                    group:
                                        (base::WeakPtr<const TabGroup>)tabGroup
                         sourceButtonItem:(UIBarButtonItem*)sourceButtonItem;

// Displays a snackbar after closing tab groups locally.
- (void)showTabGridTabGroupSnackbarAfterClosingGroups:(int)numberOfClosedGroups;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_GROUPS_COMMANDS_H_
