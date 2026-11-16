// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BLOCK_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BLOCK_DATA_TYPES_H_

#import <Foundation/Foundation.h>

// State of the response a block belongs to.
enum class GeminiResponseState {
  // The response is still being received or animated in.
  kStreaming,
  // The response completed.
  kDone,
  // The user stopped the response before it completed.
  kStopped,
  // The response failed.
  kFailed,
};

// One block of a Gemini response. Block types Chrome renders extend this
// protocol.
@protocol GeminiBlockNode <NSObject>
@end

// The response a block belongs to.
@protocol GeminiBlockContext <NSObject>

// State of the block's response.
@property(nonatomic, readonly) GeminiResponseState responseState;

// Whether the block's response is the latest one in the conversation.
@property(nonatomic, readonly, getter=isLatestResponse) BOOL latestResponse;

// Server ID of the conversation, or nil if it isn't known yet.
@property(nonatomic, readonly, copy) NSString* conversationID;

// Server ID of the block's response, or nil if it isn't known yet.
@property(nonatomic, readonly, copy) NSString* responseID;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BLOCK_DATA_TYPES_H_
