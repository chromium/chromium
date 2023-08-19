// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scanner/scanner_transitioning_delegate.h"

#import "base/ios/block_types.h"

namespace {

// The default animation duration.
const NSTimeInterval kDefaultDuration = 0.25;

enum ScannerTransition { PRESENT, DISMISS };

}  // namespace

// Animates the Scanner transition. If initialized with the `PRESENT`
// transition, positions the Scanner view below its presenting view
// controller's view in the container view and animates the presenting view to
// slide up. If initialized with the `DISMISS` transition, positions the
// presenting view controller's view above the Scanner view in the container
// view and animates the presenting view to slide down.
@interface ScannerTransitionAnimator
    : NSObject <UIViewControllerAnimatedTransitioning> {
  ScannerTransition _transition;
}

- (instancetype)initWithTransition:(ScannerTransition)transition;

@end

@implementation ScannerTransitionAnimator

- (instancetype)initWithTransition:(ScannerTransition)transition {
  self = [super init];
  if (self) {
    _transition = transition;
  }
  return self;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* containerView = [transitionContext containerView];
  UIView* presentedView =
      [transitionContext viewForKey:UITransitionContextToViewKey];
  UIView* presentingView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];

  // Get the final frame for the presented view.
  UIViewController* presentedViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  CGRect finalFrame =
      [transitionContext finalFrameForViewController:presentedViewController];

  ProceduralBlock animations;

  // Set the frame for the presented view.
  presentedView.frame = finalFrame;
  [presentedView layoutIfNeeded];

  switch (_transition) {
    case PRESENT: {
      [containerView insertSubview:presentedView belowSubview:presentingView];
      animations = ^void {
        presentingView.transform =
            CGAffineTransformMakeTranslation(0, -finalFrame.size.height);
      };
      break;
    }
    case DISMISS: {
      [containerView insertSubview:presentedView aboveSubview:presentingView];
      presentedView.transform =
          CGAffineTransformMakeTranslation(0, -finalFrame.size.height);
      animations = ^void {
        presentedView.transform = CGAffineTransformIdentity;
      };
      break;
    }
  }

  void (^completion)(BOOL) = ^void(BOOL finished) {
    presentingView.transform = CGAffineTransformIdentity;
    [transitionContext completeTransition:finished];
  };

  // Animate the transition.
  [UIView animateWithDuration:kDefaultDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:animations
                   completion:completion];
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return kDefaultDuration;
}

@end

@implementation ScannerTransitioningDelegate

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  return [[ScannerTransitionAnimator alloc] initWithTransition:PRESENT];
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  return [[ScannerTransitionAnimator alloc] initWithTransition:DISMISS];
}

@end
