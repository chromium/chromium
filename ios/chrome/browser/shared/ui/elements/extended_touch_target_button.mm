// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"

@implementation ExtendedTouchTargetButton

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.pointerInteractionEnabled = YES;
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
  // The UI Guidelines recommend having at least 44pt tap target.
  if (distance < 22.0f) {
    return YES;
  }
  return [super pointInside:point withEvent:event];
}

@end
