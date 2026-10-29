// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for the Gemini contextual cue settings view controller.
@protocol GeminiContextualCueSettingsConsumer <NSObject>

// Sets the contextual cue `prompt` displayed in the footer below the "Edit
// prompt" row.
- (void)setPrompt:(NSString*)prompt;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_UI_GEMINI_CONTEXTUAL_CUE_SETTINGS_CONSUMER_H_
