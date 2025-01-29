// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"

@implementation OmniboxAutocompleteController

#pragma mark - OmniboxEditModel event

- (void)updateWithResults:(const AutocompleteResult&)result {
  [self.omniboxPopupController updateWithResults:result];
}

@end
