// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_SELECTED_GRID_ITEMS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_SELECTED_GRID_ITEMS_H_

#import <Foundation/Foundation.h>
#import <set>

@class GridItemIdentifier;
@class URLWithTitle;

class WebStateList;

namespace web {
class WebStateID;
}  // namespace web

// This object holds a set of identifiers that represent a selected tab or group
// in the grid, and a counter to track the number of selected tabs (regular tabs
// and tabs in groups) in the grid.
@interface SelectedGridItems : NSObject

// The items in the grid (groups and tabs).
@property(nonatomic, strong, readonly)
    NSSet<GridItemIdentifier*>* itemsIdentifiers;

// Number of tabs in the grid (regular tab cell and number of tabs in a group
// cell).
@property(nonatomic, readonly) NSUInteger tabsCount;

// The count of shareable items.
@property(nonatomic, readonly) NSUInteger sharableTabsCount;

// The designated initializer with a non nil `webStateList`.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds a `GridItemIdentifier` to `itemsIdentifiers`.
- (void)addItem:(GridItemIdentifier*)item;

// Removes a `GridItemIdentifier` from `itemsIdentifiers`.
- (void)removeItem:(GridItemIdentifier*)item;

// Removes all the items in `itemsIdentifiers`.
- (void)removeAllItems;

// Checks whether a `GridItemIdentifier` is in `itemsIdentifiers`
- (BOOL)containItem:(GridItemIdentifier*)item;

// Returns a const set of shareable tabs.
- (const std::set<web::WebStateID>&)sharableTabs;

// Computes and return the list of all selected tabs.
- (std::set<web::WebStateID>)allTabs;

// Returns the URLs of all the selected tabs (regular tabs and tabs in groups)
// in the grid.
- (NSArray<URLWithTitle*>*)selectedTabsURLs;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_SELECTED_GRID_ITEMS_H_
