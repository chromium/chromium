// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_OMNIBOX_POPUP_FAKE_AUTOCOMPLETE_SUGGESTION_H_
#define IOS_SHOWCASE_OMNIBOX_POPUP_FAKE_AUTOCOMPLETE_SUGGESTION_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"

@protocol OmniboxIcon;

// Fake class implementing AutocompleteSuggestion for Showcase.
@interface FakeAutocompleteSuggestion : NSObject <AutocompleteSuggestion>

@property(nonatomic) BOOL supportsDeletion;
@property(nonatomic) BOOL hasAnswer;
@property(nonatomic) BOOL isURL;
@property(nonatomic) BOOL isAppendable;
@property(nonatomic) BOOL isTabMatch;
@property(nonatomic) NSAttributedString* text;
@property(nonatomic) NSAttributedString* detailText;
@property(nonatomic) NSInteger numberOfLines;
@property(nonatomic) UIImage* suggestionTypeIcon;

@property(nonatomic) id<OmniboxIcon> icon;

// Simple suggestion with text.
+ (instancetype)simpleSuggestion;

// Suggestion with detail text.
+ (instancetype)suggestionWithDetail;

// Suggestion with text long enough to clip on iPhone.
+ (instancetype)clippingSuggestion;

// Suggestion that can be appended.
+ (instancetype)appendableSuggestion;

// Suggestion that will switch to open tab.
+ (instancetype)otherTabSuggestion;

// Suggestion that can be deleted.
+ (instancetype)deletableSuggestion;

// Suggestion with answer for weather.
+ (instancetype)weatherSuggestion;

// Suggestion with answer for stock price.
+ (instancetype)stockSuggestion;

// Suggestion with answer for definition.
+ (instancetype)definitionSuggestion;

// Suggestion with answer for sunrise time.
+ (instancetype)sunriseSuggestion;

// Suggestion with answer for knowledge.
+ (instancetype)knowledgeSuggestion;

// Suggestion with answer for sports.
+ (instancetype)sportsSuggestion;

// Suggestion with answer for "when is" (When is <some event>).
+ (instancetype)whenIsSuggestion;

// Suggestion with answer for currency.
+ (instancetype)currencySuggestion;

// Suggestion with answer for translate.
+ (instancetype)translateSuggestion;

// Suggestion for calculator.
+ (instancetype)calculatorSuggestion;

// Suggestion for a rich entity (entity with image).
+ (instancetype)richEntitySuggestion;

@end

#endif  // IOS_SHOWCASE_OMNIBOX_POPUP_FAKE_AUTOCOMPLETE_SUGGESTION_H_
