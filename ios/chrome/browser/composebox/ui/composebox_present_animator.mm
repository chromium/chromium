// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/composebox_present_animator.h"

#import "ios/chrome/browser/composebox/ui/composebox_animation_context.h"

namespace {
// The total duration of the presentation animation.
const NSTimeInterval kTotalDuration = 0.3;

// The scale starting value for elements appearing.
const NSTimeInterval kInitialScaleForAppear = 0.9;
}  // namespace

@implementation ComposeboxPresentAnimator {
  __weak id<ComposeboxAnimationContext> _context;
  __weak id<ComposeboxAnimationBase> _animationBase;
}

- (instancetype)initWithContext:(id<ComposeboxAnimationContext>)context
                  animationBase:(id<ComposeboxAnimationBase>)animationBase {
  self = [super init];
  if (self) {
    _context = context;
    _animationBase = animationBase;
  }
  return self;
}

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  if (UIAccessibilityIsReduceMotionEnabled()) {
    return 0.2;
  }
  return kTotalDuration;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIViewController* toViewController = [transitionContext
      viewControllerForKey:UITransitionContextToViewControllerKey];
  UIView* toView = toViewController.view;
  UIView* containerView = transitionContext.containerView;
  [containerView addSubview:toView];

  // Force a layout pass to ensure `inputPlateView` has its final frame.
  toView.frame =
      [transitionContext finalFrameForViewController:toViewController];
  // This is needed to ensure that the input is correctly sized and positioned.
  [toView layoutIfNeeded];

  UIView* inputPlateView = [_context inputPlateViewForAnimation];
  CGRect finalFrame = [inputPlateView convertRect:inputPlateView.frame
                                           toView:toView];

  BOOL toggleOnAIM = self.toggleOnAIM;
  __weak id<ComposeboxAnimationContext> context = _context;

  __weak id<ComposeboxAnimationBase> animationBase = _animationBase;

  UIView* transitionContainerView = [transitionContext containerView];
  UIView* entrypointCopy = [animationBase entrypointViewVisualCopy];
  BOOL compact = [_context inputPlateIsCompact];
  BOOL reduceMotion = UIAccessibilityIsReduceMotionEnabled();
  if (reduceMotion) {
    [self animateTransitionWithReducedMotion:transitionContext];
    return;
  }

  // Where the entrypoint is not available as a shared element, fallback to
  // a different transition.
  if (!entrypointCopy || !compact) {
    [self animateTransitionWithoutSharedElement:transitionContext];
    return;
  }

  [transitionContainerView addSubview:entrypointCopy];
  [entrypointCopy layoutIfNeeded];

  UIColor* finalBackgroundColor = inputPlateView.backgroundColor;
  CGFloat finalCornerRadius = inputPlateView.layer.cornerRadius;

  // Hide the view that real view that is being morphed for the duration of the
  // animation.
  [animationBase setEntrypointViewHidden:YES];

  context.inputPlateViewForAnimation.alpha = 0;
  context.closeButtonForAnimation.alpha = 0;
  context.closeButtonForAnimation.transform = CGAffineTransformMakeScale(
      kInitialScaleForAppear, kInitialScaleForAppear);

  toView.alpha = 1;

  [UIView
      animateKeyframesWithDuration:[self transitionDuration:transitionContext]
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        // Morph the initial entrypoint to the shape and position of the
        // inputplate.
        [UIView addKeyframeWithRelativeStartTime:0.0
                                relativeDuration:0.7
                                      animations:^{
                                        entrypointCopy.frame = finalFrame;
                                        entrypointCopy.backgroundColor =
                                            finalBackgroundColor;
                                        entrypointCopy.layer.cornerRadius =
                                            finalCornerRadius;
                                        [entrypointCopy layoutIfNeeded];
                                      }];
        // The content of the visual copy (but not the container itself)
        // start fading.
        [UIView addKeyframeWithRelativeStartTime:0.0
                                relativeDuration:0.7
                                      animations:^{
                                        for (UIView* entrypointCopySubview in
                                                 entrypointCopy.subviews) {
                                          entrypointCopySubview.alpha = 0;
                                        }
                                      }];
        // Fade the entrypoint copy once it reached position to reveal the
        // content of the inputplate.
        [UIView addKeyframeWithRelativeStartTime:0.7
                                relativeDuration:1.0
                                      animations:^{
                                        entrypointCopy.alpha = 0;
                                      }];
        // Scale and reveal the close button.
        [UIView
            addKeyframeWithRelativeStartTime:0.3
                            relativeDuration:0.9
                                  animations:^{
                                    context.closeButtonForAnimation.alpha = 1;
                                    context.closeButtonForAnimation.transform =
                                        CGAffineTransformMakeScale(1, 1);
                                  }];
        // Scale and reveal the close button.
        [UIView
            addKeyframeWithRelativeStartTime:0.7
                            relativeDuration:0.9
                                  animations:^{
                                    context.inputPlateViewForAnimation.alpha =
                                        1;
                                  }];
        // Enables AIM if needed.
        [UIView
            addKeyframeWithRelativeStartTime:0.2
                            relativeDuration:0.8
                                  animations:^{
                                    if (toggleOnAIM) {
                                      [context setComposeboxMode:
                                                   ComposeboxMode::kAIM];
                                    }
                                  }];
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:finished];
        [entrypointCopy removeFromSuperview];
        // Once the transition is complete the real view can be revealed in the
        // background.
        [animationBase setEntrypointViewHidden:NO];
      }];
}

- (void)animateTransitionWithoutSharedElement:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  __weak id<ComposeboxAnimationContext> context = _context;
  BOOL toggleOnAIM = self.toggleOnAIM;

  context.inputPlateViewForAnimation.alpha = 0;
  context.closeButtonForAnimation.alpha = 0;
  context.inputPlateViewForAnimation.transform = CGAffineTransformMakeScale(
      kInitialScaleForAppear, kInitialScaleForAppear);
  context.closeButtonForAnimation.transform = CGAffineTransformMakeScale(
      kInitialScaleForAppear, kInitialScaleForAppear);

  [UIView
      animateKeyframesWithDuration:[self transitionDuration:transitionContext]
      delay:0
      options:UIViewAnimationCurveEaseInOut
      animations:^{
        // Scale and reveal the close button and the input plate.
        [UIView
            addKeyframeWithRelativeStartTime:0
                            relativeDuration:1
                                  animations:^{
                                    context.closeButtonForAnimation.alpha = 1;
                                    context.closeButtonForAnimation.transform =
                                        CGAffineTransformMakeScale(1, 1);
                                    context.inputPlateViewForAnimation.alpha =
                                        1;
                                    context.inputPlateViewForAnimation
                                        .transform =
                                        CGAffineTransformMakeScale(1, 1);
                                  }];
        // Enables AIM if needed.
        [UIView
            addKeyframeWithRelativeStartTime:0.2
                            relativeDuration:0.8
                                  animations:^{
                                    if (toggleOnAIM) {
                                      [context setComposeboxMode:
                                                   ComposeboxMode::kAIM];
                                    }
                                  }];
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:finished];
      }];
}

- (void)animateTransitionWithReducedMotion:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  BOOL toggleOnAIM = self.toggleOnAIM;
  __weak id<ComposeboxAnimationContext> context = _context;

  UIView* toView = [transitionContext viewForKey:UITransitionContextToViewKey];

  [transitionContext.containerView addSubview:toView];

  toView.alpha = 0;
  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      delay:0
      options:UIViewAnimationCurveLinear
      animations:^{
        toView.alpha = 1;
        if (toggleOnAIM) {
          [context setComposeboxMode:ComposeboxMode::kAIM];
        }
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:finished];
      }];
}

@end
