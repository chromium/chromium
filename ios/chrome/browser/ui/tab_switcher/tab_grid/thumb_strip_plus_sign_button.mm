// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/thumb_strip_plus_sign_button.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ThumbStripPlusSignButton ()
// The transparency gradient of the button.
@property(nonatomic, strong) CAGradientLayer* gradient;
// The current constraints for the image.
@property(nonatomic, strong) NSLayoutConstraint* plusYConstraints;
@end

@implementation ThumbStripPlusSignButton

- (void)didMoveToSuperview {
  [super didMoveToSuperview];

  if (self.subviews.count > 0)
    return;

  UIImageView* plusSignImage = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"grid_cell_plus_sign"]];
  self.plusSignImage = plusSignImage;
  plusSignImage.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:plusSignImage];

  self.plusYConstraints = [plusSignImage.centerYAnchor
      constraintEqualToAnchor:self.topAnchor
                     constant:kPlusSignImageYCenterConstant];

  NSArray* constraints = @[
    [plusSignImage.centerXAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kPlusSignImageTrailingCenterDistance],
    self.plusYConstraints,
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];

  if (self.gradient)
    [self.gradient removeFromSuperlayer];

  CAGradientLayer* gradient = [CAGradientLayer layer];
  self.gradient = gradient;
  gradient.frame = self.bounds;
  gradient.colors = @[
    (id)UIColor.clearColor.CGColor,
    (id)[UIColor.blackColor colorWithAlphaComponent:0.72].CGColor,
    (id)UIColor.blackColor.CGColor
  ];
  gradient.startPoint = CGPointMake(0.0, 0.5);
  gradient.endPoint = CGPointMake(1.0, 0.5);
  gradient.locations = @[ @0, @0.5, @0.87 ];
  if (UseRTLLayout()) {
    gradient.affineTransform = CGAffineTransformMakeScale(-1, 1);
  }
  [self.layer insertSublayer:gradient atIndex:0];
}

#pragma mark - Properties

- (void)setPlusSignVerticalOffset:(CGFloat)verticalOffset {
  BOOL updateNeeded = _plusSignVerticalOffset != verticalOffset;
  _plusSignVerticalOffset = verticalOffset;
  if (updateNeeded) {
    self.plusYConstraints.constant =
        kPlusSignImageYCenterConstant + self.plusSignVerticalOffset;
  }
}

@end
