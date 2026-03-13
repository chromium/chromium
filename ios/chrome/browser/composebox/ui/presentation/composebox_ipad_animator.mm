// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/presentation/composebox_ipad_animator.h"

#import "base/time/time.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
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
          omniboxFrame.origin.y,
          omniboxFrame.size.width + kInputPlateMargin * 2,
          omniboxFrame.size.height);
    }

    toViewController.view.frame = initialFrame;

    BOOL showAIMode = self.showAIMode;
    __weak ComposeboxiPadAnimator* weakSelf = self;
    [UIView
        animateKeyframesWithDuration:[self transitionDuration:transitionContext]
        delay:0
        options:UIViewAnimationCurveEaseInOut
        animations:^{
          toViewController.view.alpha = 1;
          toViewController.view.frame = finalFrame;
          if (showAIMode) {
            [UIView addKeyframeWithRelativeStartTime:0.5
                                    relativeDuration:0.5
                                          animations:^{
                                            [weakSelf.delegate
                                                setComposeboxMode:
                                                    ComposeboxMode::kAIM];
                                          }];
          }
        }
        completion:^(BOOL finished) {
          [transitionContext completeTransition:finished];
        }];
  } else {
    [UIView animateWithDuration:[self transitionDuration:transitionContext]
        animations:^{
          fromViewController.view.alpha = 0;
        }
        completion:^(BOOL finished) {
          [fromViewController.view removeFromSuperview];
          [transitionContext completeTransition:finished];
        }];
  }
}

@end
