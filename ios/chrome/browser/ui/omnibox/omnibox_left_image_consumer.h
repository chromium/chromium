// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_LEFT_IMAGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_LEFT_IMAGE_CONSUMER_H_

#import <UIKit/UIKit.h>

#include "base/optional.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/suggestion_answer.h"

// Describes an object that accepts a left image for the omnibox. The left image
// is used for showing the current selected suggestion icon, when the
// suggestions popup is visible.
@protocol OmniboxLeftImageConsumer

// The suggestion icon can either be determined by |matchType|, or, in new UI,
// answer icons will be used instead, if available (i.e. the match is an
// answer). Favicons are only used for non-search match types.
- (void)setLeftImageForAutocompleteType:(AutocompleteMatchType::Type)matchType
                             answerType:
                                 (base::Optional<SuggestionAnswer::AnswerType>)
                                     answerType
                             faviconURL:(GURL)faviconURL;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_LEFT_IMAGE_CONSUMER_H_
