// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion_group_impl.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_suggestion_wrapper.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_match_preview_delegate.h"

namespace {

/// Time interval for pedal debouncing. Pedal retrieval is async, use a timer to
/// avoid pedal flickering (cf. crbug.com/1316404).
const NSTimeInterval kPedalDebouceTimer = 0.3;

}  // namespace

@interface PedalSectionExtractor ()

@property(nonatomic, strong) id<AutocompleteSuggestionGroup> previousPedalGroup;
/// Timer for pedal debouncing.
@property(nonatomic, strong) NSTimer* removePedalsTimer;
@property(nonatomic, strong)
    NSArray<id<AutocompleteSuggestion>>* originalResult;

@end

@implementation PedalSectionExtractor

+ (id<AutocompleteSuggestionGroup>)wrapPedals:
    (NSArray<id<OmniboxPedal, OmniboxIcon>>*)extractedPedals {
  if (extractedPedals.count == 0) {
    return nil;
  }

  NSMutableArray* wrappedPedals = [[NSMutableArray alloc] init];
  for (id<OmniboxPedal, OmniboxIcon> pedal in extractedPedals) {
    [wrappedPedals
        addObject:[[PedalSuggestionWrapper alloc] initWithPedal:pedal]];
  }

  AutocompleteSuggestionGroupImpl* pedalGroup =
      [AutocompleteSuggestionGroupImpl groupWithTitle:nil
                                          suggestions:wrappedPedals];
  return pedalGroup;
}

- (id<AutocompleteSuggestionGroup>)extractPedals:
    (NSArray<id<AutocompleteSuggestion>>*)suggestions {
  self.originalResult = suggestions;
  // Extract pedals
  NSMutableArray* extractedPedals = [[NSMutableArray alloc] init];
  for (NSUInteger i = 0; i < suggestions.count; i++) {
    id<AutocompleteSuggestion> suggestion = suggestions[i];
    if (suggestion.pedal != nil) {
      [extractedPedals addObject:suggestion.pedal];
    }
  }

  if (extractedPedals.count > 0) {
    [self.removePedalsTimer invalidate];
    self.removePedalsTimer = nil;

    id<AutocompleteSuggestionGroup> pedalGroup =
        [PedalSectionExtractor wrapPedals:extractedPedals];
    self.previousPedalGroup = pedalGroup;
    return pedalGroup;
  } else if (self.previousPedalGroup && suggestions.count > 0) {
    // If no pedals, display old pedal for a duration of `kPedalDebouceTimer`
    // with new suggestion. This avoids pedal flickering because the pedal
    // results are async. (cf. crbug.com/1316404).
    if (!self.removePedalsTimer) {
      self.removePedalsTimer = [NSTimer
          scheduledTimerWithTimeInterval:kPedalDebouceTimer
                                  target:self
                                selector:@selector(expirePreviousPedals:)
                                userInfo:nil
                                 repeats:NO];
    }
    return self.previousPedalGroup;
  } else {
    // Remove cached pedals when the popup is closed.
    if (suggestions.count == 0) {
      [self expirePreviousPedals:nil];
    }
    return nil;
  }
}

/// Removes pedals from suggestions. This is used to debouce pedal with a timer
/// to avoid pedal flickering.
- (void)expirePreviousPedals:(NSTimer*)timer {
  [self.removePedalsTimer invalidate];
  self.previousPedalGroup = nil;
  self.removePedalsTimer = nil;

  [self.delegate invalidatePedals];
}

- (BOOL)hasCachedPedals {
  return self.previousPedalGroup;
}

@end
