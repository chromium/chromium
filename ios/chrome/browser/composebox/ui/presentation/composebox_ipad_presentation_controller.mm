// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/presentation/composebox_ipad_presentation_controller.h"

#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

namespace {

// The corner radius for the composebox.
const CGFloat kComposeboxCornerRadius = 16.0f;

}  // namespace

@implementation ComposeboxiPadPresentationController {
  // The dimming view, used to dismiss the composebox when tapped.
  UIView* _dimmingView;
}

- (instancetype)initWithPresentedViewController:(UIViewController*)presented
                       presentingViewController:(UIViewController*)presenting {
  self = [super initWithPresentedViewController:presented
                       presentingViewController:presenting];
  if (self) {
    _dimmingView = [[UIView alloc] init];
    _dimmingView.backgroundColor =
        [[UIColor blackColor] colorWithAlphaComponent:0.5];

    UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(dimmingViewTapped:)];
    [_dimmingView addGestureRecognizer:tap];
  }
  return self;
}
#pragma mark - UIPresentationController

- (void)presentationTransitionWillBegin {
  UIView* dimmingView = _dimmingView;
  dimmingView.frame = self.containerView.bounds;
  [self.containerView insertSubview:dimmingView atIndex:0];

  id<UIViewControllerTransitionCoordinator> coordinator =
      self.presentedViewController.transitionCoordinator;
  if (coordinator) {
    [coordinator
        animateAlongsideTransition:^(
            id<UIViewControllerTransitionCoordinatorContext> context) {
          dimmingView.alpha = 1.0;
        }
                        completion:nil];
  } else {
    dimmingView.alpha = 1.0;
  }
}

- (void)dismissalTransitionWillBegin {
  UIView* dimmingView = _dimmingView;
  id<UIViewControllerTransitionCoordinator> coordinator =
      self.presentedViewController.transitionCoordinator;
  if (coordinator) {
    [coordinator
        animateAlongsideTransition:^(
            id<UIViewControllerTransitionCoordinatorContext> context) {
          dimmingView.alpha = 0.0;
        }
        completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
          [dimmingView removeFromSuperview];
        }];
  } else {
    dimmingView.alpha = 0.0;
    [dimmingView removeFromSuperview];
  }
}
- (CGRect)frameOfPresentedViewInContainerView {
  UIView* containerView = self.containerView;
  if (!containerView) {
    return CGRectZero;
  }

  LayoutGuideCenter* layoutGuideCenter = self.layoutGuideCenter;
  UIView* topOmnibox =
      [layoutGuideCenter referencedViewUnderName:kTopOmniboxGuide];
  CGRect omniboxFrame = [topOmnibox convertRect:topOmnibox.bounds toView:nil];

  // TODO(crbug.com/469368394): Use real values.
  CGFloat top = CGRectGetMinY(omniboxFrame) - 4.0;
  CGFloat width = containerView.bounds.size.width * 0.75;
  CGFloat x = (containerView.bounds.size.width - width) / 2.0;
  CGFloat height = (containerView.bounds.size.height - top) * 0.75;

  return CGRectMake(x, top, width, height);
}

- (void)containerViewWillLayoutSubviews {
  [super containerViewWillLayoutSubviews];
  _dimmingView.frame = self.containerView.bounds;
  self.presentedView.frame = [self frameOfPresentedViewInContainerView];
  self.presentedView.layer.cornerRadius = kComposeboxCornerRadius;
}

#pragma mark - Private

// Called when the scrim is tapped.
- (void)dimmingViewTapped:(UITapGestureRecognizer*)sender {
  [self.browserCoordinatorHandler hideComposeboxImmediately:NO];
}

@end
