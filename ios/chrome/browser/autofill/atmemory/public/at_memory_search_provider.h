// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_SEARCH_PROVIDER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_SEARCH_PROVIDER_H_

#import <Foundation/Foundation.h>

@class AtMemoryGranularFillItem;
@class AtMemorySearchResultItem;

// Protocol for providing AtMemory search results and granular fill items.
@protocol AtMemorySearchProvider <NSObject>

// Returns search results matching the query text.
- (NSArray<AtMemorySearchResultItem*>*)searchResultsForText:(NSString*)text;

// Returns granular fill items for the selected search result item.
- (NSArray<AtMemoryGranularFillItem*>*)granularFillItemsForItem:
    (AtMemorySearchResultItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_SEARCH_PROVIDER_H_
