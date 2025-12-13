// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_info_message_animator.h"

namespace {

// The duration of the opacity transition in the results page.
const CGFloat kOpacityAnimationDuration = 0.4;

}  // namespace

@implementation LensOverlayInfoMessageAnimator {
  // The navigation operation for the animator.
  UINavigationControllerOperation _operation;
}

- (instancetype)initWithOperation:(UINavigationControllerOperation)operation {
  self = [super init];
  if (self) {
    _operation = operation;
  }

  return self;
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return kOpacityAnimationDuration;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIViewController* toViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIViewController* fromViewController = [transitionContext
      viewControllerForKey:UITransitionContextFromViewControllerKey];
  NSTimeInterval duration = [self transitionDuration:transitionContext];
  auto animationFinished = ^(BOOL finished) {
    [transitionContext
        completeTransition:![transitionContext transitionWasCancelled]];
  };

  if (_operation == UINavigationControllerOperationPush) {
    [[transitionContext containerView] addSubview:toViewController.view];
    toViewController.view.alpha = 0.0;
    [UIView animateWithDuration:duration
                     animations:^{
                       toViewController.view.alpha = 1.0;
                     }
                     completion:animationFinished];

    return;
  }

  if (_operation == UINavigationControllerOperationPop) {
    [[transitionContext containerView] insertSubview:toViewController.view
                                        belowSubview:fromViewController.view];

    [UIView animateWithDuration:duration
                     animations:^{
                       fromViewController.view.alpha = 0.0;
                     }
                     completion:animationFinished];
    return;
  }

  animationFinished(YES);
}

@end
