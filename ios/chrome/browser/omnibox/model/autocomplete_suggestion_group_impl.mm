// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/autocomplete_suggestion_group_impl.h"

@implementation AutocompleteSuggestionGroupImpl

- (instancetype)initWithTitle:(NSString*)title
                  suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
                 displayStyle:(SuggestionGroupDisplayStyle)displayStyle
                         type:(SuggestionGroupType)type {
  self = [super init];
  if (self) {
    _title = [title copy];
    _suggestions = suggestions;
    _displayStyle = displayStyle;
    _type = type;
  }
  return self;
}

+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
      displayStyle:(SuggestionGroupDisplayStyle)displayStyle
              type:(SuggestionGroupType)type {
  return [[self alloc] initWithTitle:title
                         suggestions:suggestions
                        displayStyle:displayStyle
                                type:type];
}

+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions
              type:(SuggestionGroupType)type {
  return [[self alloc] initWithTitle:title
                         suggestions:suggestions
                        displayStyle:SuggestionGroupDisplayStyleDefault
                                type:type];
}

@end
