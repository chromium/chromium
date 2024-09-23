// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestion;
@protocol AutocompleteSuggestionGroup;
@protocol AutocompleteResultConsumer;

@class SuggestAction;

/// Delegate for AutocompleteResultConsumer.
@protocol AutocompleteResultConsumerDelegate <NSObject>

/// Notify about a size change.
- (void)autocompleteResultConsumerDidChangeTraitCollection:
    (id<AutocompleteResultConsumer>)sender;

/// Tells the delegate on scroll.
- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender;

/// Tells the delegate when `suggestion` in `row` was selected.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
               didSelectSuggestion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row;

/// Tells the delegate when a `suggestion`'s `action` was selected in a given
/// row index, for example "Directions" button for a local entity suggestion.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
         didSelectSuggestionAction:(SuggestAction*)action
                        suggestion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row;

/// Tells the delegate when `suggestion` in `row` was chosen for appending to
/// omnibox.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
    didTapTrailingButtonOnSuggestion:(id<AutocompleteSuggestion>)suggestion
                               inRow:(NSUInteger)row;

/// Tells the delegate when `suggestion` in `row` was removed.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
    didSelectSuggestionForDeletion:(id<AutocompleteSuggestion>)suggestion
                             inRow:(NSUInteger)row;

@end

/// An abstract consumer of autocomplete results.
@protocol AutocompleteResultConsumer <NSObject>
/// Updates the current data and forces a redraw. If animation is YES, adds
/// CALayer animations to fade the OmniboxPopupRows in.
/// `preselectedMatchGroupIndex` is the section selected by default when no row
/// is highlighted.
- (void)updateMatches:(NSArray<id<AutocompleteSuggestionGroup>>*)result
    preselectedMatchGroupIndex:(NSInteger)groupIndex;

/// Sets the text alignment of the popup content.
- (void)setTextAlignment:(NSTextAlignment)alignment;
/// Sets the semantic content attribute of the popup content.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute;

/// Informs consumer that new result are available. Consumer can request new
/// results from its data source `AutocompleteResultDataSource`.
- (void)newResultsAvailable;

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

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_
