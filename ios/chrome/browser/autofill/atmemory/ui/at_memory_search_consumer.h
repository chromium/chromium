// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_CONSUMER_H_

#import <Foundation/Foundation.h>

enum class AtMemoryBackgroundStyle {
  // The default background style.
  kDefaultStyle,
  // Zero-state background style shown on the initial screen when no recent
  // fills exist and the notice is hidden.
  kEmptyStyle,
};

enum class AtMemoryErrorType {
  // The server couldn't be reached.
  kNoConnectionError,
  // No high confidence results could be obtained.
  kNoDataError,
  // No high confidence results could be obtained, but an option to search with
  // Gemini in Chrome is provided.
  kUnsupportedQueryError,
};

// Consumer for the AtMemory search feature.
@protocol AtMemorySearchConsumer <NSObject>

// Sets the `AtMemoryErrorType`.
- (void)setErrorType:(AtMemoryErrorType)errorType;

// TODO(crbug.com/541237598): Will be implemented in a separate CL.
// Sets the progress indicator while fetching results from Gemini.
- (void)setFetchingSubtitle;

// Sets whether the informational notice is visible.
- (void)setNoticeVisible:(BOOL)noticeVisible;

// TODO(crbug.com/540877897): Will be implemented once the backend is ready.
// Sets the previously filled results on the same page.
- (void)setRecentFills;

// TODO(crbug.com/543036121): Create a `setSearchResults` method once
// AtMemorySearchItem has been created.

// Displays the table view background for the given `style`.
- (void)updateTableViewBackgroundStyle:(AtMemoryBackgroundStyle)style;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_SEARCH_CONSUMER_H_
