// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_GROUP_IMPL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_GROUP_IMPL_H_

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"

// A basic implementation of AutocompleteSuggestionGroup
@interface AutocompleteSuggestionGroupImpl
    : NSObject <AutocompleteSuggestionGroup>

// Optional title.
@property(nonatomic, copy, readonly) NSString* title;

// Contained suggestions.
@property(nonatomic, strong, readonly)
    NSArray<id<AutocompleteSuggestion>>* suggestions;

- (instancetype)initWithTitle:(NSString*)title
                  suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions;

+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_GROUP_IMPL_H_
