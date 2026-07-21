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

// Returns a matcher for a search result cell matching the given subtitle text.
+ (id<GREYMatcher>)searchResultCellWithSubtitle:(NSString*)subtitle;

// Returns a matcher for the info button on a search result cell with the given
// subtitle text.
+ (id<GREYMatcher>)infoButtonForSearchResultWithSubtitle:(NSString*)subtitle;

// Returns a matcher for a granular fill chip button with the given label.
+ (id<GREYMatcher>)chipButtonWithLabel:(NSString*)label;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_AT_MEMORY_TEST_UTIL_H_
