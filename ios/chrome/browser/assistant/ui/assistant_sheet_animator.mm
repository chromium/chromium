// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_sheet_animator.h"

namespace {
// Animation constants.
constexpr NSTimeInterval kAnimationDuration = 0.5;
constexpr CGFloat kSpringDamping = 0.8;
}  // namespace

@implementation AssistantSheetAnimator

- (void)animatePresentation:(UIView*)view
                 completion:(void (^)(void))completion {
  // TODO(crbug.com/469050167): Improve animation.
  view.alpha = 0.0;
  [UIView animateWithDuration:kAnimationDuration
      delay:0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:0
      options:UIViewAnimationOptionCurveEaseOut
      animations:^{
        view.alpha = 1.0;
        [view.superview layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (completion) {
          completion();
        }
      }];
}

- (void)animateDismissal:(UIView*)view completion:(void (^)(void))completion {
  // TODO(crbug.com/469050167): Improve animation.
  [UIView animateWithDuration:kAnimationDuration
      delay:0
      usingSpringWithDamping:kSpringDamping
      initialSpringVelocity:0
      options:UIViewAnimationOptionCurveEaseIn
      animations:^{
        view.alpha = 0.0;
        [view.superview layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        if (completion) {
          completion();
        }
      }];
}

@end
