// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"

#import "components/omnibox/browser/autocomplete_result.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller_delegate.h"
#import "ios/chrome/browser/omnibox/ui_bundled/popup/autocomplete_suggestion.h"

@interface OmniboxPopupController ()
// Redefine as readwrite.
@property(nonatomic, assign, readwrite) BOOL hasSuggestions;
@end

@implementation OmniboxPopupController

- (void)disconnect {
  [self.autocompleteResultWrapper disconnect];
}

#pragma mark - OmniboxAutocomplete event

- (void)newResultsAvailable:(const AutocompleteResult&)results
                 isFocusing:(BOOL)isFocusing {
  BOOL hasSuggestions = !results.empty();
  self.hasSuggestions = hasSuggestions;
  [self.delegate popupControllerDidUpdateSuggestions:self
                                      hasSuggestions:hasSuggestions
                                          isFocusing:isFocusing];
}

- (void)updateWithSortedResults:(const AutocompleteResult&)results {
  NSArray<id<AutocompleteSuggestionGroup>>* suggestionGroups =
      [self.autocompleteResultWrapper wrapAutocompleteResultInGroups:results];
  [self.delegate popupController:self
      didUpdateSuggestionsGroups:suggestionGroups];
}

#pragma mark - OmniboxPopup event

- (void)requestSuggestionsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  [self.omniboxAutocompleteController
      requestResultsWithVisibleSuggestionCount:visibleSuggestionCount];
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

#pragma mark - OmniboxText events

- (void)setTextAlignment:(NSTextAlignment)alignment {
  [self.delegate popupController:self didUpdateTextAlignment:alignment];
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [self.delegate popupController:self
      didUpdateSemanticContentAttribute:semanticContentAttribute];
}

- (void)setHasThumbnail:(BOOL)hasThumbnail {
  [self.delegate popupController:self didUpdateHasThumbnail:hasThumbnail];
  self.autocompleteResultWrapper.hasThumbnail = hasThumbnail;
}

#pragma mark - AutocompleteResultWrapperDelegate

// TODO(crbug.com/400626674): Move isStarredMatch logic to the wrapper so it
// doesn't rely on its delegate.
- (BOOL)isStarredMatch:(const AutocompleteMatch&)match {
  return [self.omniboxAutocompleteController isStarredMatch:match];
}

- (void)autocompleteResultWrapper:(AutocompleteResultWrapper*)wrapper
              didInvalidatePedals:(NSArray<id<AutocompleteSuggestionGroup>>*)
                                      nonPedalSuggestionsGroups {
  [self.delegate popupController:self
             didInvalidatePedals:nonPedalSuggestionsGroups];
}

@end
