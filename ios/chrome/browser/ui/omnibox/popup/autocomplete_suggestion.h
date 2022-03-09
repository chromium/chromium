// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_

#import <UIKit/UIKit.h>

@protocol OmniboxIcon;

// Represents an autocomplete suggestion in UI.
@protocol AutocompleteSuggestion <NSObject>
// Some suggestions can be deleted with a swipe-to-delete gesture.
@property(nonatomic, readonly) BOOL supportsDeletion;
// Some suggestions are answers that are displayed inline, such as for weather
// or calculator.
@property(nonatomic, readonly) BOOL hasAnswer;
// Some suggestions represent a URL, for example the ones from history.
@property(nonatomic, readonly) BOOL isURL;
// Some suggestions can be appended to omnibox text in order to refine the
// query or URL.
@property(nonatomic, readonly) BOOL isAppendable;
// Some suggestions are opened in an other tab.
@property(nonatomic, readonly) BOOL isTabMatch;

// Text of the suggestion.
@property(nonatomic, readonly) NSAttributedString* text;
// Second line of text.
@property(nonatomic, readonly) NSAttributedString* detailText;
// Suggested number of lines to format |detailText|.
@property(nonatomic, readonly) NSInteger numberOfLines;

@property(nonatomic, readonly) id<OmniboxIcon> icon;

#pragma mark tail suggest

// Yes if this is a tail suggestion. Used by the popup to display according to
// tail suggest standards.
@property(nonatomic, readonly) BOOL isTailSuggestion;

// Common prefix for tail suggestions. Empty otherwise.
@property(nonatomic, readonly) NSString* commonPrefix;

@end

// A group of AutocompleteSuggestions with an optional section header.
@protocol AutocompleteSuggestionGroup

// Optional title.
@property(nonatomic, copy, readonly) NSString* title;

// Contained suggestions.
@property(nonatomic, strong, readonly)
    NSArray<id<AutocompleteSuggestion>>* suggestions;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_
