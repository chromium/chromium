// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/autocomplete_match_group.h"

@implementation AutocompleteMatchGroup

+ (AutocompleteMatchGroup*)
    groupWithTitle:(NSString*)title
           matches:(NSArray<AutocompleteMatchFormatter*>*)matches {
  AutocompleteMatchGroup* group = [[self alloc] init];
  group.title = title;
  group.matches = matches;
  return group;
}

@end
