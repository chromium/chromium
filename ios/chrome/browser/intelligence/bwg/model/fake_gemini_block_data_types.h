// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_BLOCK_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_BLOCK_DATA_TYPES_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_block_data_types.h"

// Fake block node for tests.
@interface FakeGeminiBlockNode : NSObject <GeminiBlockNode>
@end

// Fake block context with settable response metadata. Defaults to a completed
// latest response.
@interface FakeGeminiBlockContext : NSObject <GeminiBlockContext>
@property(nonatomic, assign) GeminiResponseState responseState;
@property(nonatomic, assign, getter=isLatestResponse) BOOL latestResponse;
@property(nonatomic, copy) NSString* conversationID;
@property(nonatomic, copy) NSString* responseID;
@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_FAKE_GEMINI_BLOCK_DATA_TYPES_H_
