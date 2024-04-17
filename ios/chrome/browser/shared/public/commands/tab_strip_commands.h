// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_

#import <Foundation/Foundation.h>

#import <set>

class TabGroup;
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

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_TAB_STRIP_COMMANDS_H_
