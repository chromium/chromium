// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_autocomplete_event.h"

#import "components/omnibox/browser/autocomplete_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_event.h"

@implementation OmniboxAutocompleteEvent {
  /// Whether all sync and async providers from the autocomplete controller are
  /// done.
  BOOL _autocompleteControllerAsyncPassDone;
  /// Whether all sync providers from the autocomplete controller are done.
  BOOL _autocompleteControllerSyncPassDone;
}

- (OmniboxAutocompleteEvent*)initWithAutocompleteController:
    (AutocompleteController*)controller {
  self = [super init];

  if (self) {
    _autocompleteControllerAsyncPassDone = controller->done();
    _autocompleteControllerSyncPassDone = controller->sync_pass_done();

    self.matches = [[NSMutableArray alloc] init];
    for (auto acm : controller->result()) {
      AutocompleteMatchFormatter* matcher =
          [[AutocompleteMatchFormatter alloc] initWithMatch:acm];
      [self.matches addObject:matcher];
    }
  }
  return self;
}

- (NSString*)title {
  NSString* autocompleteControllerStatus = @"Processing";
  if (_autocompleteControllerAsyncPassDone) {
    autocompleteControllerStatus = @"Async Done";
  } else if (_autocompleteControllerSyncPassDone) {
    autocompleteControllerStatus = @"Sync Done";
  }
  return [NSString
      stringWithFormat:@"Result update (%@)", autocompleteControllerStatus];
}

- (EventType)type {
  return kAutocompleteUpdate;
}

@end
