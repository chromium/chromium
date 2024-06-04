// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_

#import <UIKit/UIKit.h>

@protocol OmniboxIcon;
@protocol OmniboxPedal;
@class CrURL;
@class SuggestAction;

/// Copy of `SuggestTileType` enum in histograms.
typedef NS_ENUM(NSUInteger, SuggestTileType) {
  kOther = 0,
  kURL = 1,
  kSearch = 2,
  kCount = 3
};

/// Represents an autocomplete suggestion in UI.
@protocol AutocompleteSuggestion <NSObject>
/// Some suggestions can be deleted with a swipe-to-delete gesture.
@property(nonatomic, readonly) BOOL supportsDeletion;
/// Some suggestions are answers that are displayed inline, such as for weather
/// or calculator.
@property(nonatomic, readonly) BOOL hasAnswer;
/// Some suggestions represent a URL, for example the ones from history.
@property(nonatomic, readonly) BOOL isURL;
/// Some suggestions can be appended to omnibox text in order to refine the
/// query or URL.
@property(nonatomic, readonly) BOOL isAppendable;
/// Some suggestions are opened in an other tab.
@property(nonatomic, readonly) BOOL isTabMatch;
/// Text of the suggestion.
@property(nonatomic, readonly) NSAttributedString* text;
/// Second line of text.
@property(nonatomic, readonly) NSAttributedString* detailText;
/// Suggested number of lines to format `detailText`.
@property(nonatomic, readonly) NSInteger numberOfLines;

/// Either nil or NSNumber-wrapped omnibox::GroupId.
@property(nonatomic, readonly, strong) NSNumber* suggestionGroupId;
/// Either nil or NSNumber-wrapped omnibox::GroupSection.
@property(nonatomic, readonly, strong) NSNumber* suggestionSectionId;

/// Text to use in the omnibox when the suggestion is highlighted.
/// Effectively an accessor for fill_into_edit.
@property(nonatomic, readonly) NSAttributedString* omniboxPreviewText;

@property(nonatomic, readonly) id<OmniboxIcon> icon;

@property(nonatomic, readonly) id<OmniboxPedal, OmniboxIcon> pedal;

/// Icon corresponding to the suggestion's autocomplete match type, e.g.
/// History, Search, or Stock.
/// Ignores `starred` status of the suggestion.
@property(nonatomic, readonly) UIImage* matchTypeIcon;
/// Accessibility identifier of the icon corresponding to the suggestion's
/// autocomplete match type.
@property(nonatomic, readonly) NSString* matchTypeIconAccessibilityIdentifier;
/// Whether this is a search suggestion (as opposed to URL suggestion)
@property(nonatomic, readonly, getter=isMatchTypeSearch) BOOL matchTypeSearch;
/// Whether the text of the suggestion can wrap on multiple lines.
@property(nonatomic, readonly) BOOL isWrapping;
/// For URL suggestions, the URL that the match represents.
@property(nonatomic, readonly) CrURL* destinationUrl;
/// Suggestion attached actions in suggest.
@property(nonatomic, readonly) NSArray<SuggestAction*>* actionsInSuggest;

#pragma mark tail suggest

/// Yes if this is a tail suggestion. Used by the popup to display according to
/// tail suggest standards.
@property(nonatomic, readonly) BOOL isTailSuggestion;

/// Common prefix for tail suggestions. Empty otherwise.
@property(nonatomic, readonly, copy) NSString* commonPrefix;

@end

typedef NS_ENUM(NSUInteger, SuggestionGroupDisplayStyle) {
  SuggestionGroupDisplayStyleDefault,   // Vertical list.
  SuggestionGroupDisplayStyleCarousel,  // Horizontal scrolling icons.
};

/// A group of AutocompleteSuggestions with an optional section header.
@protocol AutocompleteSuggestionGroup

/// Optional title.
@property(nonatomic, copy, readonly) NSString* title;

/// Contained suggestions.
@property(nonatomic, strong, readonly)
    NSArray<id<AutocompleteSuggestion>>* suggestions;

/// How suggestion are displayed.
@property(nonatomic, readonly) SuggestionGroupDisplayStyle displayStyle;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_
