// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper_delegate.h"
#import "ui/base/window_open_disposition.h"

struct AutocompleteMatch;
class AutocompleteResult;
@class AutocompleteResultWrapper;
class OmniboxController;
@protocol OmniboxAutocompleteControllerDelegate;
@protocol OmniboxAutocompleteControllerDebuggerDelegate;
@class OmniboxTextController;
class OmniboxViewIOS;

/// Controller for the omnibox autocomplete system. Handles interactions with
/// the autocomplete system and dispatches results.
@interface OmniboxAutocompleteController
    : NSObject <AutocompleteResultWrapperDelegate>

/// Delegate of the omnibox autocomplete controller.
@property(nonatomic, weak) id<OmniboxAutocompleteControllerDelegate> delegate;

/// Debugger delegate of the omnibox autocomplete controller.
@property(nonatomic, weak) id<OmniboxAutocompleteControllerDebuggerDelegate>
    debuggerDelegate;

/// Autcomplete result wrapper.
@property(nonatomic, strong)
    AutocompleteResultWrapper* autocompleteResultWrapper;

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

#pragma mark - OmniboxText events

/// Updates the popup text alignment.
- (void)setTextAlignment:(NSTextAlignment)alignment;

/// Updates the popup semantic content attribute.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute;

/// Notifies thumbnail update.
- (void)setHasThumbnail:(BOOL)hasThumbnail;

#pragma mark - OmniboxAutocomplete event

/// Updates the omnibox popup with sorted`result`.
- (void)updateWithSortedResults:(const AutocompleteResult&)results;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
