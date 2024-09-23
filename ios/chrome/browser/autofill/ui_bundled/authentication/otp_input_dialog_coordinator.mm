// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_coordinator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_mediator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/authentication/otp_input_dialog_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"

@interface OtpInputDialogCoordinator () <OtpInputDialogMediatorDelegate>
@end

@implementation OtpInputDialogCoordinator {
  // A reference to the base view controller with UINavigationController type.
  __weak UINavigationController* _baseNavigationController;

  // The model layer controller. This model controller provide access to model
  // data and also handles interactions.
  std::unique_ptr<autofill::CardUnmaskOtpInputDialogControllerImpl>
      _modelController;

  // The C++ bridge class to connect Autofill model controller with the view
  // implementation. Note that the destruction order of the `_modelController`
  // and `_mediator` matters here. Need to make sure `_modelController` is
  // destroyed after the `_mediator` so that the `_modelController` is
  // correctly notified of the closure.
  std::unique_ptr<OtpInputDialogMediator> _mediator;

  __weak OtpInputDialogViewController* _viewController;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    autofill::ChromeAutofillClientIOS* client =
        AutofillTabHelper::FromWebState(
            browser->GetWebStateList()->GetActiveWebState())
            ->autofill_client();
    CHECK(client);
    auto* paymentsClient = client->GetPaymentsAutofillClient();
    CHECK(paymentsClient);
    _modelController = paymentsClient->GetOtpInputDialogModel();
    _mediator = std::make_unique<OtpInputDialogMediator>(
        _modelController->GetImplWeakPtr(), self);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  auto viewController = [[OtpInputDialogViewController alloc] init];
  _viewController = viewController;
  _mediator->SetConsumer(_viewController);
  _viewController.mutator = _mediator->AsMutator();
  [_baseNavigationController pushViewController:viewController animated:YES];
  __weak __typeof__(self) weakSelf = self;
  _modelController->ShowDialog(
      base::BindOnce(^base::WeakPtr<autofill::CardUnmaskOtpInputDialogView>() {
        return [weakSelf otpInputdialogView];
      }));
}

- (void)stop {
  [_baseNavigationController popViewControllerAnimated:YES];
}

#pragma mark - OtpInputDialogMediatorDelegate

- (void)dismissDialog {
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorCommandsHandler dismissCardUnmaskAuthentication];
}

#pragma mark - Private

- (base::WeakPtr<autofill::CardUnmaskOtpInputDialogView>)otpInputdialogView {
  return _mediator->GetWeakPtr();
}

@end
