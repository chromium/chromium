// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <string>

#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper_delegate.h"
#import "ui/base/window_open_disposition.h"

@protocol AutocompleteSuggestion;
struct AutocompleteMatch;
class AutocompleteResult;
@class AutocompleteResultWrapper;
@protocol OmniboxAutocompleteControllerDelegate;
@protocol OmniboxAutocompleteControllerDebuggerDelegate;
class OmniboxClient;
class OmniboxControllerIOS;
class OmniboxEditModelIOS;
@class OmniboxTextController;
struct OmniboxTextModel;

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
- (instancetype)initWithOmniboxController:
                    (OmniboxControllerIOS*)omniboxController
                            omniboxClient:(OmniboxClient*)omniboxClient
                         omniboxEditModel:(OmniboxEditModelIOS*)omniboxEditModel
                         omniboxTextModel:(OmniboxTextModel*)omniboxTextModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

/// Removes all C++ references.
- (void)disconnect;

/// Updates the popup suggestions.
- (void)updatePopupSuggestions;

/// Cancels any pending asynchronous query. If `clearSuggestions` is true, will
/// also erase the suggestions.
- (void)stopAutocompleteWithClearSuggestions:(BOOL)clearSuggestions;

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

/// Previews the given autocomplete suggestion.
- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion
            isFirstUpdate:(BOOL)isFirstUpdate;

#pragma mark - OmniboxText events

/// Starts autocomplete with `text`.
- (void)startAutocompleteWithText:(const std::u16string&)text
                   cursorPosition:(size_t)cursorPosition
        preventInlineAutocomplete:(bool)preventInlineAutocomplete;

/// Starts a request for zero-prefix suggestions if no query is currently
/// running and the popup is closed. This can be called multiple times without
/// harm, since it will early-exit if an earlier request is in progress or done.
/// `text` should either be empty or the pre-edit text.
- (void)startZeroSuggestRequestWithText:(const std::u16string&)text
                          userClobbered:(BOOL)userClobberedPermanentText;

/// Closes the omnibox popup.
- (void)closeOmniboxPopup;

/// Updates the popup text alignment.
- (void)setTextAlignment:(NSTextAlignment)alignment;

/// Updates the popup semantic content attribute.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute;

/// Notifies thumbnail update.
- (void)setHasThumbnail:(BOOL)hasThumbnail;

#pragma mark - Prefetch events

/// Starts an autocomplete prefetch request so that zero-prefix providers can
/// optionally start a prefetch request to warm up the their underlying
/// service(s) and/or optionally cache their otherwise async response.
- (void)startZeroSuggestPrefetch;

/// Informs autocomplete provider clients whether the app is currently in the
/// background.
- (void)setBackgroundStateForProviders:(BOOL)inBackground;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
