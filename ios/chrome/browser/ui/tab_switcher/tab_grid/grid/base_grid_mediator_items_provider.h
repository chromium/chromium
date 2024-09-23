// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_

@class GridItemIdentifier;

namespace web {
class WebStateID;
}  // namespace web

// Protocol allowing to get information of the grid model.
@protocol BaseGridMediatorItemProvider

// Returns YES if `itemID` is selected in selection mode.
- (BOOL)isItemSelected:(GridItemIdentifier*)itemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_BASE_GRID_MEDIATOR_ITEMS_PROVIDER_H_
