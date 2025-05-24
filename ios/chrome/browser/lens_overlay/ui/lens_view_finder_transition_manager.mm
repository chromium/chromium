// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_view_finder_transition_manager.h"

namespace {

// The time it takes for the animation to complete.
static const CGFloat kTransitionAnimationDuration = .35;

// The diming percentage of the base view during the custom presentation.
static const CGFloat kLVFPresentationDimmingPercentage = .5;

// Motion curves for the presentation animation (ease-out).
static const CGFloat kPresentationAnimationCurve0 = 0.25;
static const CGFloat kPresentationAnimationCurve1 = 0.46;
static const CGFloat kPresentationAnimationCurve2 = 0.45;
static const CGFloat kPresentationAnimationCurve3 = 0.94;

}  // namespace

@implementation LensViewFinderTransitionManager {
  LensViewFinderTransition _transitionType;
}

- (instancetype)initWithLVFTransitionType:
    (LensViewFinderTransition)transitionType {
  self = [super init];
  if (self) {
    _transitionType = transitionType;
  }

  return self;
}

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
  return kTransitionAnimationDuration;
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

- (void)animateTransitionWithContext:
            (id<UIViewControllerContextTransitioning>)transitionContext
                          animations:(void (^)())animations
                          completion:(void (^)())completion {
  UIViewPropertyAnimator* animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:[self transitionDuration:transitionContext]
         controlPoint1:CGPointMake(kPresentationAnimationCurve0,
                                   kPresentationAnimationCurve1)
         controlPoint2:CGPointMake(kPresentationAnimationCurve2,
                                   kPresentationAnimationCurve3)
            animations:animations];

  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    if (completion) {
      completion();
    }
    BOOL success = ![transitionContext transitionWasCancelled];
    [transitionContext completeTransition:success];
  }];

  [animator startAnimation];
}

- (void)animatePresenting:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* containerView = [transitionContext containerView];
  UIView* lensView =
      [transitionContext viewForKey:UITransitionContextToViewKey];
  UIViewController* lensViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];

  UIView* initialView =
      [transitionContext
          viewControllerForKey:UITransitionContextFromViewControllerKey]
          .view;
  UIView* initialViewSnapshot =
      [initialView snapshotViewAfterScreenUpdates:YES];

  [containerView addSubview:initialViewSnapshot];
  [containerView addSubview:lensView];
  [containerView bringSubviewToFront:lensView];

  CGRect startingFrameForLVF = containerView.bounds;
  CGRect finalFrameForBase = initialView.bounds;

  BOOL isLeftSlide = _transitionType == LensViewFinderTransitionSlideFromLeft;
  int gain = isLeftSlide ? 1 : -1;
  finalFrameForBase.origin.x = gain * finalFrameForBase.size.width / 2;
  startingFrameForLVF.origin.x = -gain * startingFrameForLVF.size.width;

  lensView.layer.masksToBounds = YES;
  lensView.frame = startingFrameForLVF;

  UIView* dimmingView =
      [[UIView alloc] initWithFrame:initialViewSnapshot.frame];
  dimmingView.backgroundColor = [UIColor blackColor];
  dimmingView.alpha = 0;
  [initialViewSnapshot addSubview:dimmingView];

  [self animateTransitionWithContext:transitionContext
      animations:^{
        initialViewSnapshot.frame = finalFrameForBase;
        dimmingView.alpha = kLVFPresentationDimmingPercentage;
        lensView.frame =
            [transitionContext finalFrameForViewController:lensViewController];
      }
      completion:^{
        [initialViewSnapshot removeFromSuperview];
      }];
}

- (void)animateDismissal:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* containerView = [transitionContext containerView];
  UIView* lensView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];
  UIViewController* targetViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIView* targetView = targetViewController.view;
  UIView* targetViewSnapshot = [targetView snapshotViewAfterScreenUpdates:YES];

  [containerView addSubview:targetViewSnapshot];
  [containerView bringSubviewToFront:lensView];

  CGRect finalFrameForLVF = containerView.bounds;
  CGRect startingFrameForSnapshot = containerView.bounds;

  BOOL isLeftSlide = _transitionType == LensViewFinderTransitionSlideFromLeft;
  int gain = isLeftSlide ? 1 : -1;
  finalFrameForLVF.origin.x = -gain * finalFrameForLVF.size.width;
  startingFrameForSnapshot.origin.x =
      gain * startingFrameForSnapshot.size.width / 2;
  targetViewSnapshot.frame = startingFrameForSnapshot;

  UIView* dimmingView = [[UIView alloc] initWithFrame:targetViewSnapshot.frame];
  dimmingView.backgroundColor = [UIColor blackColor];
  dimmingView.alpha = kLVFPresentationDimmingPercentage;

  [targetViewSnapshot addSubview:dimmingView];

  [self animateTransitionWithContext:transitionContext
      animations:^{
        lensView.frame = finalFrameForLVF;
        dimmingView.alpha = 0;
        dimmingView.frame = [transitionContext
            finalFrameForViewController:targetViewController];
        targetViewSnapshot.frame = [transitionContext
            finalFrameForViewController:targetViewController];
      }
      completion:^{
        [targetViewSnapshot removeFromSuperview];
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
  return toPresentingViewController == fromViewController;
}

@end
