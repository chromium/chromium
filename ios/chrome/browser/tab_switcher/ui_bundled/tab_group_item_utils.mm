// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item_utils.h"

#import "base/check_op.h"

// The maximum number of individual tabs that are displayed in regular height.
const NSInteger kMaxIndividualTabVisuals = 7;
// The maximum number of individual tabs that are displayed in compact height.
const NSInteger kMaxIndividualTabVisualsCompactHeight = 5;

NSInteger MaxIndividualTabVisuals(BOOL compact_height) {
  return compact_height ? kMaxIndividualTabVisualsCompactHeight
                        : kMaxIndividualTabVisuals;
}

NSInteger TabGroupItemDisplayedVisualCount(NSInteger tab_count) {
  if (tab_count > kMaxIndividualTabVisuals) {
    return kMaxIndividualTabVisuals - 1;
  }
  return tab_count;
}

NSInteger TabGroupItemFetchRequestsCount(NSInteger tab_count) {
  if (tab_count <= 4) {
    // For <= 4 tabs: Fetch favicon + snapshot (2 requests) for every tab.
    return tab_count * 2;
  }
  if (tab_count <= kMaxIndividualTabVisuals) {
    // For <= 7 tabs: First 3 tabs get favicon + snapshot (6 requests total).
    // Remaining tabs get only favicon (1 request each).
    return 6 + (tab_count - 3);
  }
  // For > 7 tabs: First 3 tabs get favicon + snapshot (6 requests total).
  // Next 3 tabs get only favicon (3 requests total), reserving a slot for
  // count.
  return 9;
}

BOOL ShouldFetchSnapshotForTabInGroup(NSInteger tab_index,
                                      NSInteger tab_count) {
  if (tab_count <= 4) {
    CHECK_LT(tab_index, 4);
    // For groups with 1 to 4 tabs: Each tab  maps directly to its own
    // corresponding tile index.
    return tab_index < tab_count;
  }
  // For groups with 5 or more tabs, a snapshot is only fetched for the first 3
  // tabs.
  return tab_index < 3;
}

//  Determines whether the tab at `tab_index` will be represented by a slot
//  within the combined summary tile
BOOL IsTabOnSummaryTile(NSInteger tab_index,
                        NSInteger tab_count,
                        BOOL compact_height) {
  if (compact_height) {
    return tab_count > 2 && tab_index >= 1;
  }
  return tab_count > 4 && tab_index >= 3;
}

NSInteger SummaryFaviconSlotForTabIndex(NSInteger tab_index,
                                        NSInteger tab_count,
                                        BOOL compact_height) {
  if (compact_height) {
    return tab_index - 1;
  }
  return tab_index - 3;
}

NSInteger SummaryHiddenTabsCount(NSInteger tab_count, BOOL compact_height) {
  if (compact_height) {
    if (tab_count <= kMaxIndividualTabVisualsCompactHeight) {
      return 0;
    }
    // For > 5 tabs: Only the first 4 tabs are visible.
    return tab_count - 4;
  }
  if (tab_count <= kMaxIndividualTabVisuals) {
    return 0;
  }
  // For > 7 tabs: Only the first 6 tabs are visible.
  return tab_count - 6;
}
