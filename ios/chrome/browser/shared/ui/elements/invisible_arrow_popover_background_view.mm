// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/invisible_arrow_popover_background_view.h"

@implementation InvisibleArrowPopoverBackgroundView

@synthesize arrowOffset = _arrowOffset;
@synthesize arrowDirection = _arrowDirection;

+ (CGFloat)arrowBase {
  return 0.0;
}

+ (CGFloat)arrowHeight {
  return 0.0;
}

+ (UIEdgeInsets)contentViewInsets {
  return UIEdgeInsetsZero;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.shadowOpacity = 0.0;
}
@end
