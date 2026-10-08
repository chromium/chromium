// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_REAUTHENTICATION_FAKE_REAUTHENTICATION_MODULE_H_
#define IOS_CHROME_COMMON_UI_REAUTHENTICATION_FAKE_REAUTHENTICATION_MODULE_H_

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// Fake reauthentication module, used by tests in order to fake
// reauthentication.
@interface FakeReauthenticationModule : NSObject <ReauthenticationProtocol>

// Localized string containing the reason why reauthentication is requested.
@property(nonatomic, copy) NSString* localizedReasonForAuthentication;

// Indicates whether the device is capable of reauthenticating the user with
// Biometric auth.
@property(nonatomic, assign) BOOL canAttemptWithBiometrics;

// Indicates whether the device is capable of reauthenticating the user.
@property(atomic, assign) BOOL canAttempt;

// Indicates whether (fake) authentication should succeed or not. Setting
// `shouldSucceed` to any value sets `canAttemptWithBiometrics` and `canAttempt`
// to YES.
@property(nonatomic, assign) ReauthenticationResult expectedResult;

// Whether the fake module should return the mocked result when the
// reauthentication request is made or wait for
// `returnMockedReauthenticationResult` to be invoked. Defaults to YES. Use it
// for testing some state while authentication is being requested.
@property(nonatomic, assign) BOOL shouldSkipReAuth;

// Invokes the last handler passed to attemptReauthWithLocalizedReason with
// `expectedResult`.
- (void)returnMockedReauthenticationResult;

@end

#endif  // IOS_CHROME_COMMON_UI_REAUTHENTICATION_FAKE_REAUTHENTICATION_MODULE_H_
