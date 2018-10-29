// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_AUTOCOMPLETE_RESULT_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_AUTOCOMPLETE_RESULT_CONSUMER_H_

#import "ios/chrome/browser/ui/omnibox/autocomplete_suggestion.h"

@protocol AutocompleteResultConsumer;

// Delegate for AutocompleteResultConsumer.
@protocol AutocompleteResultConsumerDelegate<NSObject>

// Tells the delegate when a row containing a suggestion is highlighted (i.e.
// with arrow keys).
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                   didHighlightRow:(NSUInteger)row;
// Tells the delegate when a row containing a suggestion is clicked.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                      didSelectRow:(NSUInteger)row;
// Tells the delegate when a suggestion in|row| was chosen for appending to
// omnibox.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
        didTapTrailingButtonForRow:(NSUInteger)row;
// Tells the delegate when a suggestion in |row| was removed.
- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
           didSelectRowForDeletion:(NSUInteger)row;
// Tells the delegate on scroll.
- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender;

@end

// An abstract consumer of autocomplete results.
@protocol AutocompleteResultConsumer<NSObject>
// Updates the current data and forces a redraw. If animation is YES, adds
// CALayer animations to fade the OmniboxPopupRows in.
- (void)updateMatches:(NSArray<id<AutocompleteSuggestion>>*)result
        withAnimation:(BOOL)animation;
// Sets the text alignment of the popup content.
- (void)setTextAlignment:(NSTextAlignment)alignment;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_AUTOCOMPLETE_RESULT_CONSUMER_H_
