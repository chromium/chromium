// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"

@implementation AutocompleteSuggestionGroupImpl

- (instancetype)initWithTitle:(NSString*)title
                  suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
                 displayStyle:(SuggestionGroupDisplayStyle)displayStyle {
  self = [super init];
  if (self) {
    _title = [title copy];
    _suggestions = suggestions;
    _displayStyle = displayStyle;
  }
  return self;
}

+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
      displayStyle:(SuggestionGroupDisplayStyle)displayStyle {
  return [[self alloc] initWithTitle:title
                         suggestions:suggestions
                        displayStyle:displayStyle];
}

+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions {
  return [[self alloc] initWithTitle:title
                         suggestions:suggestions
                        displayStyle:SuggestionGroupDisplayStyleDefault];
}

@end
