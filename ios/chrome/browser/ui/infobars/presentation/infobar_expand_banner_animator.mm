// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_expand_banner_animator.h"

@implementation InfobarExpandBannerAnimator

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 1;
}

// TODO(crbug.com/40061288): PLACEHOLDER animation for the modal presentation.
- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Set up the keys for the "base" view/VC and the "presented" view/VC. These
  // will be used to fetch the associated objects later.
  NSString* baseViewKey = self.presenting ? UITransitionContextFromViewKey
                                          : UITransitionContextToViewKey;
  NSString* presentedViewControllerKey =
      self.presenting ? UITransitionContextToViewControllerKey
                      : UITransitionContextFromViewControllerKey;
  NSString* presentedViewKey = self.presenting ? UITransitionContextToViewKey
                                               : UITransitionContextFromViewKey;

  NSString* presentingVCKey = UITransitionContextFromViewControllerKey;
  UIViewController* presentingViewController =
      [transitionContext viewControllerForKey:presentingVCKey];

  // Get views and view controllers for this transition.
  UIView* baseView = [transitionContext viewForKey:baseViewKey];
  UIViewController* presentedViewController =
      [transitionContext viewControllerForKey:presentedViewControllerKey];
  UIView* presentedView = [transitionContext viewForKey:presentedViewKey];

  // Always add the destination view to the container.
  UIView* containerView = [transitionContext containerView];
  if (self.presenting) {
    [containerView addSubview:presentedView];
  } else {
    [containerView addSubview:baseView];
  }

  // Set the initial frame and Compute the final frame for the presented view.
  CGRect presentedViewFinalFrame = CGRectZero;

  // If presenting, calculate the presentedView start and final frames.
  if (self.presenting) {
    presentedViewFinalFrame =
        [transitionContext finalFrameForViewController:presentedViewController];

    CGRect bannerFrame = presentingViewController.view.frame;
    CGRect initialFrame = presentedView.frame;
    initialFrame.size.height = bannerFrame.size.height;
    presentedView.frame = initialFrame;
  }

  // Animate using the animator's own duration value.
  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      delay:0
      usingSpringWithDamping:0.85
      initialSpringVelocity:0
      options:UIViewAnimationOptionTransitionNone
      animations:^{
        if (self.presenting) {
          presentedView.frame = presentedViewFinalFrame;
          presentingViewController.view.alpha = 0;
        } else {
          presentedViewController.view.alpha = 0;
        }
      }
      completion:^(BOOL finished) {
        BOOL success = ![transitionContext transitionWasCancelled];

        // If presentation failed, remove the view.
        if (self.presenting && !success) {
          [presentedView removeFromSuperview];
        }

        // If dismissmal was successfull, remove the view.
        if (!self.presenting && success) {
          [presentedView removeFromSuperview];
        }

        // Notify UIKit that the transition has finished
        [transitionContext completeTransition:success];
      }];
}

@end
