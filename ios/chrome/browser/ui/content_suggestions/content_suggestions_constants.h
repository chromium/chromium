// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/set_up_list_item_type.h"

// Enum specifying the type of Content Suggestions a module is showing.
// Entries should always keep synced with the IOSMagicStackModuleType histogram
// enum. Entries should not be renumbered and numeric values should never be
// reused.
// LINT.IfChange
enum class ContentSuggestionsModuleType {
  kMostVisited = 0,
  kShortcuts = 1,
  kSetUpListSync = 2,
  kSetUpListDefaultBrowser = 3,
  kSetUpListAutofill = 4,
  kCompactedSetUpList = 5,
  kSetUpListAllSet = 6,
  kSafetyCheck = 7,
  kSafetyCheckMultiRow = 8,
  kSafetyCheckMultiRowOverflow = 9,
  kTabResumption = 10,
  kParcelTracking = 11,
  kParcelTrackingSeeMore = 12,
  kMaxValue = kParcelTrackingSeeMore,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml)

// Represents the content suggestions collection view.
extern NSString* const kContentSuggestionsCollectionIdentifier;

// Represents the Learn More button in the content suggestions.
extern NSString* const kContentSuggestionsLearnMoreIdentifier;

// Represents the most visited tiles of the content suggestions.
extern NSString* const
    kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix;

// Represents the shortcuts of the content suggestions.
extern NSString* const
    kContentSuggestionsShortcutsAccessibilityIdentifierPrefix;

// Represents the Magic Stack ScrollView.
extern NSString* const kMagicStackScrollViewAccessibilityIdentifier;

// Represents the Magic Stack UIStackView.
extern NSString* const kMagicStackViewAccessibilityIdentifier;

// Represents the Edit Button Container in the Magic Stack.
extern NSString* const kMagicStackEditButtonContainerAccessibilityIdentifier;

// Represents the Edit Button in the Magic Stack.
extern NSString* const kMagicStackEditButtonAccessibilityIdentifier;

// Represents the "Done" button in the Magic Stack edit half sheet.
extern NSString* const
    kMagicStackEditHalfSheetDoneButtonAccessibilityIdentifier;

// Represents the width of the Magic Stack ScrollView for the unique wide
// layout.
extern const CGFloat kMagicStackWideWidth;

// The bottom margin below the Most Visited section.
extern const CGFloat kMostVisitedBottomMargin;

// Most Visited Tiles favicon width when kMagicStack is enabled.
extern const CGFloat kMagicStackFaviconWidth;

// Maximum number of Trending Queries shown.
// If the value of this constant is updated, please also update the
// TrendingQueryIndex enum so it can capture a higher max value.
const int kMaxTrendingQueries = 4;

// Returns the matching ContentSuggestionsModuleType for a given
// SetUpListItemType `type`.
ContentSuggestionsModuleType SetUpListModuleTypeForSetUpListType(
    SetUpListItemType type);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
