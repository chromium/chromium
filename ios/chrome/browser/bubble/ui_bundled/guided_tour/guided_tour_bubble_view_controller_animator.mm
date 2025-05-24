// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_animator.h"

@implementation GuidedTourBubbleViewControllerAnimator

#pragma mark - UIViewControllerAnimatedTransitioning

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
  if (self.appearing) {
    [containerView addSubview:presentedView];
    presentedView.frame =
        [transitionContext finalFrameForViewController:presentedViewController];
  }

  if (self.appearing) {
    presentedView.alpha = 0;
  }

  [UIView animateWithDuration:.5
      delay:0
      usingSpringWithDamping:.85
      initialSpringVelocity:0
      options:0
      animations:^{
        presentedView.alpha = self.appearing ? 1 : 0;
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:YES];
      }];
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.5;
}

@end
