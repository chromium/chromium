// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_

#import <Foundation/Foundation.h>

class AutocompleteResult;
@class OmniboxAutocompleteController;
@protocol OmniboxPopupControllerDelegate;

/// Controller for the omnibox popup.
@interface OmniboxPopupController : NSObject

/// Delegate of the omnibox popup controller.
@property(nonatomic, weak) id<OmniboxPopupControllerDelegate> delegate;

/// Controller of autocomplete.
@property(nonatomic, weak)
    OmniboxAutocompleteController* omniboxAutocompleteController;

#pragma mark - OmniboxAutocomplete event

/// Notifies the popup that new results are available.
/// `isOnFocus`: Whether the omnibox is being focused.
- (void)newResultsAvailable:(const AutocompleteResult&)results
                  isOnFocus:(BOOL)isOnFocus;

/// Updates the omnibox popup with sorted`result`.
- (void)updateWithSortedResults:(const AutocompleteResult&)results;

#pragma mark - OmniboxPopup event

/// Request suggestions with a number of visible suggestions.
- (void)requestSuggestionsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
