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

namespace {

// Alpha for the background dimmer view.
constexpr CGFloat kBackgroundDimmerViewAlpha = .4;

}  // namespace

@interface BottomSheetPresentationController ()

// View controller to present.
@property(nonatomic, strong)
    BottomSheetNavigationController* navigationController;
// YES if the presented view is presented.
@property(nonatomic, assign) BOOL presented;

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
  [super presentationTransitionWillBegin];
  DCHECK(self.navigationController == self.presentedViewController);
  DCHECK(!self.navigationController.backgroundDimmerView);
  DCHECK(!self.presented);
  self.presented = YES;

  // Accessibility.
  self.containerView.accessibilityViewIsModal = YES;

  // Add dimmer effect.
  self.navigationController.backgroundDimmerView = [[UIView alloc] init];
  self.navigationController.backgroundDimmerView.backgroundColor =
      [UIColor clearColor];
  [self.containerView
      addSubview:self.navigationController.backgroundDimmerView];

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
  CGRect presentedFrame = [self frameOfPresentedViewInContainerView];
  presentedFrame.origin.y = self.containerView.bounds.size.height;
  self.presentedViewController.view.frame = presentedFrame;

  // Update the dimmer view and background transparency view.
  [self.navigationController didUpdateControllerViewFrame];
  __weak __typeof(self) weakSelf = self;

  // Animate dimmer view color, and the dimmer view and background view
  // positions.
  [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [self.navigationController didUpdateControllerViewFrame];
        weakSelf.navigationController.backgroundDimmerView.backgroundColor =
            [UIColor colorWithWhite:0 alpha:kBackgroundDimmerViewAlpha];
      }
                      completion:nil];
}

- (void)dismissalTransitionWillBegin {
  [super dismissalTransitionWillBegin];
  DCHECK(self.navigationController.backgroundDimmerView);
  DCHECK(self.presented);
  self.presented = NO;
  // Remove dimmer color and update the views.
  __weak __typeof(self) weakSelf = self;
  [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        weakSelf.navigationController.backgroundDimmerView.backgroundColor =
            UIColor.clearColor;
        [self.navigationController didUpdateControllerViewFrame];
      }
                      completion:nil];
}

- (void)containerViewDidLayoutSubviews {
  [super containerViewDidLayoutSubviews];
  if (!self.presented) {
    // By updating the dimmer view frame in |dismissalTransitionWillBegin|, this
    // method is called. This method should not update the presented view frame,
    // while being dismissed, to avoid unwanted glitches.
    return;
  }
  CGRect presentedFrame = [self frameOfPresentedViewInContainerView];
  self.presentedViewController.view.frame = presentedFrame;
  [self.navigationController didUpdateControllerViewFrame];
}

#pragma mark - Private

// Closes the bottom sheet.
- (void)closeButtonAction:(id)sender {
  [self.presentationDelegate
      bottomSheetPresentationControllerDismissViewController:self];
}

@end
