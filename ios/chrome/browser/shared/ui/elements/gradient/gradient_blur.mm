// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_blur.h"

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_layer.h"

@implementation GradientBlur {
  GradientLayer* _gradientMask;
  UIViewPropertyAnimator* _blurAnimator;
  UIVisualEffect* _effect;
  CGFloat _effectPercentage;
}

- (instancetype)initWithEffect:(UIVisualEffect*)effect
              effectPercentage:(CGFloat)effectPercentage
                    startPoint:(CGPoint)startPoint
                      endPoint:(CGPoint)endPoint
                  gradientType:(GradientLayerType)gradientType {
  if (effectPercentage < 1) {
    self = [super initWithEffect:nil];
  } else {
    self = [super initWithEffect:effect];
  }
  if (self) {
    _effectPercentage = effectPercentage;
    _gradientMask = [GradientLayer layer];
    _gradientMask.gradientType = gradientType;
    _gradientMask.frame = self.bounds;
    [_gradientMask setStartColor:UIColor.blackColor
                        endColor:UIColor.clearColor];
    _gradientMask.startPoint = startPoint;
    _gradientMask.endPoint = endPoint;

    self.layer.mask = _gradientMask;

    if (effectPercentage < 1) {
      _effect = effect;
      [[NSNotificationCenter defaultCenter]
          addObserver:self
             selector:@selector(applyEffect)
                 name:UIApplicationWillEnterForegroundNotification
               object:nil];
    }
  }
  return self;
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  if (self.window) {
    [self applyEffect];
  } else {
    [self clearEffect];
  }
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _gradientMask.frame = self.bounds;
}

#pragma mark - Private

// Clears the animator that creates the partial blur effect.
- (void)clearEffect {
  if (_effectPercentage >= 1 || !_blurAnimator) {
    return;
  }

  [_blurAnimator stopAnimation:YES];
  [_blurAnimator finishAnimationAtPosition:UIViewAnimatingPositionStart];
  _blurAnimator = nil;
  self.effect = nil;
}

// Applies the partial blur effect by (re)creating a paused
// UIViewPropertyAnimator.
- (void)applyEffect {
  if (_effectPercentage >= 1) {
    return;
  }

  [self clearEffect];
  UIVisualEffect* effectForAnimator = _effect;
  __weak __typeof(self) weakSelf = self;
  _blurAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:1.0
                 curve:UIViewAnimationCurveLinear
            animations:^{
              weakSelf.effect = effectForAnimator;
            }];

  [_blurAnimator pauseAnimation];
  _blurAnimator.fractionComplete = _effectPercentage;
}

@end
