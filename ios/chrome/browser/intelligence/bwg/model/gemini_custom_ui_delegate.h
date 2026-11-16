// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol GeminiBlockContext;
@protocol GeminiBlockNode;

// Protocol for providing custom UI for Gemini response blocks. Consulted on
// the main thread while a response is rendered.
@protocol GeminiCustomUIDelegate <NSObject>

// Returns YES if the delegate provides a custom view for `blockNode`, which
// then replaces downstream's own rendering of the block.
- (BOOL)providesCustomContentViewForBlockNode:(id<GeminiBlockNode>)blockNode;

// Returns YES to remove `blockNode` from the response entirely.
- (BOOL)shouldHideBlockNode:(id<GeminiBlockNode>)blockNode;

// Returns a self-sizing view for `blockNode` (if
// `providesCustomContentViewForBlockNode:` returns YES).
//
// Called again whenever `blockNode` changes while streaming and whenever
// `context` changes or when a newer response starts.
//
// On the first call (`view` is nil), returns a new view, even if `blockNode`
// is still partial. Later calls for the same block pass that view back in
// `view`: the delegate updates it in place from `blockNode` and `context` and
// returns it, or returns a new view.
- (UIView*)customContentViewForBlockNode:(id<GeminiBlockNode>)blockNode
                                 context:(id<GeminiBlockContext>)context
                             reusingView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_DELEGATE_H_
