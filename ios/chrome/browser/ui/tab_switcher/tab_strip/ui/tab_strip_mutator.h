// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_MUTATOR_H_

@class TabGroupItem;
@class TabSwitcherItem;

/// Protocol that the tabstrip UI uses to update the model.
@protocol TabStripMutator

/// Tells the receiver to insert a new item in the tabstrip.
- (void)addNewItem;

/// Tells the receiver to activate the `item`.
- (void)activateItem:(TabSwitcherItem*)item;

/// Tells the receiver to close the `item`.
- (void)closeItem:(TabSwitcherItem*)item;

/// Tells the receiver to close all items except `item`.
- (void)closeAllItemsExcept:(TabSwitcherItem*)item;

/// Tells the receiver to create a new group containing `item`.
- (void)createNewGroupWithItem:(TabSwitcherItem*)item;

/// Tells the receiver to collapse the group associated with `tabGroupItem`.
- (void)collapseGroup:(TabGroupItem*)tabGroupItem;

/// Tells the receiver to expand the group associated with `tabGroupItem`.
- (void)expandGroup:(TabGroupItem*)tabGroupItem;

/// Tells the receiver to rename the group associated with `tabGroupItem`.
- (void)renameGroup:(TabGroupItem*)tabGroupItem;

/// Tells the receiver to add a new tab in the group associated with
/// `tabGroupItem`.
- (void)addNewTabInGroup:(TabGroupItem*)tabGroupItem
    NS_SWIFT_NAME(addNewTabInGroup(_:));

/// Tells the receiver to ungroup the tabs in the group associated with
/// `tabGroupItem`.
- (void)ungroupGroup:(TabGroupItem*)tabGroupItem;

/// Tells the receiver to delete the group associated with `tabGroupItem`.
- (void)deleteGroup:(TabGroupItem*)tabGroupItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_MUTATOR_H_
