// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_presentation_controller.h"

@interface GuidedTourBubbleViewControllerPresentationController ()

// The background dimmed view behind the BubbleView.
@property(nonatomic, strong) UIView* dimmingView;

@end

@implementation GuidedTourBubbleViewControllerPresentationController {
  CGRect _presentedBubbleViewFrame;
  CGRect _anchorViewFrame;
  CGFloat _cornerRadius;
}

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
           presentedBubbleViewFrame:(CGRect)presentedBubbleViewFrame
                    anchorViewFrame:(CGRect)anchorViewFrame
                       cornerRadius:(CGFloat)cornerRadius {
  self = [super initWithPresentedViewController:presentedViewController
                       presentingViewController:presentingViewController];
  if (self) {
    _presentedBubbleViewFrame = presentedBubbleViewFrame;
    _anchorViewFrame = anchorViewFrame;
    _cornerRadius = cornerRadius;
  }
  return self;
}

#pragma mark - Lifecycle

- (CGRect)frameOfPresentedViewInContainerView {
  UIView* containerView = self.containerView;
  if (!containerView) {
    return CGRectZero;
  }

  return _presentedBubbleViewFrame;
}

- (void)presentationTransitionWillBegin {
  UIView* containerView = self.containerView;
  UIViewController* presentedVC = self.presentedViewController;
  if (!containerView) {
    return;
  }

  _dimmingView = [[UIView alloc] initWithFrame:CGRectZero];
  _dimmingView.backgroundColor =
      [[UIColor blackColor] colorWithAlphaComponent:0.5];
  _dimmingView.alpha = 0.0;
  self.dimmingView.frame = containerView.bounds;
  [containerView insertSubview:self.dimmingView atIndex:0];
  [self addSpotlightViewCutOutWithCornerRadius:_cornerRadius];

  id<UIViewControllerTransitionCoordinator> coordinator =
      presentedVC.transitionCoordinator;
  if (coordinator) {
    __weak GuidedTourBubbleViewControllerPresentationController* weakSelf =
        self;
    [coordinator
        animateAlongsideTransition:^(
            id<UIViewControllerTransitionCoordinatorContext> context) {
          weakSelf.dimmingView.alpha = 1.0;
        }
                        completion:nil];
  } else {
    self.dimmingView.alpha = 1.0;
  }
}

- (void)dismissalTransitionWillBegin {
  UIViewController* presentedVC = self.presentedViewController;
  id<UIViewControllerTransitionCoordinator> coordinator =
      presentedVC.transitionCoordinator;

  if (coordinator) {
    __weak GuidedTourBubbleViewControllerPresentationController* weakSelf =
        self;
    [coordinator
        animateAlongsideTransition:^(
            id<UIViewControllerTransitionCoordinatorContext> context) {
          weakSelf.dimmingView.alpha = 0.0;
        }
                        completion:nil];
  } else {
    self.dimmingView.alpha = 0.0;
  }
}

- (void)dismissalTransitionDidEnd:(BOOL)completed {
  if (completed) {
    [self.dimmingView removeFromSuperview];
    self.dimmingView = nil;
  }
}

- (void)containerViewWillLayoutSubviews {
  [super containerViewWillLayoutSubviews];

  if (self.containerView) {
    self.dimmingView.frame = self.containerView.bounds;
  }
  self.presentedView.frame = [self frameOfPresentedViewInContainerView];
}

#pragma mark - Private

// Carves a hole in the background view to spotlight the view anchoring the
// bubble view.
- (void)addSpotlightViewCutOutWithCornerRadius:(CGFloat)radius {
  CAShapeLayer* maskLayer = [CAShapeLayer layer];
  CGRect backgroundBounds = self.dimmingView.bounds;
  maskLayer.frame = backgroundBounds;

  // Create the path for the mask
  // Start with a rectangle covering the whole mask area
  UIBezierPath* maskPath = [UIBezierPath bezierPathWithRect:backgroundBounds];
  // Create a path for the spotlight hole.
  UIBezierPath* spotlightPath =
      [UIBezierPath bezierPathWithRoundedRect:_anchorViewFrame
                                 cornerRadius:radius];
  [maskPath appendPath:spotlightPath];

  maskLayer.path = maskPath.CGPath;
  maskLayer.fillRule = kCAFillRuleEvenOdd;
  self.dimmingView.layer.mask = maskLayer;
}

@end
