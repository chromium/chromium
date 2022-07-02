// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol AutocompleteSuggestionGroup;

@protocol AutocompleteResultConsumer;

// Delegate for AutocompleteResultConsumer.
@protocol AutocompleteResultConsumerDelegate <NSObject>

// Tells the delegate when a row containing a suggestion is highlighted (i.e.
// with arrow keys).
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                   didHighlightRow:(NSUInteger)row
                         inSection:(NSUInteger)section;

// Highlighting has been cancelled, no row is highlighted.
- (void)autocompleteResultConsumerCancelledHighlighting:
    (id<AutocompleteResultConsumer>)sender;

// Tells the delegate when a row containing a suggestion is clicked.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                      didSelectRow:(NSUInteger)row
                         inSection:(NSUInteger)section;
// Tells the delegate when a suggestion in `row` was chosen for appending to
// omnibox.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
        didTapTrailingButtonForRow:(NSUInteger)row
                         inSection:(NSUInteger)section;
// Tells the delegate when a suggestion in `row` was removed.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
           didSelectRowForDeletion:(NSUInteger)row
                         inSection:(NSUInteger)section;
// Tells the delegate on scroll.
- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender;

@end

// An abstract consumer of autocomplete results.
@protocol AutocompleteResultConsumer <NSObject>
// Updates the current data and forces a redraw. If animation is YES, adds
// CALayer animations to fade the OmniboxPopupRows in.
// `preselectedMatchGroupIndex` is the section selected by default when no row
// is highlighted.
- (void)updateMatches:(NSArray<id<AutocompleteSuggestionGroup>>*)result
    preselectedMatchGroupIndex:(NSInteger)groupIndex;

// Sets the text alignment of the popup content.
- (void)setTextAlignment:(NSTextAlignment)alignment;
// Sets the semantic content attribute of the popup content.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute;

// Informs consumer that new result are available. Consumer can request new
// results from its data source `AutocompleteResultDataSource`.
- (void)newResultsAvailable;

@end

// An abstract data source for autocomplete results.
@protocol AutocompleteResultDataSource <NSObject>

// Request suggestions from the data source.
// `n` is the number of suggestions that are considered visible. Meaning the
// user doesn't have to scroll or hide the keyboard to see those `n` first
// suggestions.
- (void)requestResultsWithVisibleSuggestionCount:
    (NSInteger)visibleSuggestionCount
    __attribute__((swift_name("requestResults(visibleSuggestionCount:)")));
;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_RESULT_CONSUMER_H_
