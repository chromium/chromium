// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_autocomplete_event.h"

#import "components/omnibox/browser/autocomplete_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_event.h"

@implementation OmniboxAutocompleteEvent

- (OmniboxAutocompleteEvent*)initWithAutocompleteController:
    (AutocompleteController*)controller {
  self = [super init];

  if (self) {
    self.matches = [[NSMutableArray alloc] init];
    self.autocompleteControllerIsDone = controller->done();
    for (auto acm : controller->result()) {
      AutocompleteMatchFormatter* matcher =
          [[AutocompleteMatchFormatter alloc] initWithMatch:acm];
      [self.matches addObject:matcher];
    }
  }
  return self;
}

- (NSString*)title {
  return [NSString
      stringWithFormat:@"Result update %@", self.autocompleteControllerIsDone
                                                ? @"(Final)"
                                                : @"(Processing)"];
}

- (EventType)type {
  return kAutocompleteUpdate;
}

@end
