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

// Indicates whether the device is capable of reauthenticating the user.
@property(nonatomic, assign) BOOL canAttempt;

// Indicates whether (mock) authentication should succeed or not. Setting
// `shouldSucceed` to any value sets `canAttempt` to YES.
@property(nonatomic, assign) ReauthenticationResult expectedResult;

@end

#endif  // IOS_CHROME_TEST_APP_MOCK_REAUTHENTICATION_MODULE_H_
