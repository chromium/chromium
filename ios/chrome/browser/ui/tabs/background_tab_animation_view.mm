// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tabs/background_tab_animation_view.h"

#include "base/logging.h"
#include "ios/chrome/browser/ui/util/animation_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 28;
const CGFloat kMaxScale = 1.3;
const CGFloat kMinScale = 0.7;
CGFloat kRotationAngleInRadians = 20.0 / 180 * M_PI;
}  // namespace

@interface BackgroundTabAnimationView ()

// Whether the animation is taking place in incognito.
@property(nonatomic, assign) BOOL incognito;

@end

@implementation BackgroundTabAnimationView

- (instancetype)initWithFrame:(CGRect)frame incognito:(BOOL)incognito {
  self = [super initWithFrame:frame];
  if (self) {
    _incognito = incognito;

    if (@available(iOS 13, *)) {
      self.overrideUserInterfaceStyle = incognito
                                            ? UIUserInterfaceStyleDark
                                            : UIUserInterfaceStyleUnspecified;
    }
  }
  return self;
}

#pragma mark - Public

- (void)animateFrom:(CGPoint)originPoint
    toTabGridButtonWithCompletion:(void (^)())completion {
  DCHECK(self.superview);
  CGPoint origin = [self.superview convertPoint:originPoint fromView:nil];
  CGPoint destination = [self destinationPoint];

  // It can be negative.
  CGFloat xDiff = destination.x - origin.x;
  CGFloat yDiff = origin.y - destination.y;

  UIBezierPath* positionPath =
      [self positionPathWithParentHeight:self.superview.frame.size.height
                                   xDiff:xDiff
                                   yDiff:yDiff
                                  origin:origin
                             destination:destination];

  [CATransaction begin];

  [CATransaction setCompletionBlock:^{
    completion();
  }];
  CAMediaTimingFunction* easeIn = TimingFunction(ios::material::CurveEaseIn);
  CGFloat timing =
      [self animationDurationWithParentSize:self.superview.frame.size
                                      xDiff:xDiff
                                      yDiff:yDiff];

  CAKeyframeAnimation* scaleAnimation =
      [CAKeyframeAnimation animationWithKeyPath:@"transform.scale"];
  scaleAnimation.values = @[ @(1), @(kMaxScale), @(kMinScale) ];
  scaleAnimation.keyTimes = @[ @0, @0.25, @1 ];
  scaleAnimation.timingFunction = easeIn;
  scaleAnimation.duration = timing;

  CAKeyframeAnimation* rotateAnimation =
      [CAKeyframeAnimation animationWithKeyPath:@"transform.rotation.z"];
  rotateAnimation.values = @[ @(0), @(0), @(kRotationAngleInRadians) ];
  rotateAnimation.keyTimes = @[ @0, @0.5, @1 ];
  rotateAnimation.timingFunction = easeIn;
  rotateAnimation.duration = timing;

  CAKeyframeAnimation* fadeAnimation =
      [CAKeyframeAnimation animationWithKeyPath:@"opacity"];
  fadeAnimation.values = @[ @(1), @(1), @(0) ];
  fadeAnimation.keyTimes = @[ @0, @0.9, @1 ];
  fadeAnimation.timingFunction = easeIn;
  fadeAnimation.duration = timing;

  CAKeyframeAnimation* positionAnimation =
      [CAKeyframeAnimation animationWithKeyPath:@"position"];
  positionAnimation.path = positionPath.CGPath;
  positionAnimation.duration = timing;
  positionAnimation.timingFunction = easeIn;

  [self.layer
      addAnimation:AnimationGroupMake(@[
        positionAnimation, rotateAnimation, fadeAnimation, scaleAnimation
      ])
            forKey:@"OpenInNewTabAnimation"];
  [CATransaction commit];
}

#pragma mark - UIView

- (void)didMoveToSuperview {
  [super didMoveToSuperview];

  if (self.subviews.count == 0) {
    self.backgroundColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kBackgroundColor], self.incognito,
        [UIColor colorNamed:kBackgroundDarkColor]);
    self.layer.shadowRadius = 20;
    self.layer.shadowOpacity = 0.4;
    self.layer.shadowOffset = CGSizeMake(0, 3);
    self.layer.cornerRadius = 13;
    UIImageView* linkImage = [[UIImageView alloc]
        initWithImage:
            [[UIImage imageNamed:@"open_new_tab_background"]
                imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]];
    linkImage.translatesAutoresizingMaskIntoConstraints = NO;
    linkImage.tintColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kToolbarButtonColor], self.incognito,
        [UIColor colorNamed:kToolbarButtonDarkColor]);

    [self addSubview:linkImage];

    [linkImage.widthAnchor constraintEqualToConstant:kImageSize].active = YES;
    [linkImage.heightAnchor constraintEqualToConstant:kImageSize].active = YES;
    AddSameCenterConstraints(self, linkImage);
  }
}

#pragma mark - Private

// Returns the destination point for the animation, in the superview
// coordinates.
- (CGPoint)destinationPoint {
  UILayoutGuide* tabGridButtonLayoutGuide =
      [NamedGuide guideWithName:kTabSwitcherGuide view:self.superview];
  CGRect frame = [tabGridButtonLayoutGuide layoutFrame];
  CGPoint tabGridButtonCenter =
      CGPointMake(frame.origin.x + frame.size.width / 2,
                  frame.origin.y + frame.size.height / 2);
  return [self.superview convertPoint:tabGridButtonCenter
                             fromView:tabGridButtonLayoutGuide.owningView];
}

// Returns the animation duration, based on the |parentSize| and the |yDiff| and
// |xDiff| between the origin and destination point. The animation is faster the
// closer the origin and destination are.
- (CGFloat)animationDurationWithParentSize:(CGSize)parentSize
                                     xDiff:(CGFloat)xDiff
                                     yDiff:(CGFloat)yDiff {
  CGFloat parentWidth = parentSize.width;
  CGFloat parentHeight = parentSize.height;

  CGFloat parentViewDiagonal =
      parentWidth * parentWidth + parentHeight * parentHeight;
  CGFloat distance = xDiff * xDiff + yDiff * yDiff;

  return 0.8 * sqrt(distance / parentViewDiagonal) + 0.2;
}

// Returns the BezierPath that should be followed by the animated view, based on
// the |parentSize| and the |yDiff| and |xDiff| between the |origin| and
// |destination| point.
- (UIBezierPath*)positionPathWithParentHeight:(CGFloat)parentHeight
                                        xDiff:(CGFloat)xDiff
                                        yDiff:(CGFloat)yDiff
                                       origin:(CGPoint)origin
                                  destination:(CGPoint)destination {
  CGFloat absYDiff = fabs(yDiff);
  CGFloat firstControlPointYDifference =
      absYDiff > parentHeight / 2 ? parentHeight / 2 * (yDiff / absYDiff)
                                  : yDiff;
  CGPoint firstControlPoint = CGPointMake(
      origin.x + xDiff * 0.5, origin.y + firstControlPointYDifference);
  CGPoint secondControlPoint =
      CGPointMake(destination.x, destination.y + yDiff / 2);

  UIBezierPath* path = UIBezierPath.bezierPath;
  [path moveToPoint:origin];
  [path addCurveToPoint:destination
          controlPoint1:firstControlPoint
          controlPoint2:secondControlPoint];
  return path;
}

@end
