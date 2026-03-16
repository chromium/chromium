// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_REAUTH_TEST_REAUTHENTICATION_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_DEVICE_REAUTH_TEST_REAUTHENTICATION_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

// App interface to interact with the reauthentication service.
@interface ReauthenticationAppInterface : NSObject

// Mocks the expected result of the device authentication.
+ (void)mockReauthenticationModuleExpectedResult:
    (ReauthenticationResult)expectedResult;

// Mocks whether the reauth module can attempt reauth, i.e. whether any way of
// authenticating is enabled on the device.
+ (void)mockReauthenticationModuleCanAttempt:(BOOL)canAttempt;

// Whether the mock reauthentication module should return the result when the
// reauthentication request is made or wait for explicitly invoking the
// `mockReauthenticationModuleReturnMockedResult` method.
//
// Use it for testing state before the result is returned (e.g. View X
// shouldn't be visible until successful reauth).
+ (void)mockReauthenticationModuleShouldSkipReAuth:(BOOL)shouldSkip;

// Makes the mock reauthentication module return its mocked result by invoking
// the handler of the last reauthentication request.
+ (void)mockReauthenticationModuleReturnMockedResult;

@end

#endif  // IOS_CHROME_BROWSER_DEVICE_REAUTH_TEST_REAUTHENTICATION_APP_INTERFACE_H_
