// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_presentation_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/util/accessibility_close_menu_button.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_constants.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_navigation_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Alpha for the background dimmer view.
constexpr CGFloat kBackgroundDimmerViewAlpha = .4;

}  // namespace

@interface AccountPickerScreenPresentationController () <
    AccountPickerScreenNavigationControllerLayoutDelegate>

// View controller to present.
@property(nonatomic, strong)
    AccountPickerScreenNavigationController* navigationController;
// YES if the presented view is presented.
@property(nonatomic, assign) BOOL presented;
// View to dim the UI in the background of the consistency sheet.
@property(nonatomic, strong) UIView* backgroundDimmerView;

@end

@implementation AccountPickerScreenPresentationController

- (instancetype)
    initWithAccountPickerScreenNavigationController:
        (AccountPickerScreenNavigationController*)navigationController
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
  switch (self.navigationController.displayStyle) {
    case AccountPickerSheetDisplayStyle::kCentered: {
      CGFloat availableWidth = CGRectGetWidth(presentedViewFrame);
      CGFloat availableHeight = CGRectGetHeight(presentedViewFrame);

      CGFloat width = availableWidth / 2.;
      CGFloat height = MIN(
          [self.navigationController layoutFittingSizeForWidth:width].height,
          availableHeight * kMaxPickAccountHeightRatioWithWindow);

      presentedViewFrame.origin.x += (availableWidth - width) / 2.;
      presentedViewFrame.origin.y += (availableHeight - height) / 2.;
      presentedViewFrame.size.width = width;
      presentedViewFrame.size.height = height;
      break;
    }
    case AccountPickerSheetDisplayStyle::kBottom: {
      CGSize size = [self.navigationController
          layoutFittingSizeForWidth:self.containerView.frame.size.width];
      presentedViewFrame.origin.y =
          presentedViewFrame.size.height - size.height;
      presentedViewFrame.size = size;
      break;
    }
  }
  return presentedViewFrame;
}

- (void)presentationTransitionWillBegin {
  [super presentationTransitionWillBegin];
  DCHECK(self.navigationController == self.presentedViewController);
  DCHECK(!self.backgroundDimmerView);
  DCHECK(!self.presented);
  self.presented = YES;
  self.navigationController.layoutDelegate = self;

  // Accessibility.
  self.containerView.accessibilityViewIsModal = YES;

  // Add dimmer effect.
  self.backgroundDimmerView = [[UIView alloc] init];
  self.backgroundDimmerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundDimmerView.backgroundColor = [UIColor clearColor];
  [self.containerView addSubview:self.backgroundDimmerView];
  AddSameConstraints(self.containerView, self.backgroundDimmerView);
  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(backgroundTapped:)];
  [self.backgroundDimmerView addGestureRecognizer:tapGesture];

  // Add presented view controller.
  switch (self.navigationController.displayStyle) {
    case AccountPickerSheetDisplayStyle::kCentered: {
      [self.containerView addSubview:self.presentedViewController.view];
      CGRect presentedFrame = [self frameOfPresentedViewInContainerView];
      presentedFrame.origin.y = self.containerView.bounds.size.height;
      self.presentedViewController.view.frame = presentedFrame;
      break;
    }
    case AccountPickerSheetDisplayStyle::kBottom: {
      [self.containerView addSubview:self.presentedViewController.view];
      self.presentedViewController.view.frame =
          [self frameOfPresentedViewInContainerView];
      break;
    }
  }

  // Update the dimmer view and background transparency view.
  [self.navigationController didUpdateControllerViewFrame];

  // Animate dimmer view color, and the dimmer view and background view
  // positions.
  __weak __typeof(self) weakSelf = self;
  [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf doPresentationTransitionAnimation];
      }
                      completion:nil];
}

- (void)dismissalTransitionWillBegin {
  [super dismissalTransitionWillBegin];
  DCHECK(self.backgroundDimmerView);
  DCHECK(self.presented);
  self.presented = NO;
  self.navigationController.layoutDelegate = nil;
  // Remove dimmer color and update the views.
  __weak __typeof(self) weakSelf = self;
  [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf doDismissalTransitionAnimation];
      }
                      completion:nil];
}

- (void)containerViewDidLayoutSubviews {
  [super containerViewDidLayoutSubviews];
  if (!self.presented) {
    // By updating the dimmer view frame in `dismissalTransitionWillBegin`, this
    // method is called. This method should not update the presented view frame,
    // while being dismissed, to avoid unwanted glitches.
    return;
  }
  CGRect presentedFrame = [self frameOfPresentedViewInContainerView];
  self.presentedViewController.view.frame = presentedFrame;
  [self.navigationController didUpdateControllerViewFrame];
}

#pragma mark - AccountPickerScreenNavigationControllerLayoutDelegate

- (void)preferredContentSizeDidChangeForAccountPickerScreenViewController {
  [self containerViewDidLayoutSubviews];
}
#pragma mark - Tap recogniser

- (void)backgroundTapped:(UITapGestureRecognizer*)gestureRecognizer {
  [self.actionDelegate
      accountPickerScreenPresentationControllerBackgroundTapped:self];
}

#pragma mark - Private

- (void)doPresentationTransitionAnimation {
  [self.navigationController didUpdateControllerViewFrame];
  self.backgroundDimmerView.backgroundColor =
      [UIColor colorWithWhite:0 alpha:kBackgroundDimmerViewAlpha];
}

- (void)doDismissalTransitionAnimation {
  self.backgroundDimmerView.backgroundColor = UIColor.clearColor;
  [self.navigationController didUpdateControllerViewFrame];
}

@end
