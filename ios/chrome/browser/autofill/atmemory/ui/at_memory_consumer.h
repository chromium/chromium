// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_

#import <Foundation/Foundation.h>

namespace autofill {

// Represents the different view states of the AtMemory screen.
enum class AtMemoryViewState {
  // Shows the empty state view.
  kEmpty,
  // Shows previously filled items.
  kRecentFills,
  // Shows the details page of a selected item, allowing the user to tap and
  // fill individual attributes.
  kGranularFill,
  // Shows search cell (including loading state).
  kSearch,
  // Shows search results.
  kSearchResults,
  // Shows that the query is unsupported.
  kQueryUnsupported,
  // Shows a "no data" message.
  kNoData,
};

}  // namespace autofill

@class AtMemoryGranularFillItem;
@class AtMemorySearchItem;
@class AtMemorySearchResultItem;

// Consumer for AtMemory.
@protocol AtMemoryConsumer <NSObject>

// Sets the current view state of the AtMemory screen.
- (void)setViewState:(autofill::AtMemoryViewState)viewState;

// Sets the recent fills.
- (void)setRecentFills:(NSArray<AtMemorySearchItem*>*)recentFills;

// Sets the granular fill items.
- (void)setGranularFillItems:(NSArray<AtMemoryGranularFillItem*>*)items;

// Sets the current search query.
- (void)setSearchQuery:(NSString*)query;

// Sets whether the search is loading.
- (void)setSearchLoading:(BOOL)loading;

// Sets the search results.
- (void)setSearchResults:(NSArray<AtMemorySearchResultItem*>*)results;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UI_AT_MEMORY_CONSUMER_H_
