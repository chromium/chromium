// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer for the Suggestions from Gemini settings.
@protocol SuggestionsFromGeminiConsumer <NSObject>

// Tells the consumer to update the state of the Suggestions from Gemini switch.
- (void)setSuggestionsFromGeminiSwitchOn:(BOOL)on;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_UI_SUGGESTIONS_FROM_GEMINI_CONSUMER_H_
