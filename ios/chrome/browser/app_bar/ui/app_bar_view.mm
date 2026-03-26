// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view.h"

#import <QuartzCore/QuartzCore.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"

@implementation AppBarView {
  CAShapeLayer* _maskLayer;
  UIBezierPath* _maskPath;
  CGRect _lastBounds;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _maskLayer = [CAShapeLayer layer];
    _maskLayer.fillRule = kCAFillRuleEvenOdd;
    self.layer.mask = _maskLayer;
    self.backgroundColor = [UIColor.purpleColor colorWithAlphaComponent:1.0];
    [self updateMask];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateMask];
}

#pragma mark - UIView

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // For the background area, only return YES if the point is within the filled
  // part of the mask (the "ears" and the main bar).
  return [_maskPath containsPoint:point];
}

#pragma mark - Private

// Updates the cut out mask if the shape of the app bar changed.
- (void)updateMask {
  CGRect bounds = self.bounds;
  if (CGRectIsEmpty(bounds) || CGRectEqualToRect(bounds, _lastBounds)) {
    return;
  }

  _lastBounds = bounds;

  //   Use even-odd fill to subtract the cutout from the full view.
  _maskPath = [UIBezierPath bezierPathWithRect:bounds];
  _maskPath.usesEvenOddFillRule = YES;

  // Cutout covers the overlap area (height = kAppBarCornerRadius).
  // To avoid clamping the corner radius (UIKit clamps to half the height),
  // we make the cutout rect twice as tall and offset it upwards.
  CGRect cutoutRect = CGRectMake(0, -kAppBarCornerRadius, bounds.size.width,
                                 2 * kAppBarCornerRadius);

  // The cutout's bottom corners are rounded. Since the rect is 2x radius tall,
  // the corner radius will not be clamped and will provide the full 16pt curve
  // in the visible [0, 16] range.
  UIBezierPath* cutoutPath =
      [UIBezierPath bezierPathWithRoundedRect:cutoutRect
                            byRoundingCorners:(UIRectCornerBottomLeft |
                                               UIRectCornerBottomRight)
                                  cornerRadii:CGSizeMake(kAppBarCornerRadius,
                                                         kAppBarCornerRadius)];

  [_maskPath appendPath:cutoutPath];
  _maskLayer.path = _maskPath.CGPath;
}

@end
