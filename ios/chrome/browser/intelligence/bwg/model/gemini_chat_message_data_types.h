// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CHAT_MESSAGE_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CHAT_MESSAGE_DATA_TYPES_H_

#import <Foundation/Foundation.h>

// Represents a chat message submitted by the user, before it is sent to Gemini.
// Mirrors the Gemini SDK chat message request. Fields added after the initial
// set must be `readwrite` properties with a nil/NO default, so that the
// ios_internal bridge can start populating them without changing the
// initializer.
@interface GeminiChatMessageRequest : NSObject

// The whitespace-trimmed input text. May be empty.
@property(nonatomic, readonly, copy) NSString* text;

// The Gemini session ID. May be nil.
@property(nonatomic, readonly, copy) NSString* sessionID;

// The server conversation ID. Nil before the first turn.
@property(nonatomic, readonly, copy) NSString* conversationID;

- (instancetype)initWithText:(NSString*)text
                   sessionID:(NSString*)sessionID
              conversationID:(NSString*)conversationID
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Chrome's decision for a `GeminiChatMessageRequest`. Mirrors the Gemini SDK
// chat message response. Fields added after the initial set must be
// `readwrite` properties with a nil/NO default.
@interface GeminiChatMessageResponse : NSObject

// Whether Chrome consumed the message. When YES, nothing is sent to Gemini and
// everything the user composed is discarded.
@property(nonatomic, readonly, assign) BOOL shouldConsume;

- (instancetype)initWithShouldConsume:(BOOL)shouldConsume
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CHAT_MESSAGE_DATA_TYPES_H_
