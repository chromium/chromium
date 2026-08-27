// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/coordinator/level_up_promo_coordinator.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_promo_coordinator_delegate.h"
#import "ios/chrome/browser/level_up/ui/level_up_promo_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface LevelUpPromoCoordinator () <PromoStyleViewControllerDelegate>
@end

@implementation LevelUpPromoCoordinator {
  LevelUpPromoViewController* _viewController;
  UINavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[LevelUpPromoViewController alloc] init];
  _viewController.delegate = self;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [_navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _viewController.delegate = nil;
  _viewController = nil;
  _navigationController = nil;

  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self.delegate levelUpPromoCoordinatorDidOptIn:self];
}

- (void)didTapSecondaryActionButton {
  [self.delegate levelUpPromoCoordinatorDidCancel:self];
}

@end
