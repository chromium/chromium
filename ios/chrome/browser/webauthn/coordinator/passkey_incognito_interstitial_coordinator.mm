// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_incognito_interstitial_coordinator.h"

#import "ios/chrome/browser/webauthn/ui/passkey_incognito_interstitial_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface PasskeyIncognitoInterstitialCoordinator () <
    ConfirmationAlertActionHandler,
    PasskeyIncognitoInterstitialViewControllerDelegate>
@end

@implementation PasskeyIncognitoInterstitialCoordinator {
  // The callback to run when the user makes a decision.
  base::OnceCallback<void(bool)> _callback;

  // The view controller being managed by this coordinator.
  PasskeyIncognitoInterstitialViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  callback:
                                      (base::OnceCallback<void(bool)>)callback {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _callback = std::move(callback);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[PasskeyIncognitoInterstitialViewController alloc] init];
  _viewController.actionHandler = self;
  _viewController.delegate = self;
  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;

  UISheetPresentationController* sheetPresentationController =
      _viewController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
  }

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _viewController.delegate = nil;

  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;

  if (_callback) {
    std::move(_callback).Run(false);
  }
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  if (_callback) {
    std::move(_callback).Run(true);
  }
  // TODO(crbug.com/487226407): Use a handler to dismiss the coordinator.
}

- (void)confirmationAlertSecondaryAction {
  if (_callback) {
    std::move(_callback).Run(false);
  }
  // TODO(crbug.com/487226407): Use a handler to dismiss the coordinator.
}

#pragma mark - PasskeyIncognitoInterstitialViewControllerDelegate

- (void)passkeyIncognitoInterstitialViewDidDisappear {
  if (_callback) {
    std::move(_callback).Run(false);
  }
  // TODO(crbug.com/487226407): Use a handler to dismiss the coordinator.
}

@end
