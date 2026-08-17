// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_CONSTANTS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// The default opacity for cells.
inline constexpr CGFloat kDefaultCellAlpha = 1.0;

// The opacity for disabled cells.
inline constexpr CGFloat kDisabledCellAlpha = 0.5;

// The symbol point size for cell icons.
inline constexpr CGFloat kIconPointSize = 24;

// Accessibility identifier for the AtMemory close button.
extern NSString* const kAtMemoryCloseButtonAccessibilityIdentifier;

// Accessibility identifier for the AtMemory search bar.
extern NSString* const kAtMemorySearchBarAccessibilityIdentifier;

// Accessibility identifier for the AtMemory back button.
extern NSString* const kAtMemoryBackButtonAccessibilityIdentifier;

// Accessibility identifier for the AtMemory "No Data" error cell.
extern NSString* const kAtMemoryNoDataCellAccessibilityIdentifier;

// Accessibility identifier for the AtMemory "No Connection" error cell.
extern NSString* const kAtMemoryNoConnectionCellAccessibilityIdentifier;

// Accessibility identifier for the AtMemory "Unsupported Query" error cell.
extern NSString* const kAtMemoryUnsupportedQueryCellAccessibilityIdentifier;

// Accessibility identifier for the AtMemory search cell.
extern NSString* const kAtMemorySearchCellAccessibilityIdentifier;

// Accessibility identifier for the AtMemory fetching cell.
extern NSString* const kAtMemoryFetchingCellAccessibilityIdentifier;

// Accessibility identifier for the AtMemory "Manage Enhanced Autofill" item.
extern NSString* const
    kAtMemoryManageEnhancedAutofillItemAccessibilityIdentifier;

// Accessibility identifier prefix for the AtMemory granular fill cell.
extern NSString* const kAtMemoryGranularFillCellAccessibilityIdentifierPrefix;

// Accessibility identifier prefix for the AtMemory granular fill attribute
// label.
extern NSString* const
    kAtMemoryGranularFillAttributeLabelAccessibilityIdentifierPrefix;

// Accessibility identifier prefix for the AtMemory granular fill chip button.
extern NSString* const
    kAtMemoryGranularFillChipButtonAccessibilityIdentifierPrefix;

// Accessibility identifier prefix for the AtMemory search result cell.
extern NSString* const kAtMemorySearchResultCellAccessibilityIdentifierPrefix;

// Accessibility identifier prefix for the AtMemory search result info button.
extern NSString* const
    kAtMemorySearchResultInfoButtonAccessibilityIdentifierPrefix;

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_PUBLIC_AT_MEMORY_CONSTANTS_H_
