// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ui/avatar/ai_tier_ring_image_view.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation AITierRingImageView {
  CAShapeLayer* _maskLayer;
}

- (instancetype)initWithImage:(UIImage*)image {
  self = [super initWithImage:image];
  if (self) {
    _maskLayer = [CAShapeLayer layer];
    _maskLayer.fillRule = kCAFillRuleEvenOdd;
    self.layer.mask = _maskLayer;
    AddSizeConstraints(self, image.size);
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGRect bounds = self.bounds;
  // Creates a mask path with two concentric circles: the outer bounds and an
  // inner circle inset by the ring width. The even-odd fill rule ensures that
  // only the region between the two circles (the ring) is filled (opaque) in
  // the mask, cropping the image view to a ring.
  UIBezierPath* maskPath = [UIBezierPath bezierPathWithOvalInRect:bounds];
  CGRect innerRect = CGRectInset(bounds, kAiTierRingWidth, kAiTierRingWidth);
  [maskPath appendPath:[UIBezierPath bezierPathWithOvalInRect:innerRect]];
  _maskLayer.path = maskPath.CGPath;
  _maskLayer.frame = bounds;
}

@end
