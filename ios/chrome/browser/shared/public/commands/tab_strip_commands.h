// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_

#import <UIKit/UIKit.h>

#import <set>

#import "base/memory/weak_ptr.h"

class TabGroup;
enum class TabGroupActionType;
@class TabGroupItem;
@class TabStripLastTabDraggedAlertCommand;
@class TabSwitcherItem;

namespace web {
class WebStateID;
}  // namespace web

// Commands for tab strip changes.
@protocol TabStripCommands

// Shows the tab group creation view.
- (void)showTabStripGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers;

// Shows tab group editing view.
- (void)showTabStripGroupEditionForGroup:
    (base::WeakPtr<const TabGroup>)tabGroup;

// Hides the tab group creation view.
- (void)hideTabStripGroupCreation;

// Shares `tabSwitcherItem`.
- (void)shareItem:(TabSwitcherItem*)tabSwitcherItem
       originView:(UIView*)originView;

// Shows an alert for moving the last tab of a group in this tab strip.
- (void)showAlertForLastTabDragged:(TabStripLastTabDraggedAlertCommand*)command;

// Displays a confirmation dialog anchoring to `sourceView` to confirm that
// selected `groupItem` is going to take an `actionType`.
- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                groupItem:(TabGroupItem*)tabGroupItem
                               sourceView:(UIView*)sourceView;

// Displays a snackbar after closing tab groups locally.
- (void)showTabStripTabGroupSnackbarAfterClosingGroups:
    (int)numberOfClosedGroups;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_
