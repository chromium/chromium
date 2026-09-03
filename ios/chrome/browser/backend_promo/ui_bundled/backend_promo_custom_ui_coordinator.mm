// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_coordinator.h"

#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_coordinator_delegate.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_params.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_custom_ui_view_controller.h"
#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_user_action.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface BackendPromoCustomUICoordinator () <ConfirmationAlertActionHandler>
@end

@implementation BackendPromoCustomUICoordinator {
  BackendPromoCustomUIParams* _params;
  UIViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(BackendPromoCustomUIParams*)params {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _params = params;
  }
  return self;
}

- (void)start {
  if (!IsIOSBackendPromoCustomUIEnabled()) {
    [self.delegate
        backendPromoCustomUICoordinator:self
                   didDismissWithAction:BackendPromoUserActionCancelled];
    return;
  }

  _viewController =
      [BackendPromoCustomUIViewController viewControllerWithParams:_params
                                                     actionHandler:self];

  [self.baseViewController presentViewController:_viewController
                                        animated:NO
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  _viewController = nil;
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate
      backendPromoCustomUICoordinator:self
                 didDismissWithAction:BackendPromoUserActionAccepted];
}

- (void)confirmationAlertSecondaryAction {
  [self.delegate
      backendPromoCustomUICoordinator:self
                 didDismissWithAction:BackendPromoUserActionDismissed];
}

- (void)confirmationAlertDismissed {
  [self.delegate
      backendPromoCustomUICoordinator:self
                 didDismissWithAction:BackendPromoUserActionCancelled];
}

@end
