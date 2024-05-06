// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_switching/account_switcher_presentation_controller.h"

#import "ios/chrome/browser/shared/ui/util/accessibility_close_menu_button.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kMaxWidth = 350;
const CGFloat kMaxHeight = 350;
const CGFloat kMinimumMarginHorizontal = 25;
const CGFloat kMinimumMarginVertical = 35;

}  // namespace

@interface AccountSwitcherPresentationController ()

@property(nonatomic, strong) UIButton* closeButton;

@end

@implementation AccountSwitcherPresentationController

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

- (void)presentationTransitionWillBegin {
  self.containerView.accessibilityViewIsModal = YES;

  self.closeButton =
      [AccessibilityCloseMenuButton buttonWithType:UIButtonTypeCustom];
  [self.closeButton addTarget:self
                       action:@selector(closeButtonAction)
             forControlEvents:UIControlEventTouchUpInside];
  self.closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.containerView addSubview:self.closeButton];
  AddSameConstraints(self.containerView, self.closeButton);
}

- (void)closeButtonAction {
  [self.presentedViewController dismissViewControllerAnimated:YES
                                                   completion:nil];
}

@end
