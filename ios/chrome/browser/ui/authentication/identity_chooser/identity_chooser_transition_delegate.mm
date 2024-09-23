// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_transition_delegate.h"

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_animator.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_presentation_controller.h"

@implementation IdentityChooserTransitionDelegate

@synthesize origin = _origin;

#pragma mark - UIViewControllerTransitioningDelegate

- (UIPresentationController*)
presentationControllerForPresentedViewController:(UIViewController*)presented
                        presentingViewController:(UIViewController*)presenting
                            sourceViewController:(UIViewController*)source {
  return [[IdentityChooserPresentationController alloc]
      initWithPresentedViewController:presented
             presentingViewController:presenting];
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForPresentedController:(UIViewController*)presented
                     presentingController:(UIViewController*)presenting
                         sourceController:(UIViewController*)source {
  IdentityChooserAnimator* animator = [[IdentityChooserAnimator alloc] init];
  animator.appearing = YES;
  animator.origin = self.origin;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForDismissedController:(UIViewController*)dismissed {
  IdentityChooserAnimator* animator = [[IdentityChooserAnimator alloc] init];
  animator.appearing = NO;
  return animator;
}

@end
