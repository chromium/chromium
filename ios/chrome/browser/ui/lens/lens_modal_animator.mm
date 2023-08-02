// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/lens/lens_modal_animator.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"

// The time it takes for the presenting animation to complete.
static const CGFloat kPresentingTransitionAnimationDuration = .4;

// The time it takes for the dismissal animation to complete.
static const CGFloat kDismissalTransitionAnimationDuration = .3;

// Motion curves for the presentation animation.
static const CGFloat kPresentationAnimationCurve0 = 0.05;
static const CGFloat kPresentationAnimationCurve1 = 0.7;
static const CGFloat kPresentationAnimationCurve2 = 0.1;
static const CGFloat kPresentationAnimationCurve3 = 1.;

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
  return self;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return self;
}

#pragma mark - UIViewControllerAnimatedTransitioning

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return [self isPresentingFromContext:transitionContext]
             ? kPresentingTransitionAnimationDuration
             : kDismissalTransitionAnimationDuration;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  BOOL presenting = [self isPresentingFromContext:transitionContext];
  if (presenting) {
    [self animatePresenting:transitionContext];
  } else {
    [self animateDismissal:transitionContext];
  }
}

#pragma mark - Private methods

- (void)animatePresenting:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* containerView = [transitionContext containerView];
  UIView* lensView =
      [transitionContext viewForKey:UITransitionContextToViewKey];
  UIViewController* lensViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];

  [containerView addSubview:lensView];
  [containerView bringSubviewToFront:lensView];
  CGRect startingFrame = containerView.bounds;
  if (self.presentationStyle ==
      LensInputSelectionPresentationStyle::SlideFromLeft) {
    startingFrame.origin.x = -startingFrame.size.width;
  } else {
    startingFrame.origin.x = containerView.bounds.size.width;
  }

  lensView.frame = startingFrame;

  UIViewPropertyAnimator* animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:[self transitionDuration:transitionContext]
         controlPoint1:CGPointMake(kPresentationAnimationCurve0,
                                   kPresentationAnimationCurve1)
         controlPoint2:CGPointMake(kPresentationAnimationCurve2,
                                   kPresentationAnimationCurve3)
            animations:^{
              lensView.frame = [transitionContext
                  finalFrameForViewController:lensViewController];
            }];

  // Completions should only be run once.
  const ProceduralBlock presentationCompletion = _presentationCompletion;
  _presentationCompletion = nil;

  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    BOOL success = ![transitionContext transitionWasCancelled];

    if (success && presentationCompletion) {
      dispatch_async(dispatch_get_main_queue(), ^{
        presentationCompletion();
      });
    }

    [transitionContext completeTransition:success];
  }];
  [animator startAnimation];
}

- (void)animateDismissal:
    (id<UIViewControllerContextTransitioning>)transitionContext {
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

- (BOOL)isPresentingFromContext:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIViewController* fromViewController = [transitionContext
      viewControllerForKey:UITransitionContextFromViewControllerKey];
  UIViewController* toViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIViewController* toPresentingViewController =
      toViewController.presentingViewController;
  return (toPresentingViewController == fromViewController) ? YES : NO;
}

@end
