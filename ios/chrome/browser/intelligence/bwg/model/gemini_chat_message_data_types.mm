// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_chat_message_data_types.h"

@implementation GeminiChatMessageRequest

- (instancetype)initWithText:(NSString*)text
                   sessionID:(NSString*)sessionID
              conversationID:(NSString*)conversationID {
  self = [super init];
  if (self) {
    _text = [text copy];
    _sessionID = [sessionID copy];
    _conversationID = [conversationID copy];
  }
  return self;
}

@end

@implementation GeminiChatMessageResponse

- (instancetype)initWithShouldConsume:(BOOL)shouldConsume {
  self = [super init];
  if (self) {
    _shouldConsume = shouldConsume;
  }
  return self;
}

@end
