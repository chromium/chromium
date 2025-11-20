// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_AUTOCOMPLETE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <string>

#import "base/time/time.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_result_wrapper_delegate.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ui/base/window_open_disposition.h"

@protocol AutocompleteSuggestion;
struct AutocompleteMatch;
class AutocompleteController;
class AutocompleteInput;
class AutocompleteResult;
@class AutocompleteResultWrapper;
class GURL;
@protocol OmniboxAutocompleteControllerDelegate;
@protocol OmniboxAutocompleteControllerDebuggerDelegate;
@protocol OmniboxLensDelegate;
class OmniboxClient;
@class OmniboxMetricsRecorder;
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

/// Handler for Lens interactions.
@property(nonatomic, weak) id<OmniboxLensDelegate> lensHander;

/// Autcomplete result wrapper.
@property(nonatomic, strong)
    AutocompleteResultWrapper* autocompleteResultWrapper;

/// Controller of the omnibox text.
@property(nonatomic, weak) OmniboxTextController* omniboxTextController;

/// Metrics recorder.
@property(nonatomic, weak) OmniboxMetricsRecorder* omniboxMetricsRecorder;

// Whether or not the popup has suggestions.
@property(nonatomic, assign, readonly) BOOL hasSuggestions;

// Returns the autocomplete provider client that's used by the internal
// autocomplete controller.
@property(nonatomic, assign, readonly)
    AutocompleteProviderClient* autocompleteProviderClient;

- (instancetype)
     initWithOmniboxClient:(OmniboxClient*)omniboxClient
    autocompleteController:(AutocompleteController*)autocompleteController
          omniboxTextModel:(OmniboxTextModel*)omniboxTextModel
       presentationContext:(OmniboxPresentationContext)presentationContext
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/// Returns the underlying autocomplete controller.
- (AutocompleteController*)autocompleteController;

/// Removes all C++ references.
- (void)disconnect;

/// Updates the popup suggestions.
- (void)updatePopupSuggestions;

/// Cancels any pending asynchronous query. If `clearSuggestions` is true, will
/// also erase the suggestions.
- (void)stopAutocompleteWithClearSuggestions:(BOOL)clearSuggestions;

/// Opens given selection. Most kinds of selection invoke an action or
/// otherwise call `openMatch`, but some may `acceptInputWithDisposition` which
/// is not guaranteed to open a match or commit the omnibox.
- (void)openSelection:(OmniboxPopupSelection)selection
            timestamp:(base::TimeTicks)timestamp
          disposition:(WindowOpenDisposition)disposition;

/// Opens the provided autocomplete `match` using a `customDestinationURL`.
/// This method handles the logic for navigating to the specified URL.
/// It is used when the user explicitly chooses to open a match with a URL
/// different from its default destination.
- (void)selectMatchForOpening:(AutocompleteMatch&)match
     withCustomDestinationURL:(GURL)destinationURL
                        inRow:(NSUInteger)row
                       openIn:(WindowOpenDisposition)disposition;

/// A simplified version of OpenSelection that opens the model's current
/// selection.
- (void)openCurrentSelectionWithDisposition:(WindowOpenDisposition)disposition
                                  timestamp:(base::TimeTicks)timestamp;

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

/// Clears the omnibox suggestions and starts autocomplete with the current
/// input text.
- (void)clearAndRestartAutocomplete;

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

/// Called when a new omnibox session starts.
- (void)resetSession;

/// If a query is active or the popup is visible, find the best match item
/// (default or selected). This will update `match` and, if found,
/// `alternateNavigationURL`. Returns whether a match has been found.
- (BOOL)findMatchForInput:(const AutocompleteInput&)input
                     match:(AutocompleteMatch*)match
    alternateNavigationURL:(GURL*)alternateNavigationURL;

/// Computes the alternate navigation URL for `input` and `match`.
- (GURL)computeAlternateNavURLForInput:(const AutocompleteInput&)input
                                 match:(const AutocompleteMatch&)match;

/// Closes the omnibox popup.
- (void)closeOmniboxPopup;

/// Updates the popup text alignment.
- (void)setTextAlignment:(NSTextAlignment)alignment;

/// Updates the popup semantic content attribute.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute;

/// Notifies thumbnail update.
- (void)setHasThumbnail:(BOOL)hasThumbnail;

/// Returns the autocomplete result. This is used to forward the result to the
/// client. TODO(crbug.com/432215477): Remove after refactor.
- (const AutocompleteResult*)autocompleteResult;

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
