// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"

#import "components/omnibox/browser/autocomplete_result.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller_delegate.h"

@implementation OmniboxPopupController

#pragma mark - OmniboxAutocomplete event

- (void)updateWithResults:(const AutocompleteResult&)results {
  [self.delegate popupController:self didUpdateResults:results];
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

@end
