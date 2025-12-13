// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_PASSKEY_WELCOME_SCREEN_UTIL_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_PASSKEY_WELCOME_SCREEN_UTIL_H_

#import <UIKit/UIKit.h>

#import <string>

#import "base/ios/block_types.h"

@protocol PasskeyWelcomeScreenViewControllerDelegate;
enum class PasskeyWelcomeScreenPurpose;

// Creates a passkey welcome screen for `purpose` and pushes it on the provided
// `navigationController`. `primaryButtonAction` is invoked on the primary
// button press. `navigationController` and `delegate` should not be nil.
void CreateAndPresentPasskeyWelcomeScreen(
    PasskeyWelcomeScreenPurpose purpose,
    UINavigationController* navigationController,
    id<PasskeyWelcomeScreenViewControllerDelegate> delegate,
    ProceduralBlock primaryButtonAction,
    std::string userEmail);

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_PASSKEY_WELCOME_SCREEN_UTIL_H_
