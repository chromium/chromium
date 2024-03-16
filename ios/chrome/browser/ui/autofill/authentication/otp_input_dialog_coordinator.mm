// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_coordinator.h"

#import <Foundation/Foundation.h>
#import <memory>

#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_mediator.h"

@implementation OtpInputDialogCoordinator {
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
}

- (instancetype)
    initWithBaseViewController:(UINavigationController*)navigationController
                       browser:(Browser*)browser
               modelController:
                   (std::unique_ptr<
                       autofill::CardUnmaskOtpInputDialogControllerImpl>)
                       modelController {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _modelController = std::move(modelController);
    _mediator = std::make_unique<OtpInputDialogMediator>(
        _modelController->GetImplWeakPtr());
  }
  return self;
}

@end
