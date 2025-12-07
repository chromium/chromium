// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_WELCOME_SCREEN_UTIL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_WELCOME_SCREEN_UTIL_H_

#import <Foundation/Foundation.h>

@class PasskeyWelcomeScreenStrings;
enum class PasskeyWelcomeScreenPurpose;

// Returns strings needed in the welcome string for `purpose`. `userEmail` is
// needed for `PasskeyWelcomeScreenPurpose::kEnroll`, otherwise can be nil.
PasskeyWelcomeScreenStrings* GetPasskeyWelcomeScreenStrings(
    PasskeyWelcomeScreenPurpose purpose,
    NSString* userEmail);

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_WELCOME_SCREEN_UTIL_H_
