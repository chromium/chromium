// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_dismiss_animator.h"

#import "ios/chrome/browser/composebox/ui/composebox_animation_context.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@implementation ComposeboxDismissAnimator {
  __weak id<ComposeboxAnimationContext> _contextProvider;
  __weak id<ComposeboxAnimationBase> _animationBase;
}

- (instancetype)
    initWithContextProvider:(id<ComposeboxAnimationContext>)contextProvider
              animationBase:(id<ComposeboxAnimationBase>)animationBase {
  self = [super init];
  if (self) {
    _contextProvider = contextProvider;
    _animationBase = animationBase;
  }
  return self;
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.3;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* entrypointCopy = [_animationBase entrypointViewVisualCopy];
  UIView* fromView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];

  BOOL isLandscape = IsLandscape(fromView.window);
  BOOL compact = [_contextProvider inputPlateIsCompact];
  // If the final state is too far visually from the current input plate state,
  // use the simplified animation.
  if (!entrypointCopy || isLandscape || !compact) {
    [self animateTransitionWithoutSharedElement:transitionContext];
    return;
  }

  UIView* transitionContainerView = [transitionContext containerView];
  UIView* inputPlateView = [_contextProvider inputPlateViewForAnimation];
  UIView* closeButton = [_contextProvider closeButtonForAnimation];
  UIView* popupView = [_contextProvider popupViewForAnimation];
  [transitionContainerView addSubview:entrypointCopy];
  [entrypointCopy layoutIfNeeded];
  entrypointCopy.alpha = 0;

  [_contextProvider expandInputPlateForDismissal];
  [UIView
      animateKeyframesWithDuration:[self transitionDuration:transitionContext]
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        // Morphing the input plate.
        [UIView addKeyframeWithRelativeStartTime:0
                                relativeDuration:0.5
                                      animations:^{
                                        [inputPlateView.superview
                                                .superview layoutIfNeeded];
                                      }];
        [UIView addKeyframeWithRelativeStartTime:0.2
                                relativeDuration:0.6
                                      animations:^{
                                        inputPlateView.transform =
                                            CGAffineTransformMakeScale(1, 0.9);
                                      }];
        [UIView
            addKeyframeWithRelativeStartTime:0.3
                            relativeDuration:0.6
                                  animations:^{
                                    inputPlateView.alpha = 0;
                                    closeButton.alpha = 0;
                                    closeButton.transform =
                                        CGAffineTransformMakeScale(0.9, 0.9);
                                  }];
        // Briefly show the copy then fade the entire composebox view.
        [UIView addKeyframeWithRelativeStartTime:0.5
                                relativeDuration:0.3
                                      animations:^{
                                        entrypointCopy.alpha = 1;
                                      }];
        [UIView addKeyframeWithRelativeStartTime:0
                                relativeDuration:0.8
                                      animations:^{
                                        popupView.alpha = 0.2;
                                      }];
        [UIView addKeyframeWithRelativeStartTime:0.8
                                relativeDuration:0.2
                                      animations:^{
                                        fromView.alpha = 0;
                                        entrypointCopy.alpha = 0;
                                      }];
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:finished];
      }];
}

- (void)animateTransitionWithoutSharedElement:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* fromView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];
  UIView* inputPlateView = [_contextProvider inputPlateViewForAnimation];
  UIView* closeButton = [_contextProvider closeButtonForAnimation];
  BOOL isLandscape = IsLandscape(fromView.window);

  CGFloat scaleAmmount = isLandscape ? 0.7 : 0.9;
  [UIView
      animateKeyframesWithDuration:[self transitionDuration:transitionContext]
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        // Morphing the input plate.
        [UIView addKeyframeWithRelativeStartTime:0
                                relativeDuration:1
                                      animations:^{
                                        fromView.alpha = 0;
                                      }];
        [UIView addKeyframeWithRelativeStartTime:0.2
                                relativeDuration:0.6
                                      animations:^{
                                        inputPlateView.transform =
                                            CGAffineTransformMakeScale(
                                                scaleAmmount, scaleAmmount);
                                        closeButton.transform =
                                            CGAffineTransformMakeScale(
                                                scaleAmmount, scaleAmmount);
                                      }];
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:finished];
      }];
}

@end
