// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_presentation_controller.h"

#import "ios/chrome/browser/shared/ui/util/accessibility_close_menu_button.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kMaxWidth = 350;
const CGFloat kMaxHeight = 350;
const CGFloat kMinimumMarginHorizontal = 25;
const CGFloat kMinimumMarginVertical = 35;

const CGFloat kShadowMargin = 196;
const CGFloat kContainerCornerRadius = 13.0;
}  // namespace

@interface IdentityChooserPresentationController ()

@property(nonatomic, strong) UIButton* closeButton;
@property(nonatomic, strong) UIView* shadowContainer;

@end

@implementation IdentityChooserPresentationController

#pragma mark - UIPresentationController

- (CGRect)frameOfPresentedViewInContainerView {
  CGRect safeAreaFrame = UIEdgeInsetsInsetRect(
      self.containerView.bounds, self.containerView.safeAreaInsets);

  CGFloat availableWidth = CGRectGetWidth(safeAreaFrame);
  CGFloat availableHeight = CGRectGetHeight(safeAreaFrame);

  CGFloat width = MIN(kMaxWidth, availableWidth - 2 * kMinimumMarginHorizontal);
  CGFloat height =
      MIN(kMaxHeight, availableHeight - 2 * kMinimumMarginVertical);

  CGRect presentedViewFrame = safeAreaFrame;
  presentedViewFrame.origin.x += (availableWidth - width) / 2;
  presentedViewFrame.origin.y += (availableHeight - height) / 2;
  presentedViewFrame.size.width = width;
  presentedViewFrame.size.height = height;

  return presentedViewFrame;
}

- (UIView*)presentedView {
  return self.shadowContainer;
}

- (void)presentationTransitionWillBegin {
  // Set the identity chooser view as modal, so controls beneath it are
  // non-selectable.
  self.containerView.accessibilityViewIsModal = YES;

  self.closeButton =
      [AccessibilityCloseMenuButton buttonWithType:UIButtonTypeCustom];
  [self.closeButton addTarget:self
                       action:@selector(closeButtonAction)
             forControlEvents:UIControlEventTouchUpInside];
  self.closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.containerView addSubview:self.closeButton];
  AddSameConstraints(self.containerView, self.closeButton);

  self.shadowContainer = [[UIView alloc] init];

  UIView* contentClippingView = [[UIView alloc] init];
  contentClippingView.layer.cornerRadius = kContainerCornerRadius;
  contentClippingView.layer.masksToBounds = YES;
  contentClippingView.clipsToBounds = YES;

  UIImageView* shadowView =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];

  shadowView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.presentedViewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  contentClippingView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  [contentClippingView addSubview:self.presentedViewController.view];
  [self.shadowContainer addSubview:shadowView];
  [self.shadowContainer addSubview:contentClippingView];

  [self.containerView addSubview:self.shadowContainer];

  self.shadowContainer.frame = [self frameOfPresentedViewInContainerView];
  contentClippingView.frame = self.shadowContainer.bounds;
  self.presentedViewController.view.frame = self.shadowContainer.bounds;
  shadowView.frame =
      CGRectInset(self.shadowContainer.bounds, -kShadowMargin, -kShadowMargin);
}

- (void)containerViewWillLayoutSubviews {
  self.shadowContainer.frame = [self frameOfPresentedViewInContainerView];
}

#pragma mark - Private

- (void)closeButtonAction {
  [self.presentedViewController dismissViewControllerAnimated:YES
                                                   completion:nil];
}

@end
