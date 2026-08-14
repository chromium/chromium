// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_viewport_insets_animator.h"

#import <QuartzCore/QuartzCore.h>

#import <cmath>

#import "base/check.h"

@implementation CRWViewportInsetsAnimator {
  NSTimeInterval _duration;
  UIEdgeInsets _startInsets;
  CRWViewportInsetsUpdateHandler _updateHandler;
  ProceduralBlock _completion;

  CADisplayLink* _insetsDisplayLink;
  CFTimeInterval _animationStartTime;
}

- (instancetype)initWithStartInsets:(UIEdgeInsets)startInsets
                       targetInsets:(UIEdgeInsets)targetInsets
                           duration:(NSTimeInterval)duration
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
  if (_animationStartTime <= 0) {
    _animationStartTime = displayLink.timestamp;
  }
  CFTimeInterval elapsed = displayLink.timestamp - _animationStartTime;
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

  // Ease-in-out quadratic curve.
  double easedProgress = progress < 0.5
                             ? 2.0 * progress * progress
                             : 1.0 - std::pow(-2.0 * progress + 2.0, 2) / 2.0;

  UIEdgeInsets currentInsets = UIEdgeInsetsMake(
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
