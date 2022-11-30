// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_modal_animator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The time it takes for the animation to complete.
static const CGFloat kTransitionAnimationDuration = .4;

@interface LensModalAnimator ()

@end

@implementation LensModalAnimator

#pragma mark - UIViewControllerTransitioningDelegate

- (id<UIViewControllerInteractiveTransitioning>)
    interactionControllerForPresentation:
        (id<UIViewControllerAnimatedTransitioning>)animator {
  return nil;
}

- (id<UIViewControllerInteractiveTransitioning>)
    interactionControllerForDismissal:
        (id<UIViewControllerAnimatedTransitioning>)animator {
  return nil;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  return nil;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  // Only use this transitioning delegate for Lens modal dismissal.
  return self;
}

#pragma mark - UIViewControllerAnimatedTransitioning

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return kTransitionAnimationDuration;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // This animator will only be used when dismissing Lens.
  UIView* containerView = [transitionContext containerView];
  UIView* toView = [transitionContext viewForKey:UITransitionContextToViewKey];
  UIView* lensView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];

  // Create and add a Lens view snapshot.
  UIView* lensViewSnapshot = [lensView snapshotViewAfterScreenUpdates:YES];

  [containerView addSubview:toView];
  [containerView addSubview:lensViewSnapshot];
  [lensView removeFromSuperview];
  [containerView bringSubviewToFront:lensViewSnapshot];

  lensViewSnapshot.alpha = 1.0;

  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      animations:^{
        lensViewSnapshot.alpha = 0.0;
      }

      completion:^(BOOL finished) {
        BOOL success = ![transitionContext transitionWasCancelled];

        if (success) {
          [lensViewSnapshot removeFromSuperview];
        }

        [transitionContext completeTransition:success];
      }];
}

@end
