// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_

#import <Foundation/Foundation.h>

class AutocompleteResult;
class OmniboxController;
@class OmniboxPopupController;

/// Controller for the omnibox autocomplete system. Handles interactions with
/// the autocomplete system and dispatches results to the OmniboxTextController
/// and OmniboxPopupController.
@interface OmniboxAutocompleteController : NSObject

/// Controller of the omnibox popup.
@property(nonatomic, weak) OmniboxPopupController* omniboxPopupController;

/// Initializes with an OmniboxController.
- (instancetype)initWithOmniboxController:(OmniboxController*)omniboxController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Removes all C++ references.
- (void)disconnect;

#pragma mark - OmniboxEditModel event

/// Updates the popup suggestions.
- (void)updatePopupSuggestions;

#pragma mark - OmniboxPopup event

/// Request suggestions for a number of visible suggestions.
- (void)requestResultsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
