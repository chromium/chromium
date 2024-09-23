// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_coordinator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller_impl.h"
#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"
#import "ios/chrome/browser/autofill/ui_bundled/ios_chrome_payments_autofill_client.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/progress_dialog/autofill_progress_dialog_mediator_delegate.h"

@implementation AutofillProgressDialogCoordinator {
  // The model layer controller. This model controller provide access to model
  // data and also handles interactions.
  std::unique_ptr<autofill::AutofillProgressDialogControllerImpl>
      _modelController;

  // The C++ mediator class that connects the model controller and the IOS view
  // implementation.
  std::unique_ptr<AutofillProgressDialogMediator> _mediator;

  // Underlying view controller presented by this coordinator.
  __weak AlertViewController* _alertViewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    autofill::ChromeAutofillClientIOS* client =
        AutofillTabHelper::FromWebState(
            browser->GetWebStateList()->GetActiveWebState())
            ->autofill_client();
    CHECK(client);
    auto* paymentsClient = client->GetPaymentsAutofillClient();
    CHECK(paymentsClient);
    _modelController = paymentsClient->GetProgressDialogModel();
    _mediator = std::make_unique<AutofillProgressDialogMediator>(
        _modelController->GetImplWeakPtr(), self);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  AlertViewController* alertViewController = [[AlertViewController alloc] init];
  alertViewController.modalPresentationStyle =
      UIModalPresentationOverFullScreen;
  _alertViewController = alertViewController;
  _mediator->SetConsumer(alertViewController);
  [self.baseViewController presentViewController:alertViewController
                                        animated:YES
                                      completion:nil];
  // The callback is run immediately after being passed into the ShowDialog.
  // base::Unretained should not cause any lifecycle issue here.
  _modelController->ShowDialog(
      base::BindOnce(&AutofillProgressDialogMediator::GetWeakPtr,
                     base::Unretained(_mediator.get())));
}

- (void)stop {
  [_alertViewController dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - AutofillProgressDialogMediatorDelegate

- (void)dismissDialog {
  id<AutofillCommands> autofillCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AutofillCommands);
  [autofillCommandsHandler dismissAutofillProgressDialog];
}

@end
