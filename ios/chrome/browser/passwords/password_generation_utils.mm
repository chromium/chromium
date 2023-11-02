// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_generation_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace passwords {

namespace {

// The actual implementation of `RunPipeline` that begins with the first block
// in `blocks`.
void RunSearchPipeline(NSArray* blocks,
                       PipelineCompletionBlock on_complete,
                       NSUInteger from_index) {
  if (from_index == [blocks count]) {
    on_complete(NSNotFound);
    return;
  }
  PipelineBlock block = blocks[from_index];
  block(^(BOOL success) {
    if (success)
      on_complete(from_index);
    else
      RunSearchPipeline(blocks, on_complete, from_index + 1);
  });
}

}  // namespace

void RunSearchPipeline(NSArray* blocks, PipelineCompletionBlock on_complete) {
  RunSearchPipeline(blocks, on_complete, 0);
}

}  // namespace passwords
