// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_animator.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_animatable.h"

namespace {
// Animation constants.
constexpr NSTimeInterval kAnimationDuration = 0.5;
constexpr CGFloat kSpringDamping = 0.8;
constexpr CGFloat kTranslationMargin = 20.0;
}  // namespace

@implementation AssistantContainerAnimator

- (void)animatePresentation:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                 completion:(void (^)(void))completion {
  [self animate:viewController presentation:YES completion:completion];
}

- (void)animateDismissal:
            (UIViewController<AssistantContainerAnimatable>*)viewController
              completion:(void (^)(void))completion {
  [self animate:viewController presentation:NO completion:completion];
}

#pragma mark - Private

// Animates the container view. If `presentation` is YES, the container slides
// up from the bottom. Otherwise, it slides down.
- (void)animate:(UIViewController<AssistantContainerAnimatable>*)viewController
    presentation:(BOOL)presentation
      completion:(void (^)(void))completion {
  UIView* view = viewController.view;
  UIView* containerView = viewController.assistantContainerView;
  UIView* dimmingView = viewController.dimmingView;

  // Prepare for animation.
  viewController.isAnimating = YES;

  // Layout to ensure frame is valid.
  [view.superview layoutIfNeeded];

  CGFloat containerHeight = view.frame.size.height;
  CGAffineTransform hiddenTransform =
      CGAffineTransformMakeTranslation(0, containerHeight + kTranslationMargin);

  CGAffineTransform targetTransform;
  UIViewAnimationOptions options;
  CGFloat targetAlpha = 0.0;

  if (presentation) {
    targetAlpha = dimmingView.alpha;
    dimmingView.alpha = 0.0;
    containerView.transform = hiddenTransform;
    targetTransform = CGAffineTransformIdentity;
    options = UIViewAnimationOptionCurveEaseOut;
  } else {
    targetTransform = hiddenTransform;
    options = UIViewAnimationOptionCurveEaseIn;
  }

  // Animate.
  [UIView animateWithDuration:kAnimationDuration
      delay:0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:0
      options:options
      animations:^{
        containerView.transform = targetTransform;
        dimmingView.alpha = targetAlpha;
      }
      completion:^(BOOL finished) {
        viewController.isAnimating = NO;
        if (completion) {
          completion();
        }
      }];
}

@end
