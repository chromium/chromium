// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;
@protocol AutocompleteSuggestionGroup;
@protocol OmniboxPopupConsumer;

@class SuggestAction;

/// Delegate for AutocompleteResultConsumer.
@protocol AutocompleteResultConsumerDelegate <NSObject>

/// Notify about a size change.
- (void)autocompleteResultConsumerDidChangeTraitCollection:
    (id<OmniboxPopupConsumer>)sender;

/// Tells the delegate on scroll.
- (void)autocompleteResultConsumerDidScroll:(id<OmniboxPopupConsumer>)sender;

/// Tells the delegate when `suggestion` in `row` was selected.
- (void)omniboxPopupConsumer:(id<OmniboxPopupConsumer>)sender
         didSelectSuggestion:(id<AutocompleteSuggestion>)suggestion
                       inRow:(NSUInteger)row;

/// Tells the delegate when a `suggestion`'s `action` was selected in a given
/// row index, for example "Directions" button for a local entity suggestion.
- (void)omniboxPopupConsumer:(id<OmniboxPopupConsumer>)sender
    didSelectSuggestionAction:(SuggestAction*)action
                   suggestion:(id<AutocompleteSuggestion>)suggestion
                        inRow:(NSUInteger)row;

/// Tells the delegate when `suggestion` in `row` was chosen for appending to
/// omnibox.
- (void)omniboxPopupConsumer:(id<OmniboxPopupConsumer>)sender
    didTapTrailingButtonOnSuggestion:(id<AutocompleteSuggestion>)suggestion
                               inRow:(NSUInteger)row;

/// Tells the delegate when `suggestion` in `row` was removed.
- (void)omniboxPopupConsumer:(id<OmniboxPopupConsumer>)sender
    didSelectSuggestionForDeletion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row;

@end

/// An abstract data source for autocomplete results.
@protocol AutocompleteResultDataSource <NSObject>

/// Request suggestions from the data source.
/// `n` is the number of suggestions that are considered visible. Meaning the
/// user doesn't have to scroll or hide the keyboard to see those `n` first
/// suggestions.
- (void)requestResultsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount
    __attribute__((swift_name("requestResults(visibleSuggestionCount:)")));
;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_BUNDLED_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_
