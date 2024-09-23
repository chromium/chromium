// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_animator.h"

namespace {
const CGFloat kAnimationDuration = 0.25;
const CGFloat kDamping = 0.85;
const CGFloat kScaleFactor = 0.1;
}  // namespace

@implementation IdentityChooserAnimator

@synthesize appearing = _appearing;
@synthesize origin = _origin;

#pragma mark - UIViewControllerAnimatedTransitioning

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // The VC being presented/dismissed and its view.
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
  CGPoint endCenter = presentedView.center;
  CGAffineTransform finalTransform;
  CGAffineTransform scaleDown =
      CGAffineTransformMakeScale(kScaleFactor, kScaleFactor);

  if (self.appearing) {
    if (!CGPointEqualToPoint(self.origin, CGPointZero)) {
      presentedView.center =
          [containerView convertPoint:self.origin fromView:nil];
    }
    presentedView.alpha = 0;
    presentedView.transform = scaleDown;
    finalTransform = CGAffineTransformIdentity;
  } else {
    finalTransform = scaleDown;
  }

  [UIView animateWithDuration:kAnimationDuration
      delay:0
      usingSpringWithDamping:kDamping
      initialSpringVelocity:0
      options:0
      animations:^{
        presentedView.center = endCenter;
        presentedView.transform = finalTransform;
        presentedView.alpha = self.appearing ? 1 : 0;
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:YES];
      }];
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return kAnimationDuration;
}

@end
