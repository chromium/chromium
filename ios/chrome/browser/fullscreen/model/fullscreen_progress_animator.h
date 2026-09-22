// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_PROGRESS_ANIMATOR_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_PROGRESS_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/time/time.h"

// Block invoked on each animation frame with the interpolated progress.
typedef void (^FullscreenProgressUpdateHandler)(CGFloat progress);

// Drives frame-by-frame interpolation of the fullscreen progress with a
// `CADisplayLink`.
//
// This exists so that observers whose UI is a *non-linear* function of
// progress can be given real intermediate values. Observers whose UI is an
// affine function of progress do not need this: a UIKit animation already
// interpolates their result identically, for free, off the main thread.
//
// The interpolation curve deliberately matches the UIKit spring driving the
// rest of the fullscreen UI, so per-frame observers stay visually in lockstep
// with their UIKit-animated neighbours.
@interface FullscreenProgressAnimator : NSObject

// Initializes the animator. `updateHandler` is invoked on every frame,
// including a final invocation with the exact target progress.
- (instancetype)initWithUpdateHandler:
                    (FullscreenProgressUpdateHandler)updateHandler
                           completion:(ProceduralBlock)completion
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The progress computed on the most recent frame. Always within [0, 1].
@property(nonatomic, readonly) CGFloat currentProgress;

// Whether a display link is currently scheduled.
@property(nonatomic, readonly, getter=isAnimating) BOOL animating;

// Starts interpolating from `startProgress` to `targetProgress`. Supersedes
// any in-flight animation without invoking its completion. Passing
// `currentProgress` as `startProgress` retargets without a visual jump.
//
// `initialVelocity` is normalized: the fraction of the remaining distance
// covered per second at the start of the animation.
- (void)animateFromProgress:(CGFloat)startProgress
                 toProgress:(CGFloat)targetProgress
                   duration:(base::TimeDelta)duration
            initialVelocity:(CGFloat)initialVelocity;

// Invalidates the display link without invoking the completion block. Must be
// called before the owner is destroyed: a scheduled `CADisplayLink` retains
// its target, so `-dealloc` alone will never run.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_MODEL_FULLSCREEN_PROGRESS_ANIMATOR_H_
