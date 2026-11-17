// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_ACTUATIONS_BLOCK_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_ACTUATIONS_BLOCK_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/bwg/model/gemini_block_data_types.h"

@class GeminiYieldAction;

// Protocols mirroring the agent task block of the Gemini SDK, so Chrome can
// render agent tasks in Gemini responses. A `GeminiBrowserActuationsBlockNode`
// carries a `GeminiBrowserActuationsBlock`, which lists every
// `GeminiBrowserActuation` the agent performed.

// One action the agent performed in the browser during an agent task.
@protocol GeminiBrowserActuation <NSObject>

// Human-readable description of the action (e.g. "Search flights"), or nil if
// not set.
@property(nonatomic, readonly, copy) NSString* title;

// Why the agent yielded control to the user after this action, or nil if it
// didn't yield.
@property(nonatomic, readonly) GeminiYieldAction* yieldAction;

// The user's reply to this action's confirmation or clarification request, or
// nil if there is none (yet).
@property(nonatomic, readonly, copy) NSString* userResponse;

@end

// The browser actions the agent performed in one Gemini response (an agent
// task).
@protocol GeminiBrowserActuationsBlock <NSObject>

// The actions, in the order the agent performed them.
@property(nonatomic, readonly, copy)
    NSArray<id<GeminiBrowserActuation>>* actuations;

@end

// Adopted by block nodes that can carry an agent task.
@protocol GeminiBrowserActuationsBlockNode <GeminiBlockNode>

// The agent task, or nil if this node holds another type of block.
@property(nonatomic, readonly) id<GeminiBrowserActuationsBlock>
    browserActuationsBlock;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_BROWSER_ACTUATIONS_BLOCK_H_
