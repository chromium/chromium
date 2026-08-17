// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_

#import <Foundation/Foundation.h>

@class AtMemoryGranularFillItem;

namespace autofill {
struct MemorySearchResult;
}

// Returns the granular fill title for `result`.
NSString* GetAtMemoryGranularFillTitle(
    const autofill::MemorySearchResult& result);

// Returns an array of AtMemoryGranularFillItem objects created from `result`.
NSArray<AtMemoryGranularFillItem*>* AtMemoryGranularFillItemsForSearchResult(
    const autofill::MemorySearchResult& result);

// Returns the accessibility identifier for the granular fill cell
// corresponding to `attribute_name`.
NSString* GetAtMemoryGranularFillCellAccessibilityIdentifier(
    NSString* attribute_name);

// Returns the accessibility identifier for the granular fill attribute label
// corresponding to `attribute_name`.
NSString* GetAtMemoryGranularFillAttributeLabelAccessibilityIdentifier(
    NSString* attribute_name);

// Returns the accessibility identifier for the granular fill chip button
// corresponding to `attribute_name`.
NSString* GetAtMemoryGranularFillChipButtonAccessibilityIdentifier(
    NSString* attribute_name);

// Returns the accessibility identifier for the search result cell
// corresponding to `title`.
NSString* GetAtMemorySearchResultCellAccessibilityIdentifier(NSString* title);

// Returns the accessibility identifier for the search result's info button
// corresponding to `title`.
NSString* GetAtMemorySearchResultInfoButtonAccessibilityIdentifier(
    NSString* title);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
