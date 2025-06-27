// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_AUTOCOMPLETE_SUGGESTION_GROUP_IMPL_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_AUTOCOMPLETE_SUGGESTION_GROUP_IMPL_H_

#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_suggestion.h"

// A basic implementation of AutocompleteSuggestionGroup
@interface AutocompleteSuggestionGroupImpl
    : NSObject <AutocompleteSuggestionGroup>

// Optional title.
@property(nonatomic, copy, readonly) NSString* title;

// Contained suggestions.
@property(nonatomic, strong, readonly)
    NSArray<id<AutocompleteSuggestion>>* suggestions;

// How suggestion are displayed.
@property(nonatomic, readonly) SuggestionGroupDisplayStyle displayStyle;

// The suggestion group type.
@property(nonatomic, readonly) SuggestionGroupType type;

- (instancetype)initWithTitle:(NSString*)title
                  suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
                 displayStyle:(SuggestionGroupDisplayStyle)displayStyle
                         type:(SuggestionGroupType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
      displayStyle:(SuggestionGroupDisplayStyle)displayStyle
              type:(SuggestionGroupType)type;

// Instantiates a suggestion group with `SuggestionGroupDisplayStyleDefault` as
// displayStyle.
+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
              type:(SuggestionGroupType)type;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_AUTOCOMPLETE_SUGGESTION_GROUP_IMPL_H_
