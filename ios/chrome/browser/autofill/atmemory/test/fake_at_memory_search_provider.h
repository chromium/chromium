// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_FAKE_AT_MEMORY_SEARCH_PROVIDER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_FAKE_AT_MEMORY_SEARCH_PROVIDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_provider.h"

// Fake implementation of AtMemorySearchProvider that returns configured search
// results and granular fill items.
@interface FakeAtMemorySearchProvider : NSObject <AtMemorySearchProvider>

// Configures search results and granular fill items for the fake search
// provider. Each search result dictionary can contain @"fillingText",
// @"subtitle", and @"iconSymbolName". Each granular fill dictionary can contain
// @"name" (NSString) and @"values" (NSArray<NSString*>).
- (void)setSearchResults:(NSArray<NSDictionary*>*)searchResults
       granularFillItems:(NSArray<NSDictionary*>*)granularFillItems;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_FAKE_AT_MEMORY_SEARCH_PROVIDER_H_
