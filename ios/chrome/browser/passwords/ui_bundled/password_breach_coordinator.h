// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_COORDINATOR_H_

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Presents and stops the Password Breach feature, which consists in alerting
// the user that Chrome detected a leaked credential. In some scenarios it
// prompts for a checkup of the stored passwords.
@interface PasswordBreachCoordinator : ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
                       browser:(Browser*)browser
                      leakType:(password_manager::CredentialLeakType)leakType
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_PASSWORD_BREACH_COORDINATOR_H_
