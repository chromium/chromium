// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"

@implementation ExtendedTouchTargetButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.pointerInteractionEnabled = YES;
    _minimumDiameter = 44;
  }
  return self;
}

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  // Point is in `bounds` coordinates, but `center` is in the `superview`
  // coordinates. Compute center in `bounds` coords.
  CGPoint center =
      CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
  CGFloat distance = sqrt((center.x - point.x) * (center.x - point.x) +
                          ((center.y - point.y) * (center.y - point.y)));
  if (distance < self.minimumDiameter / 2) {
    return YES;
  }
  return [super pointInside:point withEvent:event];
}

@end
