// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_DELEGATE_H_

#import <Foundation/Foundation.h>

// Protocol used to issue commands related to search suggested actions.
@protocol SuggestedActionsDelegate <NSObject>

// Tells the delegate to fetch the search history results count for
// `searchText` and provide it to the `completion` block.
- (void)fetchSearchHistoryResultsCountForText:(NSString*)searchText
                                   completion:(void (^)(size_t))completion;

// Asks the delegate to open a history modal and filter the history
// `searchText`.
- (void)searchHistoryForText:(NSString*)searchText;

// Asks the delegate to open a new tab and use the default search engine to
// search for `searchText` in web.
- (void)searchWebForText:(NSString*)searchText;

// Asks the delegate to switch the tab grid page to recent tabs and filter the
// page content by `searchText`.
- (void)searchRecentTabsForText:(NSString*)searchText;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_SUGGESTED_ACTIONS_SUGGESTED_ACTIONS_DELEGATE_H_
