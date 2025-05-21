// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"

@class ActivityLabelData;
@class GridItemIdentifier;
@class TabSnapshotAndFavicon;
namespace web {
class WebStateID;
}  // namespace web

// Block invoked when a TabSnapshotAndFavicon fetching operation completes. The
// `groupTabInfos` is nil if the operation failed.
typedef void (^GroupTabSnapshotAndFaviconCompletionBlock)(
    TabGroupItem* item,
    NSArray<TabSnapshotAndFavicon*>* groupTabInfos);

// Protocol allowing to get information of the grid model.
@protocol BaseGridMediatorItemProvider

// Returns YES if `itemID` is selected in selection mode.
- (BOOL)isItemSelected:(GridItemIdentifier*)itemID;

// Returns the information needed for showing the label on the cell. Returns nil
// if the label shouldn't be displayed.
- (ActivityLabelData*)activityLabelDataForItem:(GridItemIdentifier*)itemID;

// Returns the facePile view associated with the `itemID`.
- (UIView*)facePileViewForItem:(GridItemIdentifier*)itemID;

// Fetches the `tabGroupItem` snapshot and favicon, then executes the given
// `completion` block.
- (void)fetchTabGroupItemInfo:(TabGroupItem*)tabGroupItem
                   completion:
                       (GroupTabSnapshotAndFaviconCompletionBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
