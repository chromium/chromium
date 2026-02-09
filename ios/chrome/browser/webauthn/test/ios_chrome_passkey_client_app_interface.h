// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_TEST_IOS_CHROME_PASSKEY_CLIENT_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_TEST_IOS_CHROME_PASSKEY_CLIENT_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

@interface IOSChromePasskeyClientAppInterface : NSObject

// Sets up a fake PasskeyKeychainProviderBridge for testing.
+ (void)setUpFakePasskeyKeychainProviderBridge;

// Sets up a mock ReauthenticationModule for testing. By default, it will
// return success. Use setMockReauthenticationResult: to change the result.
+ (void)setUpMockReauthenticationModule;

// Tears down the mock ReauthenticationModule.
+ (void)removeMockReauthenticationModule;

// Sets the expected result for the mock ReauthenticationModule.
+ (void)setMockReauthenticationResult:(ReauthenticationResult)result;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_TEST_IOS_CHROME_PASSKEY_CLIENT_APP_INTERFACE_H_
