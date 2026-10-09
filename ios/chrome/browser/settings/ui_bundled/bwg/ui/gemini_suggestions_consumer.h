// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SUGGESTIONS_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for Gemini suggestions settings.
@protocol GeminiSuggestionsConsumer <NSObject>

// Sets the Gemini Suggestions boolean.
- (void)setSuggestionsEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_UI_GEMINI_SUGGESTIONS_CONSUMER_H_
