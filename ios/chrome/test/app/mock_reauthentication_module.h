// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_MOCK_REAUTHENTICATION_MODULE_H_
#define IOS_CHROME_TEST_APP_MOCK_REAUTHENTICATION_MODULE_H_

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// Mock reauthentication module, used by eg tests in order to fake
// reauthentication.
@interface MockReauthenticationModule : NSObject <ReauthenticationProtocol>

// Localized string containing the reason why reauthentication is requested.
@property(nonatomic, copy) NSString* localizedReasonForAuthentication;

// Indicates whether the device is capable of reauthenticating the user with
// Biometric auth.
@property(nonatomic, assign) BOOL canAttemptWithBiometrics;

// Indicates whether the device is capable of reauthenticating the user.
@property(nonatomic, assign) BOOL canAttempt;

// Indicates whether (mock) authentication should succeed or not. Setting
// `shouldSucceed` to any value sets `canAttemptWithBiometrics` and `canAttempt`
// to YES.
@property(nonatomic, assign) ReauthenticationResult expectedResult;

// Whether the mock module should return the mocked result when the
// reauthentication request is made or wait for
// `returnMockedReauthenticationResult` to be invoked. Defaults to YES. Use it
// for testing some state while authentication is being requested.
@property(nonatomic, assign) BOOL shouldSkipReAuth;

// Invokes the last handler passed to attemptReauthWithLocalizedReason with
// `expectedResult`.
- (void)returnMockedReauthenticationResult;

@end

#endif  // IOS_CHROME_TEST_APP_MOCK_REAUTHENTICATION_MODULE_H_
