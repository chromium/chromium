// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_blur_view.h"

#import <QuartzCore/QuartzCore.h>

#import <algorithm>

namespace {

// Evaluates a standard ease-in-ease-out smoothstep curve for `t` in [0.0, 1.0].
CGFloat Smoothstep(CGFloat t) {
  return t * t * (3.0 - 2.0 * t);
}

}  // namespace

@implementation AppBarBlurView {
  UIBlurEffectStyle _style;
  UIViewPropertyAnimator* _animator;
  CADisplayLink* _displayLink;
  CGFloat _maxBlurFraction;
  CGFloat _startAmount;
  CGFloat _targetAmount;
  CFTimeInterval _animationStartTime;
  NSTimeInterval _animationDuration;
}

- (instancetype)initWithEffectStyle:(UIBlurEffectStyle)style
                    maxBlurFraction:(CGFloat)maxBlurFraction {
  self = [super initWithEffect:nil];
  if (self) {
    _style = style;
    _maxBlurFraction = std::clamp(maxBlurFraction, 0.0, 1.0);
    _blurAmount = 0.0;
  }
  return self;
}

- (void)dealloc {
  [self stopDisplayLinkAnimation];
  [self clearEffect];
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  if (!self.window) {
    [self stopDisplayLinkAnimation];
    [self clearEffect];
  }
}

#pragma mark - Properties

- (void)setBlurAmount:(CGFloat)blurAmount {
  blurAmount = std::clamp(blurAmount, 0.0, 1.0);
  if (_blurAmount == blurAmount) {
    return;
  }

  NSTimeInterval duration = [UIView inheritedAnimationDuration];
  if (duration > 0) {
    [self startDisplayLinkAnimationToAmount:blurAmount duration:duration];
  } else {
    [self stopDisplayLinkAnimation];
    _blurAmount = blurAmount;
    if (_blurAmount == 0.0) {
      [self clearEffect];
    } else {
      [self ensureAnimator];
      _animator.fractionComplete = _maxBlurFraction * _blurAmount;
    }
  }
}

#pragma mark - Private

// Clears and tears down the active animator and resets the effect to nil.
- (void)clearEffect {
  if (!_animator) {
    return;
  }
  [_animator stopAnimation:YES];
  _animator = nil;
  self.effect = nil;
}

// Ensures `_animator` is created and paused so that `fractionComplete` can be
// set.
- (void)ensureAnimator {
  if (_animator) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  UIBlurEffectStyle style = _style;
  _animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:1.0
                 curve:UIViewAnimationCurveLinear
            animations:^{
              weakSelf.effect = [UIBlurEffect effectWithStyle:style];
            }];
  _animator.pausesOnCompletion = YES;
  [_animator pauseAnimation];
}

// Starts a CADisplayLink animation stepping `_animator.fractionComplete` over
// `duration`.
- (void)startDisplayLinkAnimationToAmount:(CGFloat)targetAmount
                                 duration:(NSTimeInterval)duration {
  [self stopDisplayLinkAnimation];
  _startAmount = _blurAmount;
  _targetAmount = targetAmount;
  _animationDuration = duration;
  _animationStartTime = 0;

  if (_startAmount > 0.0 || _targetAmount > 0.0) {
    [self ensureAnimator];
    _animator.fractionComplete = _maxBlurFraction * _startAmount;
  }

  _displayLink =
      [CADisplayLink displayLinkWithTarget:self
                                  selector:@selector(handleDisplayLink:)];
  [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                     forMode:NSRunLoopCommonModes];
}

// Handler called on each screen refresh while animating.
- (void)handleDisplayLink:(CADisplayLink*)displayLink {
  if (_animationStartTime == 0) {
    _animationStartTime = displayLink.timestamp;
  }
  CFTimeInterval elapsed = displayLink.targetTimestamp - _animationStartTime;
  CGFloat t =
      std::clamp(static_cast<CGFloat>(elapsed / _animationDuration), 0.0, 1.0);
  CGFloat curveT = Smoothstep(t);
  _blurAmount = _startAmount + (_targetAmount - _startAmount) * curveT;

  if (_blurAmount == 0.0) {
    [self clearEffect];
  } else {
    [self ensureAnimator];
    _animator.fractionComplete = _maxBlurFraction * _blurAmount;
  }

  if (t >= 1.0) {
    [self stopDisplayLinkAnimation];
    if (_blurAmount == 0.0 || _blurAmount == 1.0) {
      [self clearEffect];
    }
  }
}

// Invalidates and tears down the active CADisplayLink.
- (void)stopDisplayLinkAnimation {
  [_displayLink invalidate];
  _displayLink = nil;
}

@end
