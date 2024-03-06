// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Favicon size and radius.
extern const CGFloat kFaviconImageViewSize;
extern const CGFloat kFaviconImageViewRadius;

// Accessibility identifier for the choice screen title.
extern NSString* const kSearchEngineChoiceTitleAccessibilityIdentifier;
// Prefix for the SearchEngineCell accessibility identifier.
extern NSString* const kSnippetSearchEngineIdentifierPrefix;
// `Set as Default` button accessibility identifier.
extern NSString* const kSetAsDefaultSearchEngineIdentifier;
// Search engine choice scroll view identifier.
extern NSString* const kSearchEngineChoiceScrollViewIdentifier;
// `More` button accessibility identifier.
extern NSString* const kSearchEngineMoreButtonIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_
