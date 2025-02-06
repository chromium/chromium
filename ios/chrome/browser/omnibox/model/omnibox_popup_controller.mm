// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"

#import "components/omnibox/browser/autocomplete_result.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller_delegate.h"

@implementation OmniboxPopupController

- (void)updateWithResults:(const AutocompleteResult&)results {
  [self.delegate popupController:self didUpdateResults:results];
}

@end
