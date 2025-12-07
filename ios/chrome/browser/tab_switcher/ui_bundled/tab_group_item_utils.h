// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ITEM_UTILS_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ITEM_UTILS_H_

#import <Foundation/Foundation.h>

// Visual representation of a TabGroupItem's layout. This helps illustrate how
// the fetch logic applies.
//
// +--------------------------------+
// | [ FacePile ] [Title...]   [X]  |
// +--------------------------------+
// |+-------------+  +-------------+|
// ||             |  |             ||
// ||  Tab 1      |  |  Tab 2      ||
// || (Snap+Favi) |  | (Snap+Favi) ||
// ||             |  |             ||
// |+-------------+  +-------------+|
// |+-------------+  +-------------+|
// ||             |  |  F1   F2    ||
// ||  Tab 3      |  |  F3   +N    ||
// || (Snap+Favi) |  |  (Summary)  ||
// ||             |  |             ||
// |+-------------+  +-------------+|
// +--------------------------------+
//
// 'Snap+Favi' indicates a tile displaying both a snapshot and a favicon.
// 'F1', 'F2', 'F3' represent individual favicons displayed, and '+N' indicates
// a count of additional hidden tabs.
//
// Compact Height Mode:
// In 'compact_height' mode, the bottom row of tiles is hidden. Tab 2 transforms
// into the summary tile for remaining tabs, displaying favicons and '+N'.
// +--------------------------------+
// | [ FacePile ] [Title...]   [X]  |
// +--------------------------------+
// |+-------------+  +-------------+|
// ||             |  |  F1   F2    ||
// ||  Tab 1      |  |  F3   +N    ||
// || (Snap+Favi) |  |  (Summary)  ||
// ||             |  |             ||
// |+-------------+  +-------------+|
// +--------------------------------+

// The maximum number of individual tabs that are displayed in the TabGroupItem.
// Tabs beyond this count are  represented by "+N".
NSInteger MaxIndividualTabVisuals(BOOL compact_height);

// Calculates the number of individual tabs that will be visually represented
// within a TabGroupItem. This excludes tabs summarized by the "+N" indicator.
NSInteger TabGroupItemDisplayedVisualCount(NSInteger tab_count);

// Calculates total snapshot and favicon fetch requests count for a tab group.
NSInteger TabGroupItemFetchRequestsCount(NSInteger tab_count);

// Whether a snapshot should be fetched for `tab_index` within `tab_count`.
BOOL ShouldFetchSnapshotForTabInGroup(NSInteger tab_index, NSInteger tab_count);

// Determines whether the `tab_index` within `tab_count` will be represented in
// the summary tile.
// `compact_height` indicates if the bottom row of tiles is hidden.
BOOL IsTabOnSummaryTile(NSInteger tab_index,
                        NSInteger tab_count,
                        BOOL compact_height);

// Determines the relative favicon slot index within the summary tile for a
// given `tab_index` within `tab_count`.
// `compact_height` indicates if the bottom row of tiles is hidden.
NSInteger SummaryFaviconSlotForTabIndex(NSInteger tab_index,
                                        NSInteger tab_count,
                                        BOOL compact_height);

// Determines the count of additional hidden tabs.
// `compact_height` indicates if the bottom row of tiles is hidden.
NSInteger SummaryHiddenTabsCount(NSInteger tab_count, BOOL compact_height);

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ITEM_UTILS_H_
