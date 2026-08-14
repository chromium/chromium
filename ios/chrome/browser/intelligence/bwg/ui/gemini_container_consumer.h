// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer protocol for updating the Gemini Container UI state.
@protocol GeminiContainerConsumer <NSObject>

// Updates the container for `zeroState`.
- (void)setZeroState:(BOOL)zeroState;

// Instructs the container to dismiss any active keyboard.
- (void)dismissKeyboard;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_UI_GEMINI_CONTAINER_CONSUMER_H_
