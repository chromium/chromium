// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/material_timing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

UIViewAnimationOptions AnimationOptionsForceLinearTiming(
    UIViewAnimationOptions options) {
  // Remove any non-linear timing options from |options|. They should be
  // ignored.
  options &=
      ~(UIViewAnimationOptionCurveEaseInOut | UIViewAnimationOptionCurveEaseIn |
        UIViewAnimationOptionCurveEaseOut);
  // Since default is EaseInOut, ensure linear is specified instead so the outer
  // timing function works as expected.
  options |= UIViewAnimationOptionCurveLinear;
  return options;
}

}  // namespace

namespace ios {
namespace material {

// Helper utility for making all animations slower, since 'toggle slow
// animations' in the simulator doesn't apply to CAAnimations.
const NSTimeInterval kSlowAnimationModifier = 1;

const CGFloat kDuration0 = 0 * kSlowAnimationModifier;
const CGFloat kDuration1 = 0.25 * kSlowAnimationModifier;
const CGFloat kDuration2 = 0.1 * kSlowAnimationModifier;
const CGFloat kDuration3 = 0.35 * kSlowAnimationModifier;
const CGFloat kDuration4 = 0.05 * kSlowAnimationModifier;
const CGFloat kDuration5 = 0.5 * kSlowAnimationModifier;
const CGFloat kDuration6 = 0.15 * kSlowAnimationModifier;
const CGFloat kDuration7 = 0.4 * kSlowAnimationModifier;
const CGFloat kDuration8 = 0.07 * kSlowAnimationModifier;

CAMediaTimingFunction* TransformCurve2() {
  return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                         0.0f:
                                                        0.84f:
                                                        0.13f:0.99f];
}

CAMediaTimingFunction* TimingFunction(Curve curve) {
  switch (curve) {
    case CurveEaseInOut:
      // This curve is slow both at the beginning and end.
      // Visualization of curve  http://cubic-bezier.com/#.4,0,.2,1
      return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                             0.4f:
                                                             0.0f:
                                                             0.2f:1.0f];
    case CurveEaseOut:
      // This curve is slow at the end.
      // Visualization of curve  http://cubic-bezier.com/#0,0,.2,1
      return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                             0.0f:
                                                             0.0f:
                                                             0.2f:1.0f];
    case CurveEaseIn:
      // This curve is slow at the beginning.
      // Visualization of curve  http://cubic-bezier.com/#.4,0,1,1
      return [[CAMediaTimingFunction alloc] initWithControlPoints:
                                                             0.4f:
                                                             0.0f:
                                                             1.0f:1.0f];
    case CurveLinear:
      // This curve is linear.
      return
          [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
  }
}

}  // material
}  // ios

@implementation UIView (CrMaterialAnimations)

+ (void)cr_animateWithDuration:(NSTimeInterval)duration
                         delay:(NSTimeInterval)delay
                         curve:(ios::material::Curve)curve
                       options:(UIViewAnimationOptions)options
                    animations:(void (^)(void))animations
                    completion:(void (^)(BOOL finished))completion {
  [CATransaction begin];
  [CATransaction setAnimationTimingFunction:TimingFunction(curve)];
  [UIView animateWithDuration:duration
                        delay:delay
                      options:AnimationOptionsForceLinearTiming(options)
                   animations:animations
                   completion:completion];
  [CATransaction commit];
}

@end
