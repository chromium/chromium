// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_DELEGATE_H_

@class OmniboxAutocompleteController;

// The delegate for the OmniboxAutocompleteController.
@protocol OmniboxAutocompleteControllerDelegate

/// Notifies the delegate the new suggestions are available.
- (void)omniboxAutocompleteControllerDidUpdateSuggestions:
            (OmniboxAutocompleteController*)autocompleteController
                                           hasSuggestions:(BOOL)hasSuggestions
                                               isFocusing:(BOOL)isFocusing;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_DELEGATE_H_
