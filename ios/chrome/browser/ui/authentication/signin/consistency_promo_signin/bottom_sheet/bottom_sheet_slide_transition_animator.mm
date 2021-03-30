// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/bottom_sheet_slide_transition_animator.h"

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/bottom_sheet_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Slide animation duration in seconds.
const CGFloat kAnimationDuration = 0.25;

}  // namespace

@interface BottomSheetSlideTransitionAnimator ()

@property(nonatomic, assign, readonly) BottomSheetSlideAnimation animation;

@end

@implementation BottomSheetSlideTransitionAnimator

- (instancetype)initWithAnimation:(BottomSheetSlideAnimation)animation
             navigationController:
                 (BottomSheetNavigationController*)navigationController {
  self = [super init];
  if (self) {
    _animation = animation;
    _navigationController = navigationController;
  }
  return self;
}

#pragma mark - UIViewControllerAnimatedTransitioning

- (NSTimeInterval)transitionDuration:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  return kAnimationDuration;
}

- (void)animateTransition:
    (id<UIViewControllerContextTransitioning>)transitionContext {
  // The view to slide out.
  UIView* fromView =
      [transitionContext viewForKey:UITransitionContextFromViewKey];
  // The view to slide in.
  UIView* toView = [transitionContext viewForKey:UITransitionContextToViewKey];
  if (!fromView || !toView) {
    return;
  }
  [transitionContext.containerView addSubview:toView];

  CGSize toViewSize = [self.navigationController layoutFittingSize];
  CGFloat sizeDifference = toViewSize.height - fromView.frame.size.height;
  CGRect fromViewFrameAfterAnimation = fromView.frame;
  CGRect toViewFrameBeforeAnimation = fromView.frame;
  CGRect toViewFrameAfterAnimation =
      CGRectMake(0, 0, toViewSize.width, toViewSize.height);
  CGRect navigationFrameAfterAnimation = self.navigationController.view.frame;

  switch (self.animation) {
    case BottomSheetSlideAnimationPopping:
      toViewFrameBeforeAnimation.origin.x = -toView.frame.size.width;
      fromViewFrameAfterAnimation.origin.x = fromView.frame.size.width;
      break;
    case BottomSheetSlideAnimationPushing:
      toViewFrameBeforeAnimation.origin.x = toView.frame.size.width;
      fromViewFrameAfterAnimation.origin.x = -fromView.frame.size.width;
      break;
  }
  toView.frame = toViewFrameBeforeAnimation;
  navigationFrameAfterAnimation.origin.y -= sizeDifference;
  navigationFrameAfterAnimation.size.height += sizeDifference;

  NSTimeInterval duration = [self transitionDuration:transitionContext];
  void (^animations)() = ^() {
    [UIView addKeyframeWithRelativeStartTime:.0
                            relativeDuration:1.
                                  animations:^{
                                    self.navigationController.view.frame =
                                        navigationFrameAfterAnimation;
                                    toView.frame = toViewFrameAfterAnimation;
                                    fromView.frame =
                                        fromViewFrameAfterAnimation;
                                  }];
  };
  void (^completion)(BOOL finished) = ^(BOOL finished) {
    [transitionContext
        completeTransition:!transitionContext.transitionWasCancelled];
  };
  [UIView
      animateKeyframesWithDuration:duration
                             delay:0
                           options:
                              UIViewKeyframeAnimationOptionCalculationModeLinear
                        animations:animations
                        completion:completion];
}

@end
