// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_

#import <UIKit/UIKit.h>

// State of the snippet in SnippetSearchEngineItem/Cell.
enum class SnippetState {
  // The chevron is pointing down, the snippet is hidden.
  kHidden,
  // The chevron is pointing up, the snippet is shown.
  kShown,
};

// Fake omnibox width and height (for the empty and not-empty illustration).
extern const CGFloat kFakeOmniboxWidth;
extern const CGFloat kFakeOmniboxHeight;
// Favicon size and radius.
extern const CGFloat kFaviconImageViewSize;
extern const CGFloat kFaviconImageViewRadius;
// The space before the fake omnibox field.
extern const CGFloat kFakeOmniboxFieldLeadingInset;

// Prefix for the SearchEngineCell accessibility identifier.
extern NSString* const kSnippetSearchEngineIdentifierPrefix;
// `Set as Default` button accessibility identifier.
extern NSString* const kSetAsDefaultSearchEngineIdentifier;
// Search engine table view identifier.
extern NSString* const kSearchEngineTableViewIdentifier;
// `More` button accessibility identifier.
extern NSString* const kSearchEngineMoreButtonIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_CONSTANTS_H_
