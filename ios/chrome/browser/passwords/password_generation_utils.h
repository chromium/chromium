// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_UTILS_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_UTILS_H_

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

namespace passwords {

// Block types for `RunSearchPipeline`.
typedef void (^PipelineBlock)(void (^completion)(BOOL));
typedef void (^PipelineCompletionBlock)(NSUInteger index);

// Executes each PipelineBlock in `blocks` in order until one invokes its
// completion with YES, in which case `on_complete` will be invoked with the
// `index` of the succeeding block, or until they all invoke their completions
// with NO, in which case `on_complete` will be invoked with NSNotFound.
void RunSearchPipeline(NSArray* blocks, PipelineCompletionBlock on_complete);

}  // namespace passwords

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_UTILS_H_
