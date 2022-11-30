// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Enum specifying the type of Content Suggestions a module is showing.
typedef NS_ENUM(int32_t, ContentSuggestionsModuleType) {
  ContentSuggestionsModuleTypeMostVisited,
  ContentSuggestionsModuleTypeShortcuts,
  ContentSuggestionsModuleTypeReturnToRecentTab,
  ContentSuggestionsModuleTypeTrendingQueries,
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

// The bottom margin below the Most Visited section.
extern const CGFloat kMostVisitedBottomMargin;

// Maximum number of Trending Queries shown.
// If the value of this constant is updated, please also update the
// TrendingQueryIndex enum so it can capture a higher max value.
const int kMaxTrendingQueries = 4;

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
