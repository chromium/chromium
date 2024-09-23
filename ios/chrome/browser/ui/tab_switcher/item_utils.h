// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_ITEM_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_ITEM_UTILS_H_

#import <Foundation/Foundation.h>

@class TabGroupItem;
@class TabSwitcherItem;

#ifdef __cplusplus
extern "C" {
#endif

// Returns a hash for a collection view item, using its identifier.
NSUInteger GetHashForTabSwitcherItem(TabSwitcherItem* tab_switcher_item);

// Returns a hash for a `TabGroupItem`.
// Based on NSValue's hashing of the TabGroup pointer.
NSUInteger GetHashForTabGroupItem(TabGroupItem* tab_group_item);

// Returns YES if `lhs` and `rhs` refer to the same tab.
BOOL CompareTabSwitcherItems(TabSwitcherItem* lhs, TabSwitcherItem* rhs);

// Returns YES if `lhs` and `rhs` refer to the same group.
BOOL CompareTabGroupItems(TabGroupItem* lhs, TabGroupItem* rhs);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_ITEM_UTILS_H_
