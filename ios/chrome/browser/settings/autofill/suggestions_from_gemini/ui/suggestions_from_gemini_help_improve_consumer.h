// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_HELP_IMPROVE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_HELP_IMPROVE_CONSUMER_H_

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_constants.h"

// Consumer for the Suggestions from Gemini Help Improve settings.
@protocol SuggestionsFromGeminiHelpImproveConsumer <NSObject>

// Tells the consumer the policy state for Suggestions from Gemini.
- (void)setSuggestionsFromGeminiPolicyState:
    (SuggestionsFromGeminiPolicyState)state;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_HELP_IMPROVE_CONSUMER_H_
