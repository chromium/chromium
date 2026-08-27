// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_UI_GEMINI_ZERO_STATE_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_UI_GEMINI_ZERO_STATE_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ZeroStateSuggestion;

// Consumer protocol to receive updated zero-state suggestions for display.
@protocol GeminiZeroStateConsumer <NSObject>

// Sets the list of zero-state suggestions to display. An empty list clears the
// current suggestions.
- (void)setZeroStateSuggestions:(NSArray<ZeroStateSuggestion*>*)suggestions;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_UI_GEMINI_ZERO_STATE_CONSUMER_H_
