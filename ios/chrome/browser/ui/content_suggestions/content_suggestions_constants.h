// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ntp/set_up_list_item_type.h"

// Enum specifying the type of Content Suggestions a module is showing.
enum class ContentSuggestionsModuleType {
  kMostVisited,
  kShortcuts,
  kReturnToRecentTab,
  kSetUpListSync,
  kSetUpListDefaultBrowser,
  kSetUpListAutofill,
  kCompactedSetUpList,
  kSetUpListAllSet,
};

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

// Represents the width of the Magic Stack ScrollView for the unique wide
// layout.
extern const CGFloat kMagicStackWideWidth;

// The bottom margin below the Most Visited section.
extern const CGFloat kMostVisitedBottomMargin;

// Maximum number of Trending Queries shown.
// If the value of this constant is updated, please also update the
// TrendingQueryIndex enum so it can capture a higher max value.
const int kMaxTrendingQueries = 4;

// Tile Ablation constants.
// The amount of time between two tile ablation NTP impressions. (User opens
// NTP, 1 impression. If user goes back to NTP within
// `kTileAblationImpressionThresholdMinutes` don't count that as an NTP
// impression.
extern const int kTileAblationImpressionThresholdMinutes;
// Minimum and Maximum amount of days the Tile Ablation experiment can run for.
extern const int kTileAblationMinimumUseThresholdInDays;
extern const int kTileAblationMaximumUseThresholdInDays;
// Minimum an Maximum number of impressions until the Tile Ablation experiment
// ends before the NTP goes back to the normal state.
extern const int kMinimumImpressionThresholdTileAblation;
extern const int kMaximumImpressionThresholdTileAblation;
// Stores the last time an NTP impression was recorded.
extern NSString* const kLastNTPImpressionRecordedKey;
// Stores the number of NTP impressions over a period of time.
extern NSString* const kNumberOfNTPImpressionsRecordedKey;
// Stores the first NTP impression for the MVT experiment.
extern NSString* const kFirstImpressionRecordedTileAblationKey;
extern NSString* const kDoneWithTileAblationKey;

// Returns the matching ContentSuggestionsModuleType for a given
// SetUpListItemType `type`.
ContentSuggestionsModuleType SetUpListModuleTypeForSetUpListType(
    SetUpListItemType type);

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
