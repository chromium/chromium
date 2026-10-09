// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CHAT_MESSAGE_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CHAT_MESSAGE_HANDLER_H_

#import <Foundation/Foundation.h>

@class GeminiChatMessageRequest;
@class GeminiChatMessageResponse;

// Handles chat messages submitted by the user before they are sent to Gemini,
// e.g. to route them to an ongoing task as the answer to a clarification
// request.
@protocol GeminiChatMessageHandler <NSObject>

// Decides whether `request` is consumed. Calls `completion` exactly once, on
// the main thread, and currently synchronously as the Gemini SDK does not yet
// support asynchronous interception.
- (void)handleChatMessageRequest:(GeminiChatMessageRequest*)request
                      completion:(void (^)(GeminiChatMessageResponse* response))
                                     completion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CHAT_MESSAGE_HANDLER_H_
