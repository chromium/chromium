// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_AT_MEMORY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_AT_MEMORY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// AtMemoryAppInterface contains the app-side implementation for helpers.
// These helpers are compiled into the app binary and can be called from tests.
@interface AtMemoryAppInterface : NSObject

// Sets up the fake search provider with configured search results and granular
// fill items.
+ (void)setUpFakeSearchProviderWithSearchResults:
            (NSArray<NSDictionary*>*)searchResults
                               granularFillItems:
                                   (NSArray<NSDictionary*>*)granularFillItems;

// Tears down the fake search provider.
+ (void)tearDownFakeSearchProvider;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_TEST_AT_MEMORY_APP_INTERFACE_H_
