// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/page_animation_util.h"

#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/common/material_timing.h"

namespace {

// When animating out, a card shrinks slightly.
const CGFloat kAnimateOutScale = 0.7;
const CGFloat kAnimateOutAnchorX = 0.9;
const CGFloat kAnimateOutAnchorY = 0;
}  // namespace

namespace page_animation_util {

void UpdateLayerAnchorWithTransform(CALayer* layer,
                                    CGPoint newAnchor,
                                    CGAffineTransform transform) {
  CGSize size = layer.bounds.size;
  CGPoint oldAnchor = layer.anchorPoint;
  CGPoint newCenter =
      CGPointMake(size.width * newAnchor.x, size.height * newAnchor.y);
  CGPoint oldCenter =
      CGPointMake(size.width * oldAnchor.x, size.height * oldAnchor.y);

  newCenter = CGPointApplyAffineTransform(newCenter, transform);
  oldCenter = CGPointApplyAffineTransform(oldCenter, transform);

  CGPoint position = layer.position;
  position.x = position.x - oldCenter.x + newCenter.x;
  position.y = position.y - oldCenter.y + newCenter.y;
  layer.position = position;

  layer.anchorPoint = newAnchor;
}

void AnimateOutWithCompletion(UIView* view, void (^completion)(void)) {
  // The close animation spec calls for the anchor point to be the upper right.
  CGPoint newAnchorPoint = CGPointMake(kAnimateOutAnchorX, kAnimateOutAnchorY);
  CALayer* layer = [view layer];
  UpdateLayerAnchorWithTransform(layer, newAnchorPoint, view.transform);

  [CATransaction begin];
  if (completion) {
    [CATransaction setCompletionBlock:completion];
  }

  [CATransaction setAnimationDuration:kMaterialDuration6];
  CAMediaTimingFunction* timing = MaterialTimingFunction(MaterialCurveEaseIn);
  [CATransaction setAnimationTimingFunction:timing];

  CABasicAnimation* scaleAnimation =
      [CABasicAnimation animationWithKeyPath:@"transform"];
  CATransform3D transform = CATransform3DScale(
      layer.transform, kAnimateOutScale, kAnimateOutScale, 1);
  [scaleAnimation setToValue:[NSValue valueWithCATransform3D:transform]];

  CABasicAnimation* fadeAnimation =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  [fadeAnimation setFromValue:[NSNumber numberWithFloat:[layer opacity]]];
  [fadeAnimation setToValue:@0];

  [layer addAnimation:AnimationGroupMake(@[ scaleAnimation, fadeAnimation ])
               forKey:@"animateOut"];
  [CATransaction commit];
}

}  // namespace page_animation_util
