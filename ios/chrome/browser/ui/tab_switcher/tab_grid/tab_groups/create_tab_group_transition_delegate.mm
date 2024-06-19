// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_transition_delegate.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {
const CGFloat kPresentationDuration = 0.25;
const CGFloat kDismissalDuration = 0.20;
const CGFloat kBackgroundAlpha = 0.1;
const CGFloat kScaleFactor = 0.8;
const CGFloat kShortDurationFactor = 0.9;

// Tags for the different views.
const NSInteger kBackgroundTag = 1;
const NSInteger kVisualEffectTag = 2;

// Returns the subview of `superview` with a `tag`.
UIView* ViewWithTag(NSInteger tag, UIView* superview) {
  for (UIView* view in superview.subviews) {
    if (view.tag == tag) {
      return view;
    }
  }
  return nil;
}
}  // namespace

@interface CreateTabGroupTransitionAnimator
    : NSObject <UIViewControllerAnimatedTransitioning>

@property(nonatomic, assign) BOOL appearing;

@end

@implementation CreateTabGroupTransitionAnimator

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIViewController* presentedViewController = [transitionContext
      viewControllerForKey:self.appearing
                               ? UITransitionContextToViewControllerKey
                               : UITransitionContextFromViewControllerKey];

  UIView* presentedView = [transitionContext
      viewForKey:self.appearing ? UITransitionContextToViewKey
                                : UITransitionContextFromViewKey];

  UIView* containerView = [transitionContext containerView];

  NSTimeInterval duration = [self transitionDuration:transitionContext];
  if (self.appearing) {
    CGRect finalFrame =
        [transitionContext finalFrameForViewController:presentedViewController];

    UIView* backgroundView = [[UIView alloc] initWithFrame:finalFrame];
    backgroundView.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    backgroundView.alpha = 0;
    backgroundView.tag = kBackgroundTag;
    UIVisualEffectView* blurEffectView;

    if (!UIAccessibilityIsReduceTransparencyEnabled()) {
      blurEffectView = [[UIVisualEffectView alloc] initWithEffect:nil];
      blurEffectView.tag = kVisualEffectTag;
      blurEffectView.frame = backgroundView.bounds;
      blurEffectView.autoresizingMask =
          UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
      backgroundView.backgroundColor = [[UIColor colorNamed:kGrey200Color]
          colorWithAlphaComponent:kBackgroundAlpha];
      [backgroundView addSubview:blurEffectView];
    } else {
      backgroundView.backgroundColor = [UIColor blackColor];
    }

    [containerView addSubview:backgroundView];
    [containerView addSubview:presentedView];

    presentedView.frame = finalFrame;
    presentedView.alpha = 0;
    presentedView.transform =
        CGAffineTransformMakeScale(kScaleFactor, kScaleFactor);

    [UIView animateWithDuration:duration
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          presentedView.alpha = 1;
          presentedView.transform = CGAffineTransformIdentity;
        }
        completion:^(BOOL finished) {
          [transitionContext completeTransition:YES];
        }];

    [UIView animateWithDuration:duration * kShortDurationFactor
                          delay:0
                        options:UIViewAnimationCurveEaseOut
                     animations:^{
                       UIBlurEffect* blurEffect = [UIBlurEffect
                           effectWithStyle:UIBlurEffectStyleSystemMaterial];
                       blurEffectView.effect = blurEffect;
                       backgroundView.alpha = 1;
                     }
                     completion:^(BOOL finished){
                         // No action here as it will finish first.
                     }];

  } else {
    UIView* backgroundView = ViewWithTag(kBackgroundTag, containerView);
    UIVisualEffectView* visualEffectView =
        base::apple::ObjCCast<UIVisualEffectView>(
            ViewWithTag(kVisualEffectTag, backgroundView));

    [UIView animateWithDuration:duration * kShortDurationFactor
                          delay:0
                        options:UIViewAnimationCurveEaseOut
                     animations:^{
                       presentedView.alpha = 0;
                       presentedView.transform = CGAffineTransformMakeScale(
                           kScaleFactor, kScaleFactor);
                     }
                     completion:^(BOOL finished){
                         // No action here as it will finish first.
                     }];

    [UIView animateWithDuration:duration
        delay:0
        options:UIViewAnimationCurveEaseIn
        animations:^{
          backgroundView.alpha = 0;
          visualEffectView.effect = nil;
        }
        completion:^(BOOL finished) {
          [presentedView removeFromSuperview];
          [transitionContext completeTransition:YES];
        }];
  }
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return self.appearing ? kPresentationDuration : kDismissalDuration;
}

@end

@implementation CreateTabGroupTransitionDelegate

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  CreateTabGroupTransitionAnimator* animator =
      [[CreateTabGroupTransitionAnimator alloc] init];
  animator.appearing = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  CreateTabGroupTransitionAnimator* animator =
      [[CreateTabGroupTransitionAnimator alloc] init];
  animator.appearing = NO;
  return animator;
}

@end
