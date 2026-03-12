// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/guided_tour/guided_tour_bubble_view_controller_presentation_controller.h"

@interface GuidedTourBubbleViewControllerPresentationController ()

// The background dimmed view behind the BubbleView.
@property(nonatomic, strong) UIView* dimmingView;

@end

@implementation GuidedTourBubbleViewControllerPresentationController {
  // View to anchor the bubble view to and also cut out of the dimming view.
  UIView* _anchorView;
  // Corner radius of the anchor view for the dimming view cutout.
  CGFloat _cornerRadius;

  // Layer holding dimming mask.
  CAShapeLayer* _maskLayer;
}

- (instancetype)
    initWithPresentedViewController:(UIViewController*)presentedViewController
           presentingViewController:(UIViewController*)presentingViewController
                         anchorView:(UIView*)anchorView
                       cornerRadius:(CGFloat)cornerRadius {
  self = [super initWithPresentedViewController:presentedViewController
                       presentingViewController:presentingViewController];
  if (self) {
    _anchorView = anchorView;
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

  return [self.positioner presentedBubbleViewFrame];
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
  [self addSpotlightViewCutoutLayer];

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
    _maskLayer.frame = self.dimmingView.bounds;
  }
}

- (void)containerViewDidLayoutSubviews {
  [super containerViewDidLayoutSubviews];

  [self animateLayoutChange];
}

// Updates the view states to make sure that they are correct when the animation
// is completed.
- (void)completeSpotlightViewAnimationWithPath:(UIBezierPath*)path {
  _maskLayer.path = path.CGPath;
  self.presentedView.frame = [self frameOfPresentedViewInContainerView];
}

#pragma mark - Private

// Creates and adds the layer that holds the path to spotlight the view
// anchoring the bubble view.
- (void)addSpotlightViewCutoutLayer {
  _maskLayer = [CAShapeLayer layer];
  CGRect backgroundBounds = self.dimmingView.bounds;
  _maskLayer.frame = backgroundBounds;
  _maskLayer.fillRule = kCAFillRuleEvenOdd;
  self.dimmingView.layer.mask = _maskLayer;

  _maskLayer.path = [self spotlightViewCutoutLayerPath].CGPath;
}

// Returns a path displaying the background view with a a hole in it to
// spotlight the view anchoring the bubble view.
- (UIBezierPath*)spotlightViewCutoutLayerPath {
  CGRect backgroundBounds = self.dimmingView.bounds;

  // Expand background rect so that the dimming view stretches to all sides even
  // after rotation. This makes it stretch out of the view's bounds, but that's
  // ok.
  CGFloat maxDimension =
      MAX(backgroundBounds.size.height, backgroundBounds.size.width);
  backgroundBounds.size.height = maxDimension;
  backgroundBounds.size.width = maxDimension;

  // Create the path for the mask
  // Start with a rectangle covering the whole mask area
  UIBezierPath* maskPath = [UIBezierPath bezierPathWithRect:backgroundBounds];
  // Create a path for the spotlight hole.
  CGRect convertedAnchorRect = [_anchorView convertRect:_anchorView.bounds
                                                 toView:self.dimmingView];
  UIBezierPath* spotlightPath =
      [UIBezierPath bezierPathWithRoundedRect:convertedAnchorRect
                                 cornerRadius:_cornerRadius];
  [maskPath appendPath:spotlightPath];

  return maskPath;
}

// If the view layout has changed (e.g. due to device rotation), animate the
// difference.
- (void)animateLayoutChange {
  // Force the presenting view controller to layout so the location of the
  // anchor view is correct.
  [self.presentingViewController.view layoutIfNeeded];

  UIBezierPath* path = [self spotlightViewCutoutLayerPath];

  if (CGPathEqualToPath(_maskLayer.path, path.CGPath)) {
    return;
  }

  // Animate the cutout path and the bubble view to their new positions if
  // needed.
  id<UIViewControllerTransitionCoordinator> coordinator =
      self.presentedViewController.transitionCoordinator;
  NSTimeInterval animationDuration = coordinator
                                         ? coordinator.transitionDuration
                                         : [UIView inheritedAnimationDuration];
  animationDuration = MAX(animationDuration, 0.2);
  __weak __typeof(self) weakSelf = self;

  [CATransaction begin];

  [CATransaction setCompletionBlock:^{
    [weakSelf completeSpotlightViewAnimationWithPath:path];
  }];

  // Animate the cutout path using CA animations.
  CABasicAnimation* pathAnimation =
      [CABasicAnimation animationWithKeyPath:@"path"];
  pathAnimation.duration = animationDuration;
  pathAnimation.timingFunction = [CAMediaTimingFunction
      functionWithName:kCAMediaTimingFunctionEaseInEaseOut];

  id fromPath = (id)_maskLayer.presentationLayer.path;
  if (!fromPath) {
    fromPath = (id)_maskLayer.path;
  }
  pathAnimation.fromValue = fromPath;
  pathAnimation.toValue = (id)path.CGPath;
  [_maskLayer addAnimation:pathAnimation forKey:@"pathAnimation"];

  _maskLayer.path = path.CGPath;

  // Animate the bubble view using UIView animation.
  [UIView animateWithDuration:animationDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseInOut |
                              UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     weakSelf.presentedView.frame =
                         [weakSelf frameOfPresentedViewInContainerView];
                   }
                   completion:nil];

  [CATransaction commit];
}

@end
