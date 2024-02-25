// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_autocomplete_event.h"

#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/debugger/omnibox_event.h"

namespace {

/// Copy autocomplete matches into a NSArray.
template <class Iterable>
NSArray<AutocompleteMatchFormatter*>* ExtractAutocompleteMatches(
    const Iterable& matches) {
  NSMutableArray<AutocompleteMatchFormatter*>* mutableArray =
      [[NSMutableArray alloc] init];
  for (const auto& match : matches) {
    AutocompleteMatchFormatter* matchFormatter =
        [[AutocompleteMatchFormatter alloc] initWithMatch:match];
    [mutableArray addObject:matchFormatter];
  }
  return mutableArray;
}

}  // namespace

@implementation OmniboxAutocompleteEvent {
  /// Represents the type of the latest autocomplete pass. See
  /// `AutocompleteController::UpdateType`.
  std::string _autocompleteControllerLastUpdateType;
  /// List of the autocomplete matches from the AutocompleteResult.
  NSArray<AutocompleteMatchFormatter*>* _matches;
  /// List of autocomplete matches from the ShortcutsProvider.
  NSArray<AutocompleteMatchFormatter*>* _shortcutsMatches;
}

- (OmniboxAutocompleteEvent*)initWithAutocompleteController:
    (AutocompleteController*)controller {
  self = [super init];

  if (self) {
    _autocompleteControllerLastUpdateType =
        AutocompleteController::UpdateTypeToDebugString(
            controller->last_update_type());

    // Extract matches.
    _matches = ExtractAutocompleteMatches(controller->result());

    // Adding shortcuts suggestions for debugging purposes. Future provider may
    // be added with a way to filter the list of providers to avoid scrolling.
    for (const auto& provider : controller->providers()) {
      switch (provider->type()) {
        case AutocompleteProvider::TYPE_SHORTCUTS:
          _shortcutsMatches = ExtractAutocompleteMatches(provider->matches());
          break;
        default:
          break;
      }
    }

    // Create groups.
    NSMutableArray<AutocompleteMatchGroup*>* groups =
        [[NSMutableArray alloc] init];
    [groups addObject:[AutocompleteMatchGroup groupWithTitle:nil
                                                     matches:_matches]];
    if (_shortcutsMatches.count) {
      [groups
          addObject:[AutocompleteMatchGroup groupWithTitle:@"Shortcuts"
                                                   matches:_shortcutsMatches]];
    }
    _matchGroups = groups;
  }
  return self;
}

- (NSString*)title {
  return
      [NSString stringWithFormat:@"Result update (%s)",
                                 _autocompleteControllerLastUpdateType.c_str()];
}

- (EventType)type {
  return kAutocompleteUpdate;
}

@end
