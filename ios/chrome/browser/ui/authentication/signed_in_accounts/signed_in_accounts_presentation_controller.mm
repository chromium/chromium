// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signed_in_accounts/signed_in_accounts_presentation_controller.h"

#import "ios/chrome/browser/shared/ui/util/accessibility_close_menu_button.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kShadowMargin = 196;

}  // namespace

@implementation SignedInAccountsPresentationController {
  UIView* _shadowContainer;
}

#pragma mark - UIPresentationController

- (CGRect)frameOfPresentedViewInContainerView {
  CGRect safeAreaFrame = UIEdgeInsetsInsetRect(
      self.containerView.bounds, self.containerView.safeAreaInsets);
  CGFloat availableWidth = CGRectGetWidth(safeAreaFrame);
  CGFloat availableHeight = CGRectGetHeight(safeAreaFrame);

  CGSize size = self.presentedViewController.preferredContentSize;
  CGRect presentedViewFrame = safeAreaFrame;
  presentedViewFrame.origin.x += (availableWidth - size.width) / 2.;
  presentedViewFrame.origin.y += (availableHeight - size.height) / 2.;
  presentedViewFrame.size.width = size.width;
  presentedViewFrame.size.height = size.height;
  return presentedViewFrame;
}

- (UIView*)presentedView {
  return _shadowContainer;
}

- (void)presentationTransitionWillBegin {
  // Set the identity chooser view as modal, so controls beneath it are
  // non-selectable.
  self.containerView.accessibilityViewIsModal = YES;
  // Adds close button in the background.
  UIButton* closeButton =
      [AccessibilityCloseMenuButton buttonWithType:UIButtonTypeCustom];
  [closeButton addTarget:self
                  action:@selector(closeButtonAction:)
        forControlEvents:UIControlEventTouchUpInside];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.containerView addSubview:closeButton];
  AddSameConstraints(self.containerView, closeButton);
  // Adds the shadow container.
  _shadowContainer = [[UIView alloc] init];
  [self.containerView addSubview:_shadowContainer];
  _shadowContainer.frame = [self frameOfPresentedViewInContainerView];
  // Adds the shadow image.
  UIImageView* shadowView =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];
  shadowView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.presentedViewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [_shadowContainer addSubview:shadowView];
  shadowView.frame =
      CGRectInset(_shadowContainer.bounds, -kShadowMargin, -kShadowMargin);
  // Adds the view controllver view.
  [_shadowContainer addSubview:self.presentedViewController.view];
  self.presentedViewController.view.frame = _shadowContainer.bounds;
}

- (void)containerViewWillLayoutSubviews {
  _shadowContainer.frame = [self frameOfPresentedViewInContainerView];
}

#pragma mark - Private

- (void)closeButtonAction:(id)sender {
  [self.presentedViewController dismissViewControllerAnimated:YES
                                                   completion:nil];
}

@end
