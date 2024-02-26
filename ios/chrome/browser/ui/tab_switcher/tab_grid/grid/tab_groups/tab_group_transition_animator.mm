// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_transition_animator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"

@implementation TabGroupTransitionAnimator

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
    [containerView addSubview:presentedView];
    presentedView.frame =
        [transitionContext finalFrameForViewController:presentedViewController];

    [UIView animateWithDuration:duration
        animations:^{
          // The presented view controller frame doesn't animate. The animations
          // are occurring inside the PresentationController.
        }
        completion:^(BOOL finished) {
          [transitionContext completeTransition:YES];
        }];
  } else {
    [UIView animateWithDuration:duration
        animations:^{
          // The presented view controller frame doesn't animate. The animations
          // are occurring inside the PresentationController.
          presentedView.alpha = 0;
        }
        completion:^(BOOL finished) {
          [transitionContext completeTransition:YES];
        }];
  }
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return self.appearing ? kTabGroupPresentationDuration
                        : kTabGroupDismissalDuration;
}

@end
