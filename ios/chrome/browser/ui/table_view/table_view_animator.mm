// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_animator.h"

#import "ios/chrome/browser/ui/util/rtl_geometry.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// If |direction| is TableAnimatorDirectionFromLeading returns
// LayoutRectGetRectUsingDirection using the inverted |direction| for
// |layoutRect|. If |direction| is TableAnimatorDirectionFromTrailing returns
// LayoutRectGetRect for |layoutRect|.
CGRect LayoutRectGetRectForDirection(LayoutRect layoutRect,
                                     TableAnimatorDirection direction) {
  if (direction == TableAnimatorDirectionFromLeading) {
    auto invertedDirection = base::i18n::IsRTL() ? base::i18n::LEFT_TO_RIGHT
                                                 : base::i18n::RIGHT_TO_LEFT;
    return LayoutRectGetRectUsingDirection(layoutRect, invertedDirection);
  }
  return LayoutRectGetRect(layoutRect);
}

}  // namespace

@implementation TableViewAnimator
@synthesize presenting = _presenting;

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return 0.25;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // Set up the keys for the "base" view/VC and the "presented" view/VC. These
  // will be used to fetch the associated objects later.
  NSString* baseViewKey = self.presenting ? UITransitionContextFromViewKey
                                          : UITransitionContextToViewKey;
  NSString* presentedViewControllerKey =
      self.presenting ? UITransitionContextToViewControllerKey
                      : UITransitionContextFromViewControllerKey;
  NSString* presentedViewKey = self.presenting ? UITransitionContextToViewKey
                                               : UITransitionContextFromViewKey;

  // Get views and view controllers for this transition.
  UIView* baseView = [transitionContext viewForKey:baseViewKey];
  UIViewController* presentedViewController =
      [transitionContext viewControllerForKey:presentedViewControllerKey];
  UIView* presentedView = [transitionContext viewForKey:presentedViewKey];

  // Always add the destination view to the container.
  UIView* containerView = [transitionContext containerView];
  if (self.presenting) {
    [containerView addSubview:presentedView];
  } else {
    [containerView addSubview:baseView];
  }

  // Set the initial frame and Compute the final frame for the presented view.
  LayoutRect presentedViewFinalFrame = LayoutRectZero;

  if (self.presenting) {
    presentedViewFinalFrame = LayoutRectForRectInBoundingRect(
        [transitionContext finalFrameForViewController:presentedViewController],
        containerView.bounds);

    LayoutRect presentedViewStartFrame = presentedViewFinalFrame;
    presentedViewStartFrame.position.leading =
        CGRectGetWidth(containerView.bounds);
    presentedView.frame =
        LayoutRectGetRectForDirection(presentedViewStartFrame, self.direction);
  } else {
    presentedViewFinalFrame = LayoutRectForRectInBoundingRect(
        presentedView.frame, containerView.bounds);
    presentedViewFinalFrame.position.leading =
        CGRectGetWidth(containerView.bounds);
  }

  // Animate using the animator's own duration value.
  [UIView animateWithDuration:[self transitionDuration:transitionContext]
      delay:0
      usingSpringWithDamping:0.85
      initialSpringVelocity:0
      options:UIViewAnimationOptionTransitionNone
      animations:^{
        presentedView.frame = LayoutRectGetRectForDirection(
            presentedViewFinalFrame, self.direction);
      }
      completion:^(BOOL finished) {
        BOOL success = ![transitionContext transitionWasCancelled];

        // If presentation failed, remove the view.
        if (self.presenting && !success) {
          [presentedView removeFromSuperview];
        }

        // Notify UIKit that the transition has finished
        [transitionContext completeTransition:success];
      }];
}

@end
