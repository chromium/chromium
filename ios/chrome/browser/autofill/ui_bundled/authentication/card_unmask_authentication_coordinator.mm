// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_coordinator.h"

#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/card_unmask_authentication_selection_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"

@interface CardUnmaskAuthenticationCoordinator () <
    UIAdaptivePresentationControllerDelegate>
@end

@implementation CardUnmaskAuthenticationCoordinator {
  // This coordinator will present sub-coordinators in a UINavigationController.
  UINavigationController* _navigationController;

  // This sub-coordinator is used to prompt the user to select a particular
  // authentication method.
  CardUnmaskAuthenticationSelectionCoordinator* _selectionCoordinator;

  // This sub-coordinator is used to prompt the user to type in the OTP value
  // received via text message for the card verification purposes.
  OtpInputDialogCoordinator* _otpInputCoordinator;

  // This view bridge is used to prompt the user to type in the CVC value for
  // the card verification purpose.
  std::unique_ptr<autofill::CardUnmaskPromptViewBridge> _cvcInputViewBridge;

  id<BrowserCoordinatorCommands> _browserCoordinatorCommands;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _browserCoordinatorCommands = HandlerForProtocol(
        browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  }
  return self;
}

- (void)continueWithOtpAuth {
  _otpInputCoordinator = [[OtpInputDialogCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  [_otpInputCoordinator start];
}

// TODO(crbug.com/333925306): Create a CVC input coordinator/mediator out of the
// legacy CardUnmaskPromptViewBridge and move this function there.
- (void)continueWithCvcAuth {
  autofill::ChromeAutofillClientIOS* client =
      AutofillTabHelper::FromWebState(
          self.browser->GetWebStateList()->GetActiveWebState())
          ->autofill_client();
  CHECK(client);
  autofill::payments::IOSChromePaymentsAutofillClient* paymentsClient =
      client->GetPaymentsAutofillClient();
  CHECK(paymentsClient);

  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);

  autofill::CardUnmaskPromptControllerImpl* cvcInputModelController =
      paymentsClient->GetCardUnmaskPromptModel();
  _cvcInputViewBridge = std::make_unique<autofill::CardUnmaskPromptViewBridge>(
      cvcInputModelController, _navigationController,
      client->GetPersonalDataManager(), browserCoordinatorCommandsHandler);

  __weak __typeof__(self) weakSelf = self;
  cvcInputModelController->ShowPrompt(
      base::BindOnce(^autofill::CardUnmaskPromptView*() {
        return [weakSelf cardUnmaskPromptView];
      }));
}

#pragma mark - ChromeCoordinator

- (void)start {
  _navigationController = [[UINavigationController alloc] init];
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;
  if (self.shouldStartWithCvcAuth) {
    [self continueWithCvcAuth];
  } else {
    _selectionCoordinator =
        [[CardUnmaskAuthenticationSelectionCoordinator alloc]
            initWithBaseNavigationController:_navigationController
                                     browser:self.browser];
    [_selectionCoordinator start];
  }

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  _navigationController.presentationController.delegate = self;
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [_selectionCoordinator stop];
  _selectionCoordinator = nil;
  [_otpInputCoordinator stop];
  _otpInputCoordinator = nil;
  _cvcInputViewBridge.reset();
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_browserCoordinatorCommands dismissCardUnmaskAuthentication];
}

#pragma mark - Private

// TODO(crbug.com/334652807): Change this to have the model controller take a
// WeakPtr.
- (autofill::CardUnmaskPromptView*)cardUnmaskPromptView {
  return _cvcInputViewBridge.get();
}

@end
