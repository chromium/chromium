// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_CRW_VIEWPORT_INSETS_ANIMATOR_H_
#define IOS_WEB_WEB_STATE_UI_CRW_VIEWPORT_INSETS_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// Callback block invoked on each animation frame with interpolated insets.
typedef void (^CRWViewportInsetsUpdateHandler)(UIEdgeInsets insets);

// Ephemeral helper object that drives frame-by-frame interpolation of viewport
// insets using a CADisplayLink.
@interface CRWViewportInsetsAnimator : NSObject

// Initializes the animator with start and target insets, duration, initial
// velocity, an update handler invoked on each frame tick, and a completion
// callback.
- (instancetype)initWithStartInsets:(UIEdgeInsets)startInsets
                       targetInsets:(UIEdgeInsets)targetInsets
                           duration:(NSTimeInterval)duration
                    initialVelocity:(CGFloat)initialVelocity
                      updateHandler:
                          (CRWViewportInsetsUpdateHandler)updateHandler
                         completion:(ProceduralBlock)completion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The target insets of the animation.
@property(nonatomic, readonly) UIEdgeInsets targetInsets;

// The current insets calculated during the animation.
@property(nonatomic, readonly) UIEdgeInsets currentInsets;

// Starts the CADisplayLink animation on the main run loop.
- (void)start;

// Stops and invalidates the display link animation without invoking completion.
- (void)stop;

@end

#endif  // IOS_WEB_WEB_STATE_UI_CRW_VIEWPORT_INSETS_ANIMATOR_H_
