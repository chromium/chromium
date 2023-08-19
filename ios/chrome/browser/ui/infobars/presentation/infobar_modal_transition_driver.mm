// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"

#import "ios/chrome/browser/ui/infobars/presentation/infobar_expand_banner_animator.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_presentation_controller.h"

@implementation InfobarModalTransitionDriver

- (instancetype)initWithTransitionMode:(InfobarModalTransition)transitionMode {
  self = [super init];
  if (self) {
    _transitionMode = transitionMode;
  }
  return self;
}

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  InfobarModalPresentationController* presentationController =
      [[InfobarModalPresentationController alloc]
          initWithPresentedViewController:presented
                 presentingViewController:presenting
                          modalPositioner:self.modalPositioner];
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  switch (self.transitionMode) {
    case InfobarModalTransitionBase:
      return nil;

    case InfobarModalTransitionBanner:
      InfobarExpandBannerAnimator* animator =
          [[InfobarExpandBannerAnimator alloc] init];
      animator.presenting = YES;
      return animator;
  }
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  // When dismissing the modal ViewController the default UIKit dismiss
  // animation is used.
  return nil;
}

@end
