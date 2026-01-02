// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ASSISTANT_GEMINI_UI_ASSISTANT_GEMINI_CONSUMER_H_
#define IOS_CHROME_BROWSER_ASSISTANT_GEMINI_UI_ASSISTANT_GEMINI_CONSUMER_H_

#import <Foundation/Foundation.h>

// Consumer interface for the Gemini feature.
@protocol AssistantGeminiConsumer <NSObject>

// Updates the displayed content text.
// TODO(crbug.com/469050167): Replace with real methods once the UI is defined.
- (void)setContentText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_ASSISTANT_GEMINI_UI_ASSISTANT_GEMINI_CONSUMER_H_
