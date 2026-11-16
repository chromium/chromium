// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_block_data_types.h"

@implementation FakeGeminiBlockNode
@end

@implementation FakeGeminiBlockContext

- (instancetype)init {
  self = [super init];
  if (self) {
    _responseState = GeminiResponseState::kDone;
    _latestResponse = YES;
  }
  return self;
}

@end
