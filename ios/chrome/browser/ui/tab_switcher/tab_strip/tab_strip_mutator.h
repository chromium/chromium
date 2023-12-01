// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_MUTATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_MUTATOR_H_

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

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_TAB_STRIP_MUTATOR_H_
