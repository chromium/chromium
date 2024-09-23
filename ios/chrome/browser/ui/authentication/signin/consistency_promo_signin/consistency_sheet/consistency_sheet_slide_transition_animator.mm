// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_slide_transition_animator.h"

#import "base/i18n/rtl.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_sheet/consistency_sheet_navigation_controller.h"

namespace {

// Slide animation duration in seconds.
const CGFloat kAnimationDuration = 0.25;

}  // namespace

@interface ConsistencySheetSlideTransitionAnimator ()

@property(nonatomic, assign, readonly) ConsistencySheetSlideAnimation animation;

@end

@implementation ConsistencySheetSlideTransitionAnimator

- (instancetype)initWithAnimation:(ConsistencySheetSlideAnimation)animation
             navigationController:
                 (ConsistencySheetNavigationController*)navigationController {
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

  // Save the pre-layout frame for the navigation and destination views. These
  // will be replaced with the post-layout frames if the underlying child view
  // controller requires a layout change in `layoutFittingSizeForWidth:`.
  CGRect viewControllerFrame = self.navigationController.view.frame;
  CGRect fromViewFrame = fromView.frame;

  CGSize toViewSize = [self.navigationController
      layoutFittingSizeForWidth:viewControllerFrame.size.width];

  // Restore frame layouts used prior to the layout change.
  fromView.frame = fromViewFrame;

  CGFloat sizeDifference = toViewSize.height - fromView.frame.size.height;
  CGRect fromViewFrameAfterAnimation = fromView.frame;
  CGRect toViewFrameBeforeAnimation = fromView.frame;
  CGRect toViewFrameAfterAnimation =
      CGRectMake(0, 0, toViewSize.width, toViewSize.height);
  CGRect navigationFrameAfterAnimation = viewControllerFrame;

  BOOL fromViewShouldMoveTowardsTheRight =
      (self.animation == ConsistencySheetSlideAnimationPopping &&
       !base::i18n::IsRTL()) ||
      (self.animation == ConsistencySheetSlideAnimationPushing &&
       base::i18n::IsRTL());
  if (fromViewShouldMoveTowardsTheRight) {
    toViewFrameBeforeAnimation.origin.x = -toView.frame.size.width;
    fromViewFrameAfterAnimation.origin.x = fromView.frame.size.width;
  } else {
    toViewFrameBeforeAnimation.origin.x = toView.frame.size.width;
    fromViewFrameAfterAnimation.origin.x = -fromView.frame.size.width;
  }
  toView.frame = toViewFrameBeforeAnimation;
  switch (self.navigationController.displayStyle) {
    case ConsistencySheetDisplayStyleBottom:
      navigationFrameAfterAnimation.origin.y -= sizeDifference;
      navigationFrameAfterAnimation.size.height += sizeDifference;
      break;
    case ConsistencySheetDisplayStyleCentered:
      navigationFrameAfterAnimation.origin.y -= sizeDifference / 2.;
      navigationFrameAfterAnimation.size.height += sizeDifference;
      break;
  }

  // Restore frame to pre-layout value before triggering animations.
  self.navigationController.view.frame = viewControllerFrame;
  [self.navigationController didUpdateControllerViewFrame];
  NSTimeInterval duration = [self transitionDuration:transitionContext];
  void (^animations)() = ^() {
    [UIView addKeyframeWithRelativeStartTime:.0
                            relativeDuration:1.
                                  animations:^{
                                    self.navigationController.view.frame =
                                        navigationFrameAfterAnimation;
                                    [self.navigationController
                                            didUpdateControllerViewFrame];
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
