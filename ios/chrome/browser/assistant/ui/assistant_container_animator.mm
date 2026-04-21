// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_animator.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_animatable.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_presenter.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_transition_coordinating.h"

namespace {
// Animation constants.
constexpr CGFloat kSpringDamping = 0.8;
constexpr CGFloat kTranslationMargin = 20.0;
constexpr NSTimeInterval kAssistantSidePanelAnimationDuration = 0.5;
constexpr NSTimeInterval kAssistantBottomSheetAnimationDuration = 0.4;
}  // namespace

@interface AssistantContainerAnimator () <LayoutTransitionCoordinating>
@end

@implementation AssistantContainerAnimator {
  // The layout state used for animation.
  __weak LayoutState* _layoutState;
  // Animations to be run alongside the transition.
  NSMutableArray* _animations;
  // Completion blocks to be executed after the transition.
  NSMutableArray* _completions;
}

- (instancetype)initWithLayoutState:(LayoutState*)layoutState {
  self = [super init];
  if (self) {
    _layoutState = layoutState;
    _animations = [[NSMutableArray alloc] init];
    _completions = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)animatePresentation:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                   animated:(BOOL)animated
                 completion:(void (^)(void))completion {
  [self animateBottomSheet:viewController
                 presented:YES
                  animated:animated
                completion:completion];
}

- (void)animateDismissal:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                animated:(BOOL)animated
              completion:(void (^)(void))completion {
  [self animateBottomSheet:viewController
                 presented:NO
                  animated:animated
                completion:completion];
}

- (void)animateSidePanelPresentation:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                  baseViewController:
                      (UIViewController<AssistantContainerPresenter>*)
                          baseViewController
                            animated:(BOOL)animated
                          completion:(void (^)(void))completion {
  [self animateSidePanel:viewController
      baseViewController:baseViewController
               presented:YES
                animated:animated
              completion:completion];
}

- (void)animateSidePanelDismissal:
            (UIViewController<AssistantContainerAnimatable>*)viewController
               baseViewController:
                   (UIViewController<AssistantContainerPresenter>*)
                       baseViewController
                         animated:(BOOL)animated
                       completion:(void (^)(void))completion {
  [self animateSidePanel:viewController
      baseViewController:baseViewController
               presented:NO
                animated:animated
              completion:completion];
}

#pragma mark - LayoutTransitionCoordinating

// Implements UIViewControllerTransitionCoordinator method to run animations
// alongside the transition.
- (void)animateAlongsideTransition:(void (^)(void))animation
                        completion:(void (^)(void))completion {
  if (animation) {
    [_animations addObject:animation];
  }
  if (completion) {
    [_completions addObject:completion];
  }
}

#pragma mark - Private

// Animates the side panel. If `presented` is YES, the side panel slides in
// from the left. Otherwise, it slides out to the left.
- (void)animateSidePanel:
            (UIViewController<AssistantContainerAnimatable>*)viewController
      baseViewController:
          (UIViewController<AssistantContainerPresenter>*)baseViewController
               presented:(BOOL)presented
                animated:(BOOL)animated
              completion:(void (^)(void))completion {
  UIView* assistantView = viewController.view;
  if (!assistantView) {
    if (completion) {
      completion();
    }
    return;
  }

  [baseViewController.view layoutIfNeeded];

  LayoutState* layoutState = _layoutState;
  if (layoutState.containedLayoutActive == presented) {
    if (completion) {
      completion();
    }
    return;
  }

  if (!animated) {
    layoutState.containedLayoutActive = presented;
    if (completion) {
      completion();
    }
    return;
  }

  [layoutState setContainedLayoutActive:presented
              withTransitionCoordinator:self];

  if (completion) {
    [_completions addObject:completion];
  }

  if (_animations.count > 0 || _completions.count > 0) {
    NSArray* animations = [_animations copy];
    NSArray* completions = [_completions copy];
    [_animations removeAllObjects];
    [_completions removeAllObjects];

    [UIView animateWithDuration:kAssistantSidePanelAnimationDuration
        delay:0
        usingSpringWithDamping:kSpringDamping
        initialSpringVelocity:0
        options:0
        animations:^{
          for (void (^anim)(void) in animations) {
            anim();
          }
        }
        completion:^(BOOL finished) {
          for (void (^comp)(void) in completions) {
            comp();
          }
        }];
  }
}

// Animates the bottom sheet container view. If `presented` is YES, the
// container slides up from the bottom. Otherwise, it slides down.
- (void)animateBottomSheet:
            (UIViewController<AssistantContainerAnimatable>*)viewController
                 presented:(BOOL)presented
                  animated:(BOOL)animated
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

  if (presented) {
    targetAlpha = dimmingView.alpha;
    dimmingView.alpha = 0.0;
    containerView.transform = hiddenTransform;
    targetTransform = CGAffineTransformIdentity;
    options = UIViewAnimationOptionCurveEaseOut;
  } else {
    targetTransform = hiddenTransform;
    options = UIViewAnimationOptionCurveEaseIn;
  }

  if (!animated) {
    [UIView performWithoutAnimation:^{
      containerView.transform = targetTransform;
      dimmingView.alpha = targetAlpha;
    }];
    viewController.isAnimating = NO;
    if (completion) {
      completion();
    }
    return;
  }

  [UIView animateWithDuration:kAssistantBottomSheetAnimationDuration
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
