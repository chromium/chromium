// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_PEDAL_SUGGESTION_WRAPPER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_PEDAL_SUGGESTION_WRAPPER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/model/suggestions/omnibox_pedal.h"

/// A wrapper for an OmniboxPedal that exposes AutocompleteSuggestion-compatible
/// API. Used to display pedals as suggestions in the popup.
@interface PedalSuggestionWrapper : NSObject <AutocompleteSuggestion>

- (instancetype)initWithPedal:(id<OmniboxPedal, OmniboxIcon>)pedal;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_PEDAL_SUGGESTION_WRAPPER_H_
