// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/presenters/non_modal_view_controller_presenter.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat kAnimationInDuration = 0.15;
constexpr CGFloat kAnimationInScale = 1.3;
constexpr CGFloat kAnimationOutDuration = 0.1;
}  // namespace

@interface NonModalViewControllerPresenter ()

// The container for the presentation.
@property(nonatomic, strong, readwrite) UIView* containerView;

// The presenter animator, so if dismiss happens while presenting, it is smooth.
@property(nonatomic, strong, readwrite) UIViewPropertyAnimator* animator;

@end

@implementation NonModalViewControllerPresenter

@synthesize baseViewController, presentedViewController, delegate;

- (void)prepareForPresentation {
  DCHECK(self.baseViewController);
  DCHECK(self.presentedViewController);

  // Add child view controller.
  [self.baseViewController addChildViewController:self.presentedViewController];

  // Prepare the container view.
  self.containerView =
      [[UIView alloc] initWithFrame:self.baseViewController.view.bounds];
  self.containerView.alpha = 0.0;
  self.containerView.transform = CGAffineTransformScale(
      CGAffineTransformIdentity, kAnimationInScale, kAnimationInScale);

  // Add the presented view in the container.
  self.presentedViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [self.containerView addSubview:self.presentedViewController.view];
  AddSameConstraints(self.presentedViewController.view, self.containerView);

  // Add the container to the presenting view controller.
  self.containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.baseViewController.view addSubview:self.containerView];
  AddSameConstraints(self.containerView, self.baseViewController.view);
}

- (void)presentAnimated:(BOOL)animated {
  DCHECK(!self.animator) << "Presenting again is not supported.";
  __weak __typeof(self) weakSelf = self;
  auto animation = ^{
    weakSelf.containerView.alpha = 1.0;
    weakSelf.containerView.transform = CGAffineTransformIdentity;
  };
  auto completion = ^void(UIViewAnimatingPosition) {
    [weakSelf.presentedViewController
        didMoveToParentViewController:weakSelf.baseViewController];
    [weakSelf.delegate containedPresenterDidPresent:weakSelf];
  };

  if (animated) {
    self.animator = [[UIViewPropertyAnimator alloc]
        initWithDuration:animated ? kAnimationInDuration : 0
                   curve:UIViewAnimationCurveEaseOut
              animations:animation];
    [self.animator addCompletion:completion];
    [self.animator startAnimation];
  } else {
    animation();
    completion(UIViewAnimatingPositionEnd);
  }
}

- (void)dismissAnimated:(BOOL)animated {
  if (self.animator.state == UIViewAnimatingStateActive)
    [self.animator stopAnimation:YES];

  [self.presentedViewController willMoveToParentViewController:nil];

  __weak __typeof(self) weakSelf = self;
  auto animation = ^{
    weakSelf.containerView.alpha = 0.0;
  };
  auto completion = ^void(UIViewAnimatingPosition) {
    [weakSelf.presentedViewController.view removeFromSuperview];
    [weakSelf.presentedViewController removeFromParentViewController];
    [weakSelf.containerView removeFromSuperview];
    [weakSelf.delegate containedPresenterDidDismiss:weakSelf];
  };

  if (animated) {
    UIViewPropertyAnimator* dismissAnimator = [[UIViewPropertyAnimator alloc]
        initWithDuration:animated ? kAnimationOutDuration : 0
                   curve:UIViewAnimationCurveEaseOut
              animations:animation];
    [dismissAnimator addCompletion:completion];
    [dismissAnimator startAnimation];
  } else {
    animation();
    completion(UIViewAnimatingPositionEnd);
  }
}

@end
