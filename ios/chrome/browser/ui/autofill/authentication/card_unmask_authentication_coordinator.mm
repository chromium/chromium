// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_coordinator.h"

#import "ios/chrome/browser/ui/autofill/authentication/card_unmask_authentication_selection_coordinator.h"
#import "ios/chrome/browser/ui/autofill/authentication/otp_input_dialog_coordinator.h"

@implementation CardUnmaskAuthenticationCoordinator {
  // This coordinator will present sub-coordinators in a UINavigationController.
  UINavigationController* _navigationController;

  // This sub-coordinator is used to prompt the user to select a particular
  // authentication method.
  CardUnmaskAuthenticationSelectionCoordinator* _selectionCoordinator;

  // This sub-coordinator is used to prompt the user to type in the OTP value
  // received via text message for the card verification purposes.
  OtpInputDialogCoordinator* _otpInputCoordinator;
}

- (void)start {
  _navigationController = [[UINavigationController alloc] init];
  _navigationController.modalPresentationStyle = UIModalPresentationPageSheet;

  _selectionCoordinator = [[CardUnmaskAuthenticationSelectionCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  [_selectionCoordinator start];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [_selectionCoordinator stop];
  _selectionCoordinator = nil;
  [_otpInputCoordinator stop];
  _otpInputCoordinator = nil;
}

- (void)continueCardUnmaskWithOtpAuth {
  _otpInputCoordinator = [[OtpInputDialogCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  [_otpInputCoordinator start];
}

@end
