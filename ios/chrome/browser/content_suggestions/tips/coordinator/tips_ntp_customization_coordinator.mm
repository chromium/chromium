// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tips/coordinator/tips_ntp_customization_coordinator.h"

#import "base/notreached.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/browser/content_suggestions/tips/ui/ntp_customization_promo_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"

@interface TipsNTPCustomizationCoordinator () <ButtonStackActionDelegate>
@end

@implementation TipsNTPCustomizationCoordinator {
  NTPCustomizationPromoViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[NTPCustomizationPromoViewController alloc] init];
  _viewController.actionDelegate = self;
  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;

  UISheetPresentationController* sheetController =
      _viewController.sheetPresentationController;
  sheetController.prefersEdgeAttachedInCompactHeight = YES;
  sheetController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  sheetController.detents = @[
    [_viewController preferredHeightDetent],
  ];

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _viewController.actionDelegate = nil;
  if (_viewController.presentingViewController) {
    [_viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }
  _viewController = nil;
  _delegate = nil;
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  if (TipsManagerIOS* tipsManager =
          TipsManagerIOSFactory::GetForProfile(self.profile)) {
    tipsManager->NotifySignal(
        segmentation_platform::tips_manager::signals::kNTPCustomized);
  }
  _viewController.actionDelegate = nil;
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf didAcceptPromo];
                         }];
}

- (void)didTapSecondaryActionButton {
  [self.delegate tipsNTPCustomizationCoordinator:self didFinishWithAccept:NO];
}

- (void)didTapTertiaryActionButton {
  NOTREACHED();
}

- (void)didDismissButtonStackViewController {
  [self.delegate tipsNTPCustomizationCoordinator:self didFinishWithAccept:NO];
}

#pragma mark - Private

- (void)didAcceptPromo {
  [self.delegate tipsNTPCustomizationCoordinator:self didFinishWithAccept:YES];
}

@end
