// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/blue_dot_util.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
// Radius of the blue dot.
constexpr CGFloat kBlueDotRadius = 3;
// Margin between the blue dot and the top trailing corner of the view it is
// anchored to.
constexpr CGFloat kBlueDotMargin = 1;
// Thickness of the white border (hole) around the blue dot.
constexpr CGFloat kBlueDotWhiteBorderThickness = 2;
}  // namespace

void UpdateBlueDotMaskForView(UIView* viewToMask, BOOL hasBlueDot) {
  if (hasBlueDot) {
    CAShapeLayer* maskLayer = [CAShapeLayer layer];
    UIBezierPath* path = [UIBezierPath bezierPathWithRect:viewToMask.bounds];
    BOOL isRTL =
        [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                    viewToMask.semanticContentAttribute] ==
        UIUserInterfaceLayoutDirectionRightToLeft;
    CGFloat centerX = isRTL ? (kBlueDotMargin + kBlueDotRadius)
                            : (viewToMask.bounds.size.width -
                               (kBlueDotMargin + kBlueDotRadius));
    CGFloat centerY = kBlueDotMargin + kBlueDotRadius;
    UIBezierPath* holePath = [UIBezierPath
        bezierPathWithArcCenter:CGPointMake(centerX, centerY)
                         radius:(kBlueDotWhiteBorderThickness + kBlueDotRadius)
                     startAngle:0
                       endAngle:2 * M_PI
                      clockwise:YES];
    [path appendPath:holePath];
    maskLayer.path = path.CGPath;
    maskLayer.fillRule = kCAFillRuleEvenOdd;
    viewToMask.layer.mask = maskLayer;
  } else {
    viewToMask.layer.mask = nil;
  }
}

UIView* ConfigureAndAddBlueDotView(UIButton* button) {
  UIView* blueDotView = [[UIView alloc] initWithFrame:CGRectZero];
  blueDotView.translatesAutoresizingMaskIntoConstraints = NO;
  blueDotView.isAccessibilityElement = NO;
  blueDotView.backgroundColor = [UIColor colorNamed:kBlueColor];
  blueDotView.layer.cornerRadius = kBlueDotRadius;

  if (!button.configuration && button.imageView) {
    [button insertSubview:blueDotView belowSubview:button.imageView];
  } else {
    [button addSubview:blueDotView];
  }

  [NSLayoutConstraint activateConstraints:@[
    [blueDotView.widthAnchor constraintEqualToConstant:2 * kBlueDotRadius],
    [blueDotView.heightAnchor constraintEqualToAnchor:blueDotView.widthAnchor],
    [blueDotView.topAnchor constraintEqualToAnchor:button.topAnchor
                                          constant:kBlueDotMargin],
    [blueDotView.trailingAnchor constraintEqualToAnchor:button.trailingAnchor
                                               constant:-kBlueDotMargin],
  ]];

  return blueDotView;
}
