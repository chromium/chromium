// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_blur.h"

#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_layer.h"

@implementation GradientBlur {
  GradientLayer* _gradientMask;
  UIViewPropertyAnimator* _blurAnimator;
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
    _gradientMask = [GradientLayer layer];
    _gradientMask.gradientType = gradientType;
    _gradientMask.frame = self.bounds;
    [_gradientMask setStartColor:UIColor.blackColor
                        endColor:UIColor.clearColor];
    _gradientMask.startPoint = startPoint;
    _gradientMask.endPoint = endPoint;

    self.layer.mask = _gradientMask;

    if (effectPercentage < 1) {
      __weak __typeof(self) weakSelf = self;
      _blurAnimator = [[UIViewPropertyAnimator alloc]
          initWithDuration:1.0
                     curve:UIViewAnimationCurveLinear
                animations:^{
                  weakSelf.effect = effect;
                }];

      [_blurAnimator pauseAnimation];

      _blurAnimator.fractionComplete = effectPercentage;
    }
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _gradientMask.frame = self.bounds;
}

@end
