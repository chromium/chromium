// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SWITCHER_ITEM_SNAPSHOT_AND_FAVICON_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SWITCHER_ITEM_SNAPSHOT_AND_FAVICON_DATA_SOURCE_H_

@class TabSnapshotAndFavicon;
@class TabSwitcherItem;

// Block invoked when a TabSnapshotAndFavicon fetching operation completes. The
// `tabSnapshotAndFavicon` is nil if the operation failed.
typedef void (^TabSnapshotAndFaviconFetchingCompletionBlock)(
    TabSwitcherItem* item,
    TabSnapshotAndFavicon* tabSnapshotAndFavicon);

// Protocol that the UI uses to retrieve snapshots and favicons for tab
// switcher items.
@protocol TabSwitcherItemSnapShotAndFaviconDataSource

// Fetches the `item` snapshot and favicon.
// The `completion` block is invoked twice: once when the snapshot has been
// fetched, and again when the favicon has been fetched.
- (void)fetchTabSnapshotAndFavicon:(TabSwitcherItem*)item
                        completion:
                            (TabSnapshotAndFaviconFetchingCompletionBlock)
                                completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_SWITCHER_ITEM_SNAPSHOT_AND_FAVICON_DATA_SOURCE_H_
