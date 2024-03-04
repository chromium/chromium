// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_coordinator.h"

#import <Foundation/Foundation.h>
#import <memory>

#import "components/autofill/core/browser/ui/payments/card_unmask_otp_input_dialog_controller_impl.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@interface OtpInputDialogCoordinator () {
  // The model layer controller. This model controller provide access to model
  // data and also handles interactions.
  std::unique_ptr<autofill::CardUnmaskOtpInputDialogControllerImpl>
      _modelController;
}

@end

@implementation OtpInputDialogCoordinator

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
  }
  return self;
}

@end
