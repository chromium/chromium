// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"
#import "third_party/omnibox_proto/groups.pb.h"

@implementation OmniboxAutocompleteController {
  /// Controller of the omnibox.
  raw_ptr<OmniboxController> _omniboxController;
  /// Controller of autocomplete.
  raw_ptr<AutocompleteController> _autocompleteController;
}

- (instancetype)initWithOmniboxController:
    (OmniboxController*)omniboxController {
  self = [super init];
  if (self) {
    _omniboxController = omniboxController;
    _autocompleteController = omniboxController->autocomplete_controller();
  }
  return self;
}

- (void)disconnect {
  _autocompleteController = nullptr;
  _omniboxController = nullptr;
}

#pragma mark - OmniboxEditModel event

- (void)updatePopupSuggestions {
  if (_autocompleteController) {
    BOOL isFocusing = _autocompleteController->input().focus_type() ==
                      metrics::OmniboxFocusType::INTERACTION_FOCUS;
    [self.omniboxPopupController
        newResultsAvailable:_autocompleteController->result()
                  isOnFocus:isFocusing];
  }
}

#pragma mark - OmniboxPopup event

- (void)requestResultsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  if (!_autocompleteController) {
    return;
  }
  size_t resultSize = _autocompleteController->result().size();
  // If no suggestions are visible, consider all of them visible.
  if (visibleSuggestionCount == 0) {
    visibleSuggestionCount = resultSize;
  }
  NSUInteger visibleSuggestions = MIN(visibleSuggestionCount, resultSize);
  if (visibleSuggestions > 0) {
    // Groups visible suggestions by search vs url. Skip the first suggestion
    // because it's the omnibox content.
    _autocompleteController->GroupSuggestionsBySearchVsURL(1,
                                                           visibleSuggestions);
  }
  // Groups hidden suggestions by search vs url.
  if (visibleSuggestions < resultSize) {
    _autocompleteController->GroupSuggestionsBySearchVsURL(visibleSuggestions,
                                                           resultSize);
  }

  [self.omniboxPopupController
      updateWithSortedResults:_autocompleteController->result()];
}

@end
