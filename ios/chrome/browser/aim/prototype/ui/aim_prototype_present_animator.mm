// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_present_animator.h"

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_animation_context_provider.h"

namespace {
// The total duration of the presentation animation.
const NSTimeInterval kTotalDuration = 0.5;
// The duration of the input plate slide-in animation, relative to the total
// duration.
const NSTimeInterval kSlideInDuration = 0.1;
}  // namespace

@implementation AIMPrototypePresentAnimator {
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
  return kTotalDuration;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  UIView* toView = [transitionContext viewForKey:UITransitionContextToViewKey];
  UIView* containerView = transitionContext.containerView;
  [containerView addSubview:toView];

  UIView* mainView = [_contextProvider mainViewForAnimation];
  UIView* inputPlateView = [_contextProvider inputPlateViewForAnimation];
  UITextView* textView = [_contextProvider textViewForAnimation];

  mainView.alpha = 0.0;
  CGRect finalFrame = inputPlateView.frame;
  inputPlateView.frame =
      CGRectMake(finalFrame.origin.x, containerView.bounds.size.height,
                 finalFrame.size.width, finalFrame.size.height);

  BOOL toggleOnAIM = self.toggleOnAIM;
  __weak id<AIMPrototypeAnimationContextProvider> contextProvider =
      _contextProvider;

  [textView becomeFirstResponder];
  [UIView
      animateKeyframesWithDuration:[self transitionDuration:transitionContext]
      delay:0
      options:UIViewKeyframeAnimationOptionCalculationModeLinear
      animations:^{
        // Fade in the main view over the full duration.
        [UIView addKeyframeWithRelativeStartTime:0.0
                                relativeDuration:1.0
                                      animations:^{
                                        mainView.alpha = 1.0;
                                      }];
        // Slide in the input plate.
        [UIView addKeyframeWithRelativeStartTime:0.0
                                relativeDuration:kSlideInDuration
                                      animations:^{
                                        inputPlateView.frame = finalFrame;
                                      }];

        // Enables AIM.
        [UIView
            addKeyframeWithRelativeStartTime:0.2
                            relativeDuration:0.8
                                  animations:^{
                                    if (toggleOnAIM) {
                                      [contextProvider setAIModeEnabled:YES];
                                    }
                                  }];
      }
      completion:^(BOOL finished) {
        [transitionContext completeTransition:finished];
      }];
}

@end
