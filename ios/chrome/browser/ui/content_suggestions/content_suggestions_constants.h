// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_

#import <UIKit/UIKit.h>

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

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_CONSTANTS_H_
