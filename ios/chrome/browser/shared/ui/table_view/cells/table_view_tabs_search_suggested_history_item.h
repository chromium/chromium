// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TABS_SEARCH_SUGGESTED_HISTORY_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TABS_SEARCH_SUGGESTED_HISTORY_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"

// Model object for an item which displays a suggested action to search the
// user's history.
@interface TableViewTabsSearchSuggestedHistoryItem : TableViewImageItem
@end

@interface TableViewTabsSearchSuggestedHistoryCell : TableViewImageCell

// The current search term associated with this cell.
@property(nonatomic, copy) NSString* searchTerm;

// Updates the cell title with `resultsCount` to display the number of matches.
- (void)updateHistoryResultsCount:(size_t)resultsCount;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TABS_SEARCH_SUGGESTED_HISTORY_ITEM_H_
