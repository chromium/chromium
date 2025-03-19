// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ui/base/window_open_disposition.h"

struct AutocompleteMatch;
class AutocompleteResult;
class OmniboxController;
@protocol OmniboxAutocompleteControllerDelegate;
@class OmniboxTextController;
@class OmniboxPopupController;
class OmniboxViewIOS;

/// Controller for the omnibox autocomplete system. Handles interactions with
/// the autocomplete system and dispatches results to the OmniboxTextController
/// and OmniboxPopupController.
@interface OmniboxAutocompleteController : NSObject

/// Controller of the omnibox popup.
@property(nonatomic, weak) OmniboxPopupController* omniboxPopupController;

/// Delegate of the omnibox autocomplete controller.
@property(nonatomic, weak) id<OmniboxAutocompleteControllerDelegate> delegate;

/// Controller of the omnibox text.
@property(nonatomic, weak) OmniboxTextController* omniboxTextController;

// Whether or not the popup has suggestions.
@property(nonatomic, assign, readonly) BOOL hasSuggestions;

/// Initializes with an OmniboxController.
- (instancetype)initWithOmniboxController:(OmniboxController*)omniboxController
                           omniboxViewIOS:(OmniboxViewIOS*)omniboxViewIOS
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Removes all C++ references.
- (void)disconnect;

#pragma mark - OmniboxEditModel event

/// Updates the popup suggestions.
- (void)updatePopupSuggestions;

#pragma mark - OmniboxPopup event

/// Request suggestions for a number of visible suggestions.
- (void)requestSuggestionsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount;

/// Whether `match` is a starred/bookmarked match.
- (BOOL)isStarredMatch:(const AutocompleteMatch&)match;

/// Selects `match` for opening.
- (void)selectMatchForOpening:(const AutocompleteMatch&)match
                        inRow:(NSUInteger)row
                       openIn:(WindowOpenDisposition)disposition;

/// Selects `match` for appending.
- (void)selectMatchForAppending:(const AutocompleteMatch&)match;

/// Deletes `match`.
- (void)selectMatchForDeletion:(const AutocompleteMatch&)match;

/// Notifies of scroll event.
- (void)onScroll;

/// Notifies of call action.
- (void)onCallAction;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
