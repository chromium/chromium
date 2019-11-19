// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_

#import <UIKit/UIKit.h>

class GURL;
@protocol OmniboxIcon;

// Represents an autocomplete suggestion in UI.
@protocol AutocompleteSuggestion <NSObject>
// Some suggestions can be deleted with a swipe-to-delete gesture.
- (BOOL)supportsDeletion;
// Some suggestions are answers that are displayed inline, such as for weather
// or calculator.
- (BOOL)hasAnswer;
// Some suggestions represent a URL, for example the ones from history.
- (BOOL)isURL;
// Some suggestions can be appended to omnibox text in order to refine the
// query or URL.
- (BOOL)isAppendable;
// The leading image for this suggestion type (loupe, globe, etc). The returned
// image is in template rendering mode, it is expected to be tinted by the image
// view.
- (UIImage*)suggestionTypeIcon;
// Some suggestions are opened in an other tab.
- (BOOL)isTabMatch;

// Text of the suggestion.
- (NSAttributedString*)text;
// Second line of text.
- (NSAttributedString*)detailText;
// Suggested number of lines to format |detailText|.
- (NSInteger)numberOfLines;

// Wether the suggestion has a downloadable image.
- (BOOL)hasImage;
// URL of the image, if |hasImage| is true.
- (GURL)imageURL;
// Page URL to be used to retrieve the favicon.
- (GURL)faviconPageURL;

- (id<OmniboxIcon>)icon;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_AUTOCOMPLETE_SUGGESTION_H_
