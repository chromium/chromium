// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_MUTATOR_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_MUTATOR_H_

@protocol AutocompleteSuggestion;
@class SuggestAction;

// The omnibox popup mutator
@protocol OmniboxPopupMutator <NSObject>

/// Notify about a size change.
- (void)onTraitCollectionChange;

/// Tells the mutator on scroll.
- (void)onScroll;

/// Tells the mutator when `suggestion` in `row` was selected.
- (void)selectSuggestion:(id<AutocompleteSuggestion>)suggestion
                   inRow:(NSUInteger)row;

/// Tells the mutator when a `suggestion`'s `action` was selected in a given
/// row index, for example "Directions" button for a local entity suggestion.
- (void)selectSuggestionAction:(SuggestAction*)action
                    suggestion:(id<AutocompleteSuggestion>)suggestion
                         inRow:(NSUInteger)row;

/// Tells the mutator when `suggestion` in `row` was chosen for appending to
/// omnibox.
- (void)tapTrailingButtonOnSuggestion:(id<AutocompleteSuggestion>)suggestion
                                inRow:(NSUInteger)row;

/// Tells the mutator when `suggestion` in `row` was removed.
- (void)selectSuggestionForDeletion:(id<AutocompleteSuggestion>)suggestion
                              inRow:(NSUInteger)row;

/// Informs the mutator that the close button was pressed.
- (void)closeButtonTapped;

/// Request suggestions from the data source.
/// `n` is the number of suggestions that are considered visible. Meaning the
/// user doesn't have to scroll or hide the keyboard to see those `n` first
/// suggestions.
- (void)requestResultsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount
    __attribute__((swift_name("requestResults(visibleSuggestionCount:)")));

/// Notifies the mutator of the suggestion to preview.
/// `suggestion` can be nil e.g. when there is no highlighting in the popup.
/// `isFirstUpdate` flag is set when this is the first suggestion for a new set
/// of results. This may be used to display the suggestion in a different way,
/// e.g. as inline autocomplete.
- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion
            isFirstUpdate:(BOOL)isFirstUpdate;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_MUTATOR_H_
