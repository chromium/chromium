// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_DEBUGGER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_DEBUGGER_DELEGATE_H_

@class OmniboxAutocompleteController;

// Delegate for debugging autocomplete events in the omnibox.
@protocol OmniboxAutocompleteControllerDebuggerDelegate

/// Notifies the delegate of changes to suggestion availability.
- (void)omniboxAutocompleteController:
            (OmniboxAutocompleteController*)autocompleteController
    didUpdateWithSuggestionsAvailable:(BOOL)hasSuggestions;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_DEBUGGER_DELEGATE_H_
