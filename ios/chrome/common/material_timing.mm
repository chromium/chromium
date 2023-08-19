// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/material_timing.h"

namespace {

UIViewAnimationOptions AnimationOptionsForceLinearTiming(
    UIViewAnimationOptions options) {
  // Remove any non-linear timing options from `options`. They should be
  // ignored.
  options &=
      ~(UIViewAnimationOptionCurveEaseInOut | UIViewAnimationOptionCurveEaseIn |
        UIViewAnimationOptionCurveEaseOut);
  // Since default is EaseInOut, ensure linear is specified instead so the outer
  // timing function works as expected.
  options |= UIViewAnimationOptionCurveLinear;
  return options;
}

// Constant to slow down animations during development.
// Since iOS Simulator's "Debug > Slow Animations" doesn't apply to
// CAAnimations, modify this multiplier here in code to run with slower/faster
// Material animations.
const NSTimeInterval kSlowAnimationModifier = 1;

}  // namespace

const CGFloat kMaterialDuration0 = 0 * kSlowAnimationModifier;
const CGFloat kMaterialDuration1 = 0.25 * kSlowAnimationModifier;
const CGFloat kMaterialDuration2 = 0.1 * kSlowAnimationModifier;
const CGFloat kMaterialDuration3 = 0.35 * kSlowAnimationModifier;
const CGFloat kMaterialDuration4 = 0.05 * kSlowAnimationModifier;
const CGFloat kMaterialDuration5 = 0.5 * kSlowAnimationModifier;
const CGFloat kMaterialDuration6 = 0.15 * kSlowAnimationModifier;
const CGFloat kMaterialDuration7 = 0.4 * kSlowAnimationModifier;
const CGFloat kMaterialDuration8 = 0.07 * kSlowAnimationModifier;

CAMediaTimingFunction* MaterialTransformCurve2() {
  return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                         0.0f:
                                                        0.84f:
                                                        0.13f:0.99f];
}

CAMediaTimingFunction* MaterialTimingFunction(MaterialCurve curve) {
  switch (curve) {
    case MaterialCurveEaseInOut:
      // This curve is slow both at the beginning and end.
      // Visualization of curve  http://cubic-bezier.com/#.4,0,.2,1
      return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                             0.4f:
                                                             0.0f:
                                                             0.2f:1.0f];
    case MaterialCurveEaseOut:
      // This curve is slow at the end.
      // Visualization of curve  http://cubic-bezier.com/#0,0,.2,1
      return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                             0.0f:
                                                             0.0f:
                                                             0.2f:1.0f];
    case MaterialCurveEaseIn:
      // This curve is slow at the beginning.
      // Visualization of curve  http://cubic-bezier.com/#.4,0,1,1
      return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                             0.4f:
                                                             0.0f:
                                                             1.0f:1.0f];
    case MaterialCurveLinear:
      // This curve is linear.
      return
          [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
  }
}

@implementation UIView (CrMaterialAnimations)

+ (void)cr_animateWithDuration:(NSTimeInterval)duration
                         delay:(NSTimeInterval)delay
                 materialCurve:(MaterialCurve)materialCurve
                       options:(UIViewAnimationOptions)options
                    animations:(void (^)(void))animations
                    completion:(void (^)(BOOL finished))completion {
  [CATransaction begin];
  [CATransaction
      setAnimationTimingFunction:MaterialTimingFunction(materialCurve)];
  [UIView animateWithDuration:duration
                        delay:delay
                      options:AnimationOptionsForceLinearTiming(options)
                   animations:animations
                   completion:completion];
  [CATransaction commit];
}

@end
