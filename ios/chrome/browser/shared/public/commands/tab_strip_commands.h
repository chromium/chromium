// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_

#import <Foundation/Foundation.h>

#import <set>

class Browser;
class TabGroup;
enum class TabGroupActionType;
@class TabGroupItem;
@class TabSwitcherItem;
namespace web {
class WebStateID;
}  // namespace web

// Commands for tab strip changes.
@protocol TabStripCommands

// Set the `iphHighlighted` state for the new tab button on the tab strip.
- (void)setNewTabButtonOnTabStripIPHHighlighted:(BOOL)IPHHighlighted;

// Shows the tab group creation view.
- (void)showTabStripGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers;

// Shows tab group editing view.
- (void)showTabStripGroupEditionForGroup:(const TabGroup*)tabGroup;

// Hides the tab group creation view.
- (void)hideTabStripGroupCreation;

// Shares `tabSwitcherItem`.
- (void)shareItem:(TabSwitcherItem*)tabSwitcherItem
       originView:(UIView*)originView;

// Shows an alert for moving `tabID` out of its `group`, with its
// `originBrowser` and `originIndex`.
- (void)showTabGroupDeletionAlertForTab:(web::WebStateID)tabID
                          originBrowser:(Browser*)browser
                            originIndex:(int)index
                            originGroup:(const TabGroup*)group;

// Displays a confirmation dialog anchoring to `sourceView` to confirm that
// selected `groupItem` is going to take an `actionType`.
- (void)showTabGroupConfirmationForAction:(TabGroupActionType)actionType
                                groupItem:(TabGroupItem*)tabGroupItem
                               sourceView:(UIView*)sourceView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_
