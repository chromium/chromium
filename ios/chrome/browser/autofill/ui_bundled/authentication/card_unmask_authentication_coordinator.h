// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// This class coordinates the card unmasking flow including authentication
// selection and particular authentication methods such as SMS OTP (one time
// passwords via text messages) or CVC (security codes).
@interface CardUnmaskAuthenticationCoordinator : ChromeCoordinator

// If YES, should start directly with the CVC authentication. Otherwise,
// initialize an auth selection view.
@property(nonatomic, assign) BOOL shouldStartWithCvcAuth;

// Start OTP authentication.
- (void)continueWithOtpAuth;

// Push extra dialogs with correct type to the authentication coordinator's
// navigation stack.
- (void)continueWithCvcAuth;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_COORDINATOR_H_
