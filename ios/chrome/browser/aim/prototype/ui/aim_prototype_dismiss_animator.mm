// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_dismiss_animator.h"

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"

@implementation AIMPrototypeDismissAnimator {
  __weak id<AIMPrototypeAnimationContextProvider> _contextProvider;
}

- (instancetype)initWithContextProvider:
    (id<AIMPrototypeAnimationContextProvider>)contextProvider {
  self = [super init];
  if (self) {
    _contextProvider = contextProvider;
  }
  return self;
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.3;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* presentedView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];

  UIView* containerView = transitionContext.containerView;
  UIView* inputPlateView = [_contextProvider inputPlateViewForAnimation];

  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      animations:^{
        presentedView.alpha = 0.0;
        CGRect finalFrame = inputPlateView.frame;
        finalFrame.origin.y = containerView.bounds.size.height;
        inputPlateView.frame = finalFrame;
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:finished];
      }];
}

@end
