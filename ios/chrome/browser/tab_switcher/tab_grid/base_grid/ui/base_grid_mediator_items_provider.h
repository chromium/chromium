// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_UI_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_UI_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"

@class ActivityLabelData;
@protocol FacePileProviding;
@class GridItemIdentifier;
@class TabSnapshotAndFavicon;

// Block invoked when a TabSnapshotAndFavicon fetching operation completes.
typedef void (^GroupTabSnapshotAndFaviconCompletionBlock)(
    TabGroupItem* item,
    NSInteger tabIndex,
    TabSnapshotAndFavicon* tabSnapshotAndFavicon);

// Protocol allowing to get information of the grid model.
@protocol BaseGridMediatorItemProvider

// Returns YES if `itemID` is selected in selection mode.
- (BOOL)isItemSelected:(GridItemIdentifier*)itemID;

// Returns the information needed for showing the label on the cell. Returns nil
// if the label shouldn't be displayed.
- (ActivityLabelData*)activityLabelDataForItem:(GridItemIdentifier*)itemID;

// Returns the facePile view associated with the `itemID`.
- (id<FacePileProviding>)facePileProviderForItem:(GridItemIdentifier*)itemID;

// Fetches snapshots and favicons for the tabs within `tabGroupItem`.
// The `completion` block is called multiple times, executing each time a
// snapshot or favicon for an individual tab is fetched.
- (void)
    fetchTabGroupItemSnapshotsAndFavicons:(TabGroupItem*)tabGroupItem
                               completion:
                                   (GroupTabSnapshotAndFaviconCompletionBlock)
                                       completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_TAB_GRID_BASE_GRID_UI_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
