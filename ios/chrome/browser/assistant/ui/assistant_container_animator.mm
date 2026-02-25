// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_animator.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

namespace {
// Animation constants.
constexpr NSTimeInterval kAnimationDuration = 0.5;
constexpr CGFloat kSpringDamping = 0.8;
constexpr CGFloat kTranslationMargin = 20.0;
}  // namespace

@implementation AssistantContainerAnimator

- (void)animatePresentation:(AssistantContainerViewController*)viewController
                 completion:(void (^)(void))completion {
  [self animate:viewController presentation:YES completion:completion];
}

- (void)animateDismissal:(AssistantContainerViewController*)viewController
              completion:(void (^)(void))completion {
  [self animate:viewController presentation:NO completion:completion];
}

#pragma mark - Private

// Animates the container view. If `presentation` is YES, the container slides
// up from the bottom. Otherwise, it slides down.
- (void)animate:(AssistantContainerViewController*)viewController
    presentation:(BOOL)presentation
      completion:(void (^)(void))completion {
  UIView* view = viewController.view;
  UIView* anchorView = viewController.anchorView;

  // Prepare for animation.
  viewController.isAnimating = YES;

  // Ensure Anchor View is visually on top during animation.
  CGFloat originalZPosition = anchorView.layer.zPosition;
  anchorView.layer.zPosition = view.layer.zPosition + 1;

  // Layout to ensure frame is valid.
  [view.superview layoutIfNeeded];

  CGFloat containerHeight = view.frame.size.height;
  CGAffineTransform hiddenTransform =
      CGAffineTransformMakeTranslation(0, containerHeight + kTranslationMargin);

  CGAffineTransform targetTransform;
  UIViewAnimationOptions options;

  if (presentation) {
    view.transform = hiddenTransform;
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
        view.transform = targetTransform;
      }
      completion:^(BOOL finished) {
        viewController.isAnimating = NO;
        anchorView.layer.zPosition = originalZPosition;
        if (completion) {
          completion();
        }
      }];
}

@end
