// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_

@class GridItemIdentifier;

@class ActivityLabelData;
namespace web {
class WebStateID;
}  // namespace web

// Protocol allowing to get information of the grid model.
@protocol BaseGridMediatorItemProvider

// Returns YES if `itemID` is selected in selection mode.
- (BOOL)isItemSelected:(GridItemIdentifier*)itemID;

// Returns the information needed for showing the label on the cell. Returns nil
// if the label shouldn't be displayed.
- (ActivityLabelData*)activityLabelDataForItem:(GridItemIdentifier*)itemID;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
