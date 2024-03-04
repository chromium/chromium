// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_OTP_INPUT_DIALOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_OTP_INPUT_DIALOG_COORDINATOR_H_

#import <Foundation/Foundation.h>
#import <memory>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace autofill {
class CardUnmaskOtpInputDialogControllerImpl;
}  // namespace autofill

// The coordinator responsible for managing the card unmask otp input dialog.
// This dialog is responsible for taking in OTP (one time password) input
// during the card unmasking.
@interface OtpInputDialogCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UINavigationController*)navigationController
                       browser:(Browser*)browser
               modelController:
                   (std::unique_ptr<
                       autofill::CardUnmaskOtpInputDialogControllerImpl>)
                       modelController NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_OTP_INPUT_DIALOG_COORDINATOR_H_
