// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_SHAREABLE_ITEMS_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_SHAREABLE_ITEMS_PROVIDER_H_

#import <Foundation/Foundation.h>

// Protocol for instances that will provide shareable state for items in the
// Grid view.
@protocol GridShareableItemsProvider

// Returns whether the item with `itemID` is shareable.
- (BOOL)isItemWithIDShareable:(web::WebStateID)itemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_SHAREABLE_ITEMS_PROVIDER_H_
