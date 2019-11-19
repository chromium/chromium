// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_animator.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_presentation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarBannerTransitionDriver ()
// Object that handles the animation and interactivity for the Banner
// presentation.
@property(nonatomic, weak) InfobarBannerAnimator* bannerAnimator;
@end

@implementation InfobarBannerTransitionDriver

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  InfobarBannerPresentationController* presentationController =
      [[InfobarBannerPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting];
  presentationController.bannerPositioner = self.bannerPositioner;
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  InfobarBannerAnimator* animator = [[InfobarBannerAnimator alloc] init];
  animator.presenting = YES;
  self.bannerAnimator = animator;
  return self.bannerAnimator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  InfobarBannerAnimator* animator = [[InfobarBannerAnimator alloc] init];
  animator.presenting = NO;
  self.bannerAnimator = animator;
  return self.bannerAnimator;
}

- (id<UIViewControllerInteractiveTransitioning>)
    interactionControllerForPresentation:
        (id<UIViewControllerAnimatedTransitioning>)animator {
  DCHECK_EQ(self.bannerAnimator, animator);
  return self.bannerAnimator;
}

- (id<UIViewControllerInteractiveTransitioning>)
    interactionControllerForDismissal:
        (id<UIViewControllerAnimatedTransitioning>)animator {
  DCHECK_EQ(self.bannerAnimator, animator);
  return self.bannerAnimator;
}

#pragma mark - Public Methods

- (void)completePresentationTransitionIfRunning {
  if (self.bannerAnimator.propertyAnimator.running) {
    [self.bannerAnimator.propertyAnimator stopAnimation:NO];
    [self.bannerAnimator.propertyAnimator
        finishAnimationAtPosition:UIViewAnimatingPositionCurrent];
  }
}

#pragma mark - InfobarBannerInteractionDelegate

- (void)infobarBannerStartedInteraction {
  [self completePresentationTransitionIfRunning];
}

@end
