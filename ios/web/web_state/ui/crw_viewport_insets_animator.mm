// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_viewport_insets_animator.h"

#import <QuartzCore/QuartzCore.h>

#import <algorithm>
#import <cmath>

#import "base/check.h"

namespace {

// Returns a UIEdgeInsets with each edge clamped to be non-negative.
UIEdgeInsets MakeSanitizedInsets(CGFloat top,
                                 CGFloat left,
                                 CGFloat bottom,
                                 CGFloat right) {
  return UIEdgeInsetsMake(
      std::max<CGFloat>(0.0, top), std::max<CGFloat>(0.0, left),
      std::max<CGFloat>(0.0, bottom), std::max<CGFloat>(0.0, right));
}

}  // namespace

@implementation CRWViewportInsetsAnimator {
  NSTimeInterval _duration;
  CGFloat _initialVelocity;
  UIEdgeInsets _startInsets;
  CRWViewportInsetsUpdateHandler _updateHandler;
  ProceduralBlock _completion;

  CADisplayLink* _insetsDisplayLink;
  CFTimeInterval _animationStartTime;
}

- (instancetype)initWithStartInsets:(UIEdgeInsets)startInsets
                       targetInsets:(UIEdgeInsets)targetInsets
                           duration:(NSTimeInterval)duration
                    initialVelocity:(CGFloat)initialVelocity
                      updateHandler:
                          (CRWViewportInsetsUpdateHandler)updateHandler
                         completion:(ProceduralBlock)completion {
  self = [super init];
  if (self) {
    CHECK(updateHandler);
    _startInsets = startInsets;
    _targetInsets = targetInsets;
    _currentInsets = startInsets;
    _duration = duration;
    _initialVelocity = initialVelocity;
    _updateHandler = [updateHandler copy];
    _completion = [completion copy];
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)start {
  if (_insetsDisplayLink) {
    return;
  }
  _animationStartTime = 0;
  _insetsDisplayLink =
      [CADisplayLink displayLinkWithTarget:self
                                  selector:@selector(handleDisplayLink:)];
  [_insetsDisplayLink addToRunLoop:[NSRunLoop mainRunLoop]
                           forMode:NSRunLoopCommonModes];
}

- (void)stop {
  [_insetsDisplayLink invalidate];
  _insetsDisplayLink = nil;
  _animationStartTime = 0;
}

#pragma mark - Private

// Handles each display link frame tick, calculates easing interpolation
// progress, invokes the update handler with intermediate insets, and runs
// completion on finish.
- (void)handleDisplayLink:(CADisplayLink*)displayLink {
  CFTimeInterval frameTime = displayLink.targetTimestamp > 0
                                 ? displayLink.targetTimestamp
                                 : displayLink.timestamp;
  if (_animationStartTime <= 0 || _animationStartTime > frameTime) {
    _animationStartTime = frameTime;
  }
  CFTimeInterval elapsed = std::max(0.0, frameTime - _animationStartTime);
  double progress = _duration > 0 ? (elapsed / _duration) : 1.0;
  if (progress >= 1.0) {
    UIEdgeInsets targetInsets = _targetInsets;
    CRWViewportInsetsUpdateHandler updateHandler = _updateHandler;
    ProceduralBlock completion = _completion;
    _updateHandler = nil;
    _completion = nil;
    [self stop];
    _currentInsets = targetInsets;
    updateHandler(targetInsets);
    if (completion) {
      completion();
    }
    return;
  }

  double easedProgress = 0.0;
  if (_duration > 0) {
    // Matches UIKit's critically damped spring curve so viewport insets
    // interpolate in lockstep with UIKit-animated browser toolbars.
    // 9.2334 is the empirical settling factor for UIKit springs.
    constexpr double kSpringSettlingFactor = 9.233414;
    double omega = kSpringSettlingFactor / _duration;
    easedProgress = 1.0 - (1.0 + (omega - _initialVelocity) * elapsed) *
                              std::exp(-omega * elapsed);
  } else {
    easedProgress = 1.0;
  }

  UIEdgeInsets currentInsets = MakeSanitizedInsets(
      _startInsets.top + easedProgress * (_targetInsets.top - _startInsets.top),
      _startInsets.left +
          easedProgress * (_targetInsets.left - _startInsets.left),
      _startInsets.bottom +
          easedProgress * (_targetInsets.bottom - _startInsets.bottom),
      _startInsets.right +
          easedProgress * (_targetInsets.right - _startInsets.right));

  _currentInsets = currentInsets;
  _updateHandler(currentInsets);
}

@end
