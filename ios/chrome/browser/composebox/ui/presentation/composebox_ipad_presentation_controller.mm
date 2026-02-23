// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/ui/presentation/composebox_ipad_presentation_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/composebox/ui/composebox_ui_constants.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/ui_util.h"

@implementation ComposeboxiPadPresentationController {
  // The dimming view, used to dismiss the composebox when tapped.
  UIView* _dimmingView;
  // The layout guide used to anchor the composebox.
  UILayoutGuide* _layoutGuide;
}

- (instancetype)initWithPresentedViewController:(UIViewController*)presented
                       presentingViewController:(UIViewController*)presenting {
  self = [super initWithPresentedViewController:presented
                       presentingViewController:presenting];
  if (self) {
    _dimmingView = [[UIView alloc] init];
    _dimmingView.accessibilityIdentifier = @"Typing Shield";
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
  CHECK(self.layoutGuideCenter);
  _layoutGuide = [self.layoutGuideCenter makeLayoutGuideNamed:kTopOmniboxGuide];
  [self.containerView addLayoutGuide:_layoutGuide];
  self.containerView.accessibilityViewIsModal = YES;

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

  CGRect omniboxFrame =
      [_layoutGuide.owningView convertRect:_layoutGuide.layoutFrame
                                    toView:containerView];
  CGFloat top = CGRectGetMinY(omniboxFrame) - kInputPlateMargin;
  CGFloat width = omniboxFrame.size.width;
  CGFloat x = omniboxFrame.origin.x;
  if (IsRegularXRegularSizeClass(self.traitCollection)) {
    x -= kComposeboxOmniboxLayoutGuideHorizontalMargin;
    width += kComposeboxOmniboxLayoutGuideHorizontalMargin * 2;
  }

  CGFloat preferredHeight =
      self.presentedViewController.preferredContentSize.height;
  CGFloat maxHeight = (containerView.bounds.size.height - top) * 0.75;
  CGFloat height =
      preferredHeight > 0 ? MIN(preferredHeight, maxHeight) : kOmniboxMinHeight;

  return CGRectMake(x, top, width, height);
}

- (void)containerViewWillLayoutSubviews {
  [super containerViewWillLayoutSubviews];
  _dimmingView.frame = self.containerView.bounds;
  self.presentedView.frame = [self frameOfPresentedViewInContainerView];
  self.presentedView.layer.cornerRadius =
      kInputPlateCornerRadius + kInputPlateMargin;
  self.presentedView.clipsToBounds = YES;
}

#pragma mark - UIContentContainer

- (void)preferredContentSizeDidChangeForChildContentContainer:
    (id<UIContentContainer>)container {
  [super preferredContentSizeDidChangeForChildContentContainer:container];
  if (container == self.presentedViewController) {
    [self.containerView setNeedsLayout];
    [self.containerView layoutIfNeeded];
  }
}

#pragma mark - Private

// Called when the scrim is tapped.
- (void)dimmingViewTapped:(UITapGestureRecognizer*)sender {
  [self.browserCoordinatorHandler hideComposebox];
}

@end
