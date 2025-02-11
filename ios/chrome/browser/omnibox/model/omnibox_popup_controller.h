// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ui/base/window_open_disposition.h"

struct AutocompleteMatch;
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

/// Whether the popup currently has suggestions.
@property(nonatomic, assign, readonly) BOOL hasSuggestions;

#pragma mark - OmniboxAutocomplete event

/// Notifies the popup that new results are available.
/// `isFocusing`: Whether the omnibox is being focused.
- (void)newResultsAvailable:(const AutocompleteResult&)results
                 isFocusing:(BOOL)isFocusing;

/// Updates the omnibox popup with sorted`result`.
- (void)updateWithSortedResults:(const AutocompleteResult&)results;

#pragma mark - OmniboxPopup event

/// Request suggestions with a number of visible suggestions.
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

// OmniboxText events should only contain events that don't impact autocomplete.
#pragma mark - OmniboxText events

/// Updates the popup text alignment.
- (void)setTextAlignment:(NSTextAlignment)alignment;

/// Updates the popup semantic content attribute.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute;

/// Notifies thumbnail update.
- (void)setHasThumbnail:(BOOL)hasThumbnail;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_POPUP_CONTROLLER_H_
