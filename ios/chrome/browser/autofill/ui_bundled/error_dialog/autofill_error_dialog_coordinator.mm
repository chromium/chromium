// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_coordinator.h"

#import <memory>

#import "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#import "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller_impl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/error_dialog/autofill_error_dialog_mediator_delegate.h"

@implementation AutofillErrorDialogCoordinator {
  // The model layer controller. This model controller provide access to model
  // data and also handles interactions.
  std::unique_ptr<autofill::AutofillErrorDialogControllerImpl> _modelController;

  // The C++ mediator class that connects the model controller and the IOS view
  // implementation.
  std::unique_ptr<AutofillErrorDialogMediator> _mediator;

  __weak UIAlertController* _alertController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              errorContext:
                                  (autofill::AutofillErrorDialogContext)
                                      errorContext {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _modelController =
        std::make_unique<autofill::AutofillErrorDialogControllerImpl>(
            std::move(errorContext));
    _mediator = std::make_unique<AutofillErrorDialogMediator>(
        _modelController->GetWeakPtr(), self);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Displays the error dialog. This is routed through the model-layer
  // controller, which then invokes the mediator. This is done to allow the
  // model to have a handle on the mediator, which implements a required
  // model-layer interface.
  // base::Unretained here is safe since the callback is invoked immediately
  // after being passed to the `_modelController`.
  _modelController->Show(base::BindOnce(&AutofillErrorDialogMediator::Show,
                                        base::Unretained(_mediator.get())));
}

- (void)stop {
  [_alertController dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - AutofillErrorDialogMediatorDelegate

- (void)showErrorDialog:(NSString*)title
                message:(NSString*)message
            buttonLabel:(NSString*)buttonLabel {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  __weak __typeof__(self) weakSelf = self;
  UIAlertAction* buttonAction =
      [UIAlertAction actionWithTitle:buttonLabel
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               [weakSelf dismissViewController];
                             }];
  [alertController addAction:buttonAction];
  alertController.modalPresentationStyle = UIModalPresentationOverFullScreen;
  [self.baseViewController presentViewController:alertController
                                        animated:YES
                                      completion:nil];
  _alertController = alertController;
}

#pragma mark - Private

- (void)dismissViewController {
  // Terminate everything via the browser command. `_modelController` will get
  // notified when the AutofillErrorDialogMediator is being destroyed.
  [_autofillCommandsHandler dismissAutofillErrorDialog];
}

@end
