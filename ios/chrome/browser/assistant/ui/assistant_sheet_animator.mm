// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_sheet_animator.h"

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"

namespace {
// Animation constants.
constexpr NSTimeInterval kAnimationDuration = 0.5;
constexpr CGFloat kSpringDamping = 0.8;
constexpr CGFloat kTranslationMargin = 20.0;
}  // namespace

@implementation AssistantSheetAnimator

- (void)animatePresentation:(AssistantSheetViewController*)viewController
                 completion:(void (^)(void))completion {
  [self animate:viewController presentation:YES completion:completion];
}

- (void)animateDismissal:(AssistantSheetViewController*)viewController
              completion:(void (^)(void))completion {
  [self animate:viewController presentation:NO completion:completion];
}

#pragma mark - Private

// Animates the sheet view. If `presentation` is YES, the sheet slides up from
// the bottom. Otherwise, it slides down.
- (void)animate:(AssistantSheetViewController*)viewController
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

  CGFloat sheetHeight = view.frame.size.height;
  CGAffineTransform hiddenTransform =
      CGAffineTransformMakeTranslation(0, sheetHeight + kTranslationMargin);

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
