// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_VIEW_PROVIDER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_VIEW_PROVIDER_H_

#import <UIKit/UIKit.h>

@protocol GeminiBlockContext;
@protocol GeminiBlockNode;

// Renders one type of Gemini response block in place of the SDK's own UI.
// Added to `GeminiCustomUIHandler` and called on the main thread.
@protocol GeminiCustomUIViewProvider <NSObject>

// Whether the provider renders `block`, depending on the block's type.
- (BOOL)providesViewForBlock:(id<GeminiBlockNode>)block;

// Whether `block` should be removed from the response, depending on the
// block's type. A hidden block is rendered by nobody, so a provider must not
// both hide and render the same block type.
- (BOOL)hidesBlock:(id<GeminiBlockNode>)block;

// Returns a view for `block`. `view` is nil on the first call (while `block`
// may still be partial). Later calls pass the view back to be updated in place.
- (UIView*)viewForBlock:(id<GeminiBlockNode>)block
                context:(id<GeminiBlockContext>)context
            reusingView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CUSTOM_UI_VIEW_PROVIDER_H_
