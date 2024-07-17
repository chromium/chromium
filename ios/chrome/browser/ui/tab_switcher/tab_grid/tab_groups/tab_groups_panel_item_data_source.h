// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

@class TabGroupsPanelItem;
@class TabGroupsPanelItemData;

// Protocol used to query relevant properties related to a given
// TabGroupsPanelItem.
@protocol TabGroupsPanelItemDataSource

// Returns the data associated with the item and fetches up to 4 favicons.
// It will fetch up to 4 favicons if the tab group has at most 4 tabs, and only
// 3 favicons if the tab group has strictly more than 4 tabs.
- (TabGroupsPanelItemData*)dataForItem:(TabGroupsPanelItem*)item
           withFaviconsFetchCompletion:(void (^)(NSArray<UIImage*>*))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUPS_TAB_GROUPS_PANEL_ITEM_DATA_SOURCE_H_
