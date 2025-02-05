// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"

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
    [self.omniboxPopupController
        updateWithResults:_autocompleteController->result()];
  }
}

@end
