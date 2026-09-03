// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_CONSUMER_H_

#import <Foundation/Foundation.h>

enum class AtMemoryErrorType {
  // The server couldn't be reached.
  kNoConnectionError,
  // No high confidence results could be obtained.
  kNoDataError,
  // No high confidence results could be obtained, but an option to search with
  // Gemini in Chrome is provided.
  kUnsupportedQueryError,
};

@class AtMemorySearchItem;
// Consumer for the AtMemory search feature.
@protocol AtMemorySearchConsumer <NSObject>

// Sets the `AtMemoryErrorType`.
- (void)setErrorType:(AtMemoryErrorType)errorType;

// TODO(crbug.com/541237598): Will be implemented in a separate CL.
// Sets the progress indicator while fetching results from Gemini.
- (void)setFetchingSubtitle;

// Sets whether the informational notice is visible.
- (void)setNoticeVisible:(BOOL)noticeVisible;

// Sets the previously filled results on the same page to display in the
// initial empty search state.
- (void)setRecentFills:(NSArray<AtMemorySearchItem*>*)recentFills;

// Sets search results to display in the UI.
- (void)setSearchResults:(NSArray<AtMemorySearchItem*>*)searchResults;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_CONSUMER_H_
