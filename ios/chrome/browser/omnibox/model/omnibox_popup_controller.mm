// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"

#import "components/omnibox/browser/autocomplete_result.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller_delegate.h"

@implementation OmniboxPopupController

#pragma mark - OmniboxAutocomplete event

- (void)newResultsAvailable:(const AutocompleteResult&)results
                  isOnFocus:(BOOL)isOnFocus {
  [self.delegate popupControllerDidUpdateSuggestions:self
                                      hasSuggestions:!results.empty()
                                           isOnFocus:isOnFocus];
}

- (void)updateWithSortedResults:(const AutocompleteResult&)results {
  [self.delegate popupController:self didSortResults:results];
}

#pragma mark - OmniboxPopup event

- (void)requestSuggestionsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  [self.omniboxAutocompleteController
      requestResultsWithVisibleSuggestionCount:visibleSuggestionCount];
}

- (BOOL)isStarredMatch:(const AutocompleteMatch&)match {
  return [self.omniboxAutocompleteController isStarredMatch:match];
}

- (void)selectMatchForOpening:(const AutocompleteMatch&)match
                        inRow:(NSUInteger)row
                       openIn:(WindowOpenDisposition)disposition {
  [self.omniboxAutocompleteController selectMatchForOpening:match
                                                      inRow:row
                                                     openIn:disposition];
}

- (void)selectMatchForAppending:(const AutocompleteMatch&)match {
  [self.omniboxAutocompleteController selectMatchForAppending:match];
}

- (void)selectMatchForDeletion:(const AutocompleteMatch&)match {
  [self.omniboxAutocompleteController selectMatchForDeletion:match];
}

- (void)onScroll {
  [self.omniboxAutocompleteController onScroll];
}

- (void)onCallAction {
  [self.omniboxAutocompleteController onCallAction];
}

@end
