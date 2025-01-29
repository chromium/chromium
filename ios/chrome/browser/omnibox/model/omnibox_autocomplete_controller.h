// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_

#import <Foundation/Foundation.h>

class AutocompleteResult;
@class OmniboxPopupController;

/// Controller for the omnibox autocomplete system. Handles interactions with
/// the autocomplete system and dispatches results to the OmniboxTextController
/// and OmniboxPopupController.
@interface OmniboxAutocompleteController : NSObject

/// Controller of the omnibox popup.
@property(nonatomic, weak) OmniboxPopupController* omniboxPopupController;

// Reaction to events from OmniboxEditModel.
#pragma mark - OmniboxEditModel delegate

/// Updates the omnibox with `results`.
- (void)updateWithResults:(const AutocompleteResult&)result;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
