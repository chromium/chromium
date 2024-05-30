// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Favicon size and radius.
extern const CGFloat kFaviconImageViewSize;
extern const CGFloat kFaviconImageViewRadius;

// Preferred width and height of the Search Engine Choice screen on iPad.
// We prefer to set a prefferred content size to avoid cases when the view is
// too big which leads to too much white space and unnecessary chevrons. This
// may need to be adjusted if the layout of the choice screen changes.
extern const CGFloat kIPadSearchEngineChoiceScreenPreferredWidth;
extern const CGFloat kIPadSearchEngineChoiceScreenPreferredHeight;

// Accessibility identifier for the choice screen title.
extern NSString* const kSearchEngineChoiceTitleAccessibilityIdentifier;
// Prefix for the SnippetSearchEngineButton accessibility identifier.
extern NSString* const kSnippetSearchEngineIdentifierPrefix;
// Prefix for the chevron accessibility identifier in SnippetSearchEngineButton,
// when showing one line.
extern NSString* const kSnippetSearchEngineOneLineChevronIdentifierPrefix;
// Prefix for the chevron accessibility identifier in SnippetSearchEngineButton,
// when being expanded.
extern NSString* const kSnippetSearchEngineExpandedChevronIdentifierPrefix;
// `Set as Default` button accessibility identifier.
extern NSString* const kSetAsDefaultSearchEngineIdentifier;
// Search engine choice scroll view identifier.
extern NSString* const kSearchEngineChoiceScrollViewIdentifier;
// `More` button accessibility identifier.
extern NSString* const kSearchEngineMoreButtonIdentifier;
// `Continue` button accessibility identifier.
extern NSString* const kSearchEngineContinueButtonIdentifier;
// User action when the search engine snippet is expanded.
extern const char kExpandSearchEngineDescriptionUserAction[];

// Accessibility identifier for the Learn More view.
extern NSString* const kSearchEngineChoiceLearnMoreAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_
