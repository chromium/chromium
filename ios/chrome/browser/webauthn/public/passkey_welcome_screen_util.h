// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_PASSKEY_WELCOME_SCREEN_UTIL_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_PASSKEY_WELCOME_SCREEN_UTIL_H_

#import <string>

#import "components/webauthn/ios/passkey_types.h"

@class PasskeyWelcomeScreenStrings;

// Returns strings needed in the welcome string for `purpose`. `userEmail` is
// needed for `PasskeyWelcomeScreenPurpose::kEnroll`, otherwise can be nil.
PasskeyWelcomeScreenStrings* GetPasskeyWelcomeScreenStrings(
    webauthn::PasskeyWelcomeScreenPurpose purpose,
    std::string userEmail);

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_PASSKEY_WELCOME_SCREEN_UTIL_H_
