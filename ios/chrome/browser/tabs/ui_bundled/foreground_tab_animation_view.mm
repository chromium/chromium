// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/ui_bundled/foreground_tab_animation_view.h"

#import "ios/chrome/browser/shared/ui/util/property_animator_group.h"

namespace {
const NSTimeInterval kAnimationDuration = 0.75;
const CGFloat kTabMotionDamping = 0.75;
const CGFloat kTabFadeInRelativeDuration = 0.4;
const CGFloat kBackgroundFadeRelativeDuration = 0.33;
const CGFloat kCornerRoundingRelativeDuration = 0.33;
const CGFloat kInitialTabScale = 0.75;
const CGFloat kInitialTabCornerRadius = 26.0;
const CGFloat kPositionCoefficient = 0.25;
}  // namespace

@implementation ForegroundTabAnimationView

@synthesize contentView = _contentView;

- (void)setContentView:(UIView*)contentView {
  [_contentView removeFromSuperview];
  [self addSubview:contentView];
  _contentView = contentView;
}

- (void)animateFrom:(CGPoint)originPoint withCompletion:(void (^)())completion {
  CGPoint origin = [self convertPoint:originPoint fromView:nil];

  self.backgroundColor = UIColor.clearColor;

  // Translate the content view part of the way from the center of this view to
  // `originPoint`.
  CGFloat dx = kPositionCoefficient * (origin.x - self.contentView.center.x);
  CGFloat dy = kPositionCoefficient * (origin.y - self.contentView.center.y);

  CGAffineTransform transform = self.contentView.transform;
  transform = CGAffineTransformTranslate(transform, dx, dy);
  transform =
      CGAffineTransformScale(transform, kInitialTabScale, kInitialTabScale);

  self.contentView.transform = transform;
  self.contentView.alpha = 0;
  self.contentView.layer.cornerRadius = kInitialTabCornerRadius;
  self.contentView.clipsToBounds = YES;

  // Animation components.
  auto tabResizeAnimation = ^{
    self.contentView.transform = CGAffineTransformIdentity;
  };
  auto tabFadeAnimation = ^{
    self.contentView.alpha = 1.0;
  };
  auto backgroundFadeAnimation = ^{
    self.backgroundColor = UIColor.blackColor;
  };
  auto cornerAnimation = ^{
    self.contentView.layer.cornerRadius = 0.0;
  };

  PropertyAnimatorGroup* animations = [[PropertyAnimatorGroup alloc] init];

  // Motion animation runs for the whole duration with a spring effect.
  // Because of the spring effect, the total duration needs to be quite long
  // or else the animation will feel very abrupt.
  UIViewPropertyAnimator* motionAnimation =
      [[UIViewPropertyAnimator alloc] initWithDuration:kAnimationDuration
                                          dampingRatio:kTabMotionDamping
                                            animations:tabResizeAnimation];

  // Tab fades in over the first half of the overall animation, easing out (so
  // most of the fade happens sooner).
  auto tabfadeAnimationKeyframes = ^{
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:kTabFadeInRelativeDuration
                                  animations:tabFadeAnimation];

  };
  UIViewPropertyAnimator* tabfadeAnimation = [[UIViewPropertyAnimator alloc]
      initWithDuration:kAnimationDuration
                 curve:UIViewAnimationCurveEaseOut
            animations:^{
              [UIView animateKeyframesWithDuration:kAnimationDuration
                                             delay:0
                                           options:0
                                        animations:tabfadeAnimationKeyframes
                                        completion:nil];
            }];

  // Additional animations happen in the first third of the overall animation,
  // and are linear.
  auto additionalAnimationsKeyframes = ^{
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:kBackgroundFadeRelativeDuration
                                  animations:backgroundFadeAnimation];
    [UIView addKeyframeWithRelativeStartTime:0
                            relativeDuration:kCornerRoundingRelativeDuration
                                  animations:cornerAnimation];
  };
  UIViewPropertyAnimator* additionalAnimations = [[UIViewPropertyAnimator alloc]
      initWithDuration:kAnimationDuration
                 curve:UIViewAnimationCurveLinear
            animations:^{
              [UIView animateKeyframesWithDuration:kAnimationDuration
                                             delay:0
                                           options:0
                                        animations:additionalAnimationsKeyframes
                                        completion:nil];
            }];

  [animations addAnimator:motionAnimation];
  [animations addAnimator:tabfadeAnimation];
  [animations addAnimator:additionalAnimations];

  [animations addCompletion:^(UIViewAnimatingPosition finalPosition) {
    self.contentView.clipsToBounds = NO;
    completion();
  }];

  [animations startAnimation];
}

@end
