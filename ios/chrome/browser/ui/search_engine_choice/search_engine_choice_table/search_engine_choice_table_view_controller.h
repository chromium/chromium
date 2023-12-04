// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_consumer.h"

class FaviconLoader;

// Action delegate for the search engine choice table.
@protocol SearchEngineChoiceTableActionDelegate <NSObject>

// Called when the user taps on the designated row.
- (void)selectSearchEngineAtRow:(NSInteger)row;
// Called when the table view reach the bottom for the first time.
- (void)didReachBottom;

@end

@interface SearchEngineChoiceTableViewController
    : LegacyChromeTableViewController <SearchEngineChoiceTableConsumer>

@property(nonatomic, weak) id<SearchEngineChoiceTableActionDelegate> delegate;

// YES if the table view reached the bottom at least once.
@property(nonatomic, assign) BOOL didReachBottom;

// Scrolls the table view to the bottom.
- (void)scrollToBottom;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_TABLE_SEARCH_ENGINE_CHOICE_TABLE_VIEW_CONTROLLER_H_
