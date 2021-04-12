// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/bottom_sheet_presentation_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/bottom_sheet/bottom_sheet_navigation_controller.h"
#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/util/accessibility_close_menu_button.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BottomSheetPresentationController ()

// View controller to present.
@property(nonatomic, strong)
    BottomSheetNavigationController* navigationController;
// View to dim the background.
@property(nonatomic, strong) UIView* backgroundDimmerView;

@end

@implementation BottomSheetPresentationController

- (instancetype)initWithBottomSheetNavigationController:
                    (BottomSheetNavigationController*)navigationController
                               presentingViewController:
                                   (UIViewController*)presentingViewController {
  self = [super initWithPresentedViewController:navigationController
                       presentingViewController:presentingViewController];
  if (self) {
    _navigationController = navigationController;
  }
  return self;
}

#pragma mark - UIPresentationController

- (CGRect)frameOfPresentedViewInContainerView {
  CGRect presentedViewFrame = self.containerView.frame;
  CGSize size = [self.navigationController layoutFittingSize];
  presentedViewFrame.origin.y = presentedViewFrame.size.height - size.height;
  presentedViewFrame.size = size;
  return presentedViewFrame;
}

- (void)presentationTransitionWillBegin {
  DCHECK(self.navigationController == self.presentedViewController);
  DCHECK(!self.backgroundDimmerView);

  // Accessibility.
  self.containerView.accessibilityViewIsModal = YES;

  // Add dim effect.
  self.backgroundDimmerView = [[UIView alloc] init];
  self.backgroundDimmerView.backgroundColor = [UIColor clearColor];
  self.backgroundDimmerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.containerView addSubview:self.backgroundDimmerView];
  AddSameConstraints(self.containerView, self.backgroundDimmerView);
  __weak __typeof(self) weakSelf = self;
  [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        weakSelf.backgroundDimmerView.backgroundColor =
            [UIColor colorWithWhite:0 alpha:0.4];
      }
                      completion:nil];

  // Add close button view.
  UIButton* closeButton =
      [AccessibilityCloseMenuButton buttonWithType:UIButtonTypeCustom];
  [closeButton addTarget:self
                  action:@selector(closeButtonAction:)
        forControlEvents:UIControlEventTouchUpInside];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.containerView addSubview:closeButton];
  AddSameConstraints(self.containerView, closeButton);

  // Add presented view controller.
  [self.containerView addSubview:self.presentedViewController.view];
  self.presentedViewController.view.frame =
      [self frameOfPresentedViewInContainerView];
}

- (void)dismissalTransitionWillBegin {
  DCHECK(self.backgroundDimmerView);

  // Remove dim effect.
  __weak __typeof(self) weakSelf = self;
  [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        weakSelf.backgroundDimmerView.backgroundColor = UIColor.clearColor;
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        weakSelf.backgroundDimmerView = nil;
      }];
}

- (void)containerViewDidLayoutSubviews {
  [super containerViewDidLayoutSubviews];
  self.presentedViewController.view.frame =
      [self frameOfPresentedViewInContainerView];
}

#pragma mark - Private

// Closes the bottom sheet.
- (void)closeButtonAction:(id)sender {
  [self.presentationDelegate
      bottomSheetPresentationControllerDismissViewController:self];
}

@end
