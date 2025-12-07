// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@protocol FacePileProviding;
@class TabGroupsPanelCell;
@class TabGroupsPanelItem;
@class TabGroupsPanelItemData;

// Protocol used to query relevant properties related to a given
// TabGroupsPanelItem.
@protocol TabGroupsPanelItemDataSource

// Returns the data associated with the item.
- (TabGroupsPanelItemData*)dataForItem:(TabGroupsPanelItem*)item;

// Fetches the `cell` favicons.
- (void)fetchFaviconsForCell:(TabGroupsPanelCell*)cell;

// Returns the facePile view associated with the item.
- (id<FacePileProviding>)facePileProviderForItem:(TabGroupsPanelItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_SOURCE_H_
