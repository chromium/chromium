// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_CONSUMER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_CONSUMER_H_

@protocol AutocompleteSuggestionGroup;

// The omnibox popup consumer.
@protocol OmniboxPopupConsumer <NSObject>

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

#endif  // IOS_CHROME_BROWSER_OMNIBOX_UI_POPUP_OMNIBOX_POPUP_CONSUMER_H_
