// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AutocompleteSuggestionGroupImpl

- (instancetype)initWithTitle:(NSString*)title
                  suggestions:
                      (NSArray<id<AutocompleteSuggestion>>*)suggestions {
  self = [super init];
  if (self) {
    _title = [title copy];
    _suggestions = suggestions;
  }
  return self;
}

+ (AutocompleteSuggestionGroupImpl*)
    groupWithTitle:(NSString*)title
       suggestions:(NSArray<id<AutocompleteSuggestion>>*)suggestions {
  return [[self alloc] initWithTitle:title suggestions:suggestions];
}

@end
