// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_MATCH_FORMATTER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_MATCH_FORMATTER_H_

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"

struct AutocompleteMatch;
@class OmniboxPedalData;

@interface AutocompleteMatchFormatter : NSObject <AutocompleteSuggestion>

/// This is a temporary solution for coloring strings.
@property(nonatomic, assign, getter=isIncognito) BOOL incognito;
@property(nonatomic, assign, getter=isStarred) BOOL starred;
/// Whether it's a multimodal suggestion.
@property(nonatomic, assign) BOOL isMultimodal;

/// Whether the default search engine is Google impacts which icon is used in
/// some cases
@property(nonatomic, assign) BOOL defaultSearchEngineIsGoogle;

/// The pedal data for the underlying match.
@property(nonatomic, strong) OmniboxPedalData* pedalData;
/// From <AutocompleteSuggestion>, redefined as readwrite.
@property(nonatomic, strong) NSNumber* suggestionSectionId;

/// From <AutocompleteSuggestion>, redefined as readwrite.
@property(nonatomic, strong) NSArray<SuggestAction*>* actionsInSuggest;

- (instancetype)initWithMatch:(const AutocompleteMatch&)match
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Convenience constructor.
+ (instancetype)formatterWithMatch:(const AutocompleteMatch&)match;

/// Underlying match.
- (const AutocompleteMatch&)autocompleteMatch;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_MATCH_FORMATTER_H_
