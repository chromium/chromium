// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/model/fullscreen_progress_animator.h"

#import <QuartzCore/QuartzCore.h>

#import <algorithm>

#import "base/check.h"
#import "ios/web/common/critically_damped_spring.h"

@implementation FullscreenProgressAnimator {
  FullscreenProgressUpdateHandler _updateHandler;
  ProceduralBlock _completion;

  CADisplayLink* _displayLink;
  CFTimeInterval _animationStartTime;

  CGFloat _startProgress;
  CGFloat _targetProgress;
  CGFloat _initialVelocity;
  base::TimeDelta _duration;

  // Incremented on every `-animateFromProgress:...` call so that a completion
  // belonging to a superseded animation can be identified and discarded.
  NSInteger _generation;
}

- (instancetype)initWithUpdateHandler:
                    (FullscreenProgressUpdateHandler)updateHandler
                           completion:(ProceduralBlock)completion {
  self = [super init];
  if (self) {
    CHECK(updateHandler);
    _updateHandler = [updateHandler copy];
    _completion = [completion copy];
    _currentProgress = 1.0;
  }
  return self;
}

- (void)dealloc {
  // Only reachable once the display link has been invalidated, since a
  // scheduled link retains its target. Owners must call `-stop` explicitly.
  [self invalidateDisplayLink];
}

#pragma mark - Public

- (void)animateFromProgress:(CGFloat)startProgress
                 toProgress:(CGFloat)targetProgress
                   duration:(base::TimeDelta)duration
            initialVelocity:(CGFloat)initialVelocity {
  ++_generation;

  _startProgress = startProgress;
  _targetProgress = targetProgress;
  _initialVelocity = initialVelocity;
  _duration = duration;
  _currentProgress = startProgress;
  _animationStartTime = 0;

  if (!duration.is_positive()) {
    [self finish];
    return;
  }

  if (!_displayLink) {
    _displayLink =
        [CADisplayLink displayLinkWithTarget:self
                                    selector:@selector(handleDisplayLink:)];
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                       forMode:NSRunLoopCommonModes];
  }
  _animating = YES;
}

- (void)stop {
  [self invalidateDisplayLink];
  _animating = NO;
}

#pragma mark - Private

// Tears down the display link, releasing its strong reference to `self`.
- (void)invalidateDisplayLink {
  [_displayLink invalidate];
  _displayLink = nil;
  _animationStartTime = 0;
}

// Handles a single frame tick: advances the interpolation and reports it.
- (void)handleDisplayLink:(CADisplayLink*)displayLink {
  // Target the frame currently being composed rather than the one just shown,
  // so the value reported is the one that will actually be displayed.
  CFTimeInterval frameTime = displayLink.targetTimestamp > 0
                                 ? displayLink.targetTimestamp
                                 : displayLink.timestamp;
  if (_animationStartTime <= 0 || _animationStartTime > frameTime) {
    _animationStartTime = frameTime;
  }
  CFTimeInterval elapsed = std::max(0.0, frameTime - _animationStartTime);

  if (elapsed >= _duration.InSecondsF()) {
    [self finish];
    return;
  }

  const double easedProgress = web::CriticallyDampedSpringProgress(
      elapsed, _duration.InSecondsF(), _initialVelocity);
  // A fast flick can drive the spring past its target; progress is
  // contractually bound to [0, 1], so absorb the overshoot here rather than in
  // every observer.
  _currentProgress = std::clamp<CGFloat>(
      _startProgress + easedProgress * (_targetProgress - _startProgress), 0.0,
      1.0);
  _updateHandler(_currentProgress);
}

// Settles exactly on the target and notifies the owner.
- (void)finish {
  const NSInteger generation = _generation;
  [self invalidateDisplayLink];
  _animating = NO;
  _currentProgress = _targetProgress;
  _updateHandler(_targetProgress);

  // The update handler may have started a new animation, in which case the
  // completion belongs to that animation instead.
  if (_completion && generation == _generation) {
    _completion();
  }
}

@end
