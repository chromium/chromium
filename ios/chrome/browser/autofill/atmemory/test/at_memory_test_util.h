// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_AT_MEMORY_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_AT_MEMORY_TEST_UTIL_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

// Test utility for EarlGrey tests of the AtMemory feature.
@interface AtMemoryTestUtil : NSObject

// Returns a matcher for the AtMemory keyboard accessory button.
+ (id<GREYMatcher>)atMemoryButton;

// Returns a matcher for the search bar inside the AtMemory bottom sheet.
+ (id<GREYMatcher>)searchBar;

// Returns a matcher for the close button inside the AtMemory bottom sheet.
+ (id<GREYMatcher>)closeButton;

// Returns a matcher for the search prompt cell ("Find and fill this with
// Gemini").
+ (id<GREYMatcher>)searchPromptCell;

// Returns a matcher for the search prompt cell displaying `query`.
+ (id<GREYMatcher>)searchPromptCellWithQuery:(NSString*)query;

// Returns a matcher for the "No Data" error cell.
+ (id<GREYMatcher>)noDataCell;

// Returns a matcher for the "No Connection" error cell.
+ (id<GREYMatcher>)noConnectionCell;

// Returns a matcher for the image displayed in the zero-state empty view.
+ (id<GREYMatcher>)emptyStateImage;

// Returns a matcher for a search result cell with the given `title`.
+ (id<GREYMatcher>)searchResultCellWithTitle:(NSString*)title;

// Returns a matcher for the info button on a search result cell with the given
// `title`.
+ (id<GREYMatcher>)infoButtonForSearchResultWithTitle:(NSString*)title;

// Returns a matcher for a granular fill chip button with the given `label`.
+ (id<GREYMatcher>)chipButtonWithLabel:(NSString*)label;

// Returns a matcher for the "Manage your saved info" cell in granular fill.
+ (id<GREYMatcher>)manageSavedInfoCell;

// Returns a matcher for the AtMemory zero-state empty view.
+ (id<GREYMatcher>)emptyView;

// Returns a matcher for the inline privacy notice title label.
+ (id<GREYMatcher>)inlineNoticeTitle;

// Returns a matcher for the inline privacy notice "OK" button.
+ (id<GREYMatcher>)inlineNoticeOKButton;

// Returns a matcher for the AI disclosure footer view.
+ (id<GREYMatcher>)aiDisclosureFooter;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_AT_MEMORY_TEST_UTIL_H_
