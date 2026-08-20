// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/coordinator/passkey_incognito_interstitial_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/webauthn/ui/passkey_incognito_interstitial_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

// Represents the user's interaction with the incognito interstitial dialog.
enum class IncognitoInterstitialAction { kDismissed, kContinue, kCancel };

@interface PasskeyIncognitoInterstitialCoordinator () <
    ConfirmationAlertActionHandler,
    PasskeyIncognitoInterstitialViewControllerDelegate>
@end

@implementation PasskeyIncognitoInterstitialCoordinator {
  // The callback to run when the user makes a decision.
  base::OnceCallback<void(bool)> _callback;

  // The view controller being managed by this coordinator.
  PasskeyIncognitoInterstitialViewController* _viewController;

  // Tracks the user's interaction with the incognito interstitial dialog.
  IncognitoInterstitialAction _userAction;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  callback:
                                      (base::OnceCallback<void(bool)>)callback {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _callback = std::move(callback);
    _userAction = IncognitoInterstitialAction::kDismissed;
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

  bool result = (_userAction == IncognitoInterstitialAction::kContinue);

  if (!_viewController.presentingViewController && _callback) {
    std::move(_callback).Run(result);
    _viewController = nil;
    return;
  }

  // Run the callback only after the dismissal animation completes. This
  // prevents a presentation race condition where the backend tries to present
  // the next sheet while the current sheet is still animating away.
  ProceduralBlock completionBlock = nil;
  if (_callback) {
    completionBlock =
        base::CallbackToBlock(base::BindOnce(std::move(_callback), result));
  }

  [self.baseViewController dismissViewControllerAnimated:YES
                                              completion:completionBlock];
  _viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  _userAction = IncognitoInterstitialAction::kContinue;
  [self dismissInterstitial];
}

- (void)confirmationAlertSecondaryAction {
  _userAction = IncognitoInterstitialAction::kCancel;
  [self dismissInterstitial];
}

#pragma mark - PasskeyIncognitoInterstitialViewControllerDelegate

- (void)passkeyIncognitoInterstitialViewDidDisappear {
  if (_callback) {
    std::move(_callback).Run(false);
  }
  [self dismissInterstitial];
}

#pragma mark - Private

// Dismisses the passkey incognito interstitial.
- (void)dismissInterstitial {
  id<IOSPasskeyClientCommands> passkeyClientHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), IOSPasskeyClientCommands);
  [passkeyClientHandler dismissPasskeyIncognitoInterstitial];
}

@end
