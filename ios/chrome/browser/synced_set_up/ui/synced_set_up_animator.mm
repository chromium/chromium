// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_animator.h"

#import "ios/chrome/common/ui/animations/radial_wipe_animation.h"

namespace {

// Animation duration when dismissing the UI.
const CGFloat kDismissAnimationDuration = 1.0;
// Animation duration when presenting the UI.
const CGFloat kPresentAnimationDuration = 0.4;

}  // namespace

@implementation SyncedSetUpAnimator {
  // Indicates the direction of the transition.
  BOOL _isPresenting;
  // The object responsible for animating the interstitial.
  RadialWipeAnimation* _animation;
}

- (instancetype)initForPresenting:(BOOL)isPresenting {
  if ((self = [super init])) {
    _isPresenting = isPresenting;
  }
  return self;
}

#pragma mark - UIViewControllerAnimatedTransitioning

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return _isPresenting ? kPresentAnimationDuration : kDismissAnimationDuration;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIWindow* window = [self windowFromContext:transitionContext];

  if (_isPresenting) {
    [self animatePresentationWithContext:transitionContext window:window];
  } else {
    [self animateDismissalWithContext:transitionContext window:window];
  }
}

#pragma mark - Private methods

- (UIWindow*)windowFromContext:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* containerView = [transitionContext containerView];
  UIWindow* window = containerView.window;
  if (window) {
    return window;
  }

  UIViewController* toVC = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIViewController* fromVC = [transitionContext
      viewControllerForKey:UITransitionContextFromViewControllerKey];

  return _isPresenting ? toVC.view.window : fromVC.view.window;
}

- (void)animatePresentationWithContext:
            (id<UIViewControllerContextTransitioning>)transitionContext
                                window:(UIWindow*)window {
  UIViewController* toVC = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIView* animatedView = toVC.view;
  UIView* containerView = [transitionContext containerView];

  [containerView addSubview:animatedView];
  animatedView.frame = [transitionContext finalFrameForViewController:toVC];

  // Initial state for presentation: scaled down and transparent.
  animatedView.transform = CGAffineTransformMakeScale(0.8, 0.8);
  animatedView.alpha = 0.0;

  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      delay:0.0
      usingSpringWithDamping:0.8
      initialSpringVelocity:0.1
      options:UIViewAnimationOptionCurveEaseInOut
      animations:^{
        // Animate to the final state: full size and opaque.
        animatedView.transform = CGAffineTransformIdentity;
        animatedView.alpha = 1.0;
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:YES];
      }];
}

- (void)animateDismissalWithContext:
            (id<UIViewControllerContextTransitioning>)transitionContext
                             window:(UIWindow*)window {
  UIViewController* fromVC = [transitionContext
      viewControllerForKey:UITransitionContextFromViewControllerKey];
  UIView* animatedView = fromVC.view;

  _animation = [[RadialWipeAnimation alloc] initWithWindow:window
                                               targetViews:@[ animatedView ]];
  _animation.startPoint = CGPointMake(0.5, 0.0);
  _animation.type = RadialWipeAnimationType::kHideTarget;

  window.userInteractionEnabled = NO;

  __weak __typeof(self) weakSelf = self;
  [_animation animateWithCompletion:^{
    window.userInteractionEnabled = YES;
    [animatedView removeFromSuperview];
    [transitionContext completeTransition:YES];
    [weakSelf onAnimationCompleted];
  }];
}

// Called when the animation has completed to properly clear any obsolete state.
- (void)onAnimationCompleted {
  _animation = nil;
}

@end
