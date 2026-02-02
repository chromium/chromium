// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_WELCOME_SCREEN_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_WELCOME_SCREEN_UTIL_H_

#import <Foundation/Foundation.h>

namespace webauthn {
enum class PasskeyWelcomeScreenPurpose;
}  // namespace webauthn

@class PasskeyWelcomeScreenStrings;

// Returns strings needed in the welcome string for `purpose`. `userEmail` is
// needed for `PasskeyWelcomeScreenPurpose::kEnroll`, otherwise can be nil.
PasskeyWelcomeScreenStrings* GetPasskeyWelcomeScreenStrings(
    webauthn::PasskeyWelcomeScreenPurpose purpose,
    NSString* userEmail);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_WELCOME_SCREEN_UTIL_H_
