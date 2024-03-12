// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// This class coordinates the card unmasking flow including authentication
// selection and particular authentication methods such as SMS OTP (one time
// passwords via text messages) or CVC (security codes).
@interface CardUnmaskAuthenticationCoordinator : ChromeCoordinator

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_AUTHENTICATION_CARD_UNMASK_AUTHENTICATION_COORDINATOR_H_
