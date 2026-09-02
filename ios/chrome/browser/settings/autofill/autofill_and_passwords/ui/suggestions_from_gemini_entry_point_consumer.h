// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SUGGESTIONS_FROM_GEMINI_ENTRY_POINT_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SUGGESTIONS_FROM_GEMINI_ENTRY_POINT_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer for displaying the Suggestions from Gemini entry point.
@protocol SuggestionsFromGeminiEntryPointConsumer <NSObject>

// Sets whether Suggestions from Gemini should be shown, and its enabled state.
- (void)setShouldShowSuggestionsFromGemini:(BOOL)shouldShow
                                   enabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_UI_SUGGESTIONS_FROM_GEMINI_ENTRY_POINT_CONSUMER_H_
