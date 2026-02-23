// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/presentation/composebox_ipad_animator.h"

#import "base/time/time.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

namespace {
base::TimeDelta kAnimationDuration = base::Seconds(0.3);
}

@implementation ComposeboxiPadAnimator

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return kAnimationDuration.InSecondsF();
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* containerView = [transitionContext containerView];
  UIViewController* toViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIViewController* fromViewController = [transitionContext
      viewControllerForKey:UITransitionContextFromViewControllerKey];

  if (self.presenting) {
    [containerView addSubview:toViewController.view];

    LayoutGuideCenter* layoutGuideCenter = self.layoutGuideCenter;

    UIView* topOmnibox =
        [layoutGuideCenter referencedViewUnderName:kTopOmniboxGuide];
    CGRect omniboxFrame = [topOmnibox convertRect:topOmnibox.bounds
                                           toView:containerView];
    toViewController.view.alpha = 0;

    CGRect finalFrame =
        [transitionContext finalFrameForViewController:toViewController];
    CGRect initialFrame = omniboxFrame;
    if (self.shouldUseLargeLayout) {
      initialFrame = CGRectMake(
          omniboxFrame.origin.x - kComposeboxOmniboxLayoutGuideHorizontalMargin,
          omniboxFrame.origin.x, omniboxFrame.size.width,
          omniboxFrame.size.height);
    }

    toViewController.view.frame = initialFrame;

    [UIView animateWithDuration:[self transitionDuration:transitionContext]
        delay:0
        usingSpringWithDamping:0.8
        initialSpringVelocity:0
        options:UIViewAnimationOptionCurveEaseInOut
        animations:^{
          toViewController.view.alpha = 1;
          toViewController.view.frame = finalFrame;
        }
        completion:^(BOOL finished) {
          [transitionContext completeTransition:finished];
        }];

  } else {
    LayoutGuideCenter* layoutGuideCenter = self.layoutGuideCenter;
    UIView* topOmnibox =
        [layoutGuideCenter referencedViewUnderName:kTopOmniboxGuide];
    CGRect omniboxFrame = [topOmnibox convertRect:topOmnibox.bounds
                                           toView:containerView];
    if (self.shouldUseLargeLayout) {
      omniboxFrame = CGRectMake(
          omniboxFrame.origin.x - kComposeboxOmniboxLayoutGuideHorizontalMargin,
          omniboxFrame.origin.x, omniboxFrame.size.width,
          omniboxFrame.size.height);
    }

    [UIView animateWithDuration:[self transitionDuration:transitionContext]
        animations:^{
          fromViewController.view.alpha = 0;
          fromViewController.view.frame = omniboxFrame;
        }
        completion:^(BOOL finished) {
          [fromViewController.view removeFromSuperview];
          [transitionContext completeTransition:finished];
        }];
  }
}

@end
