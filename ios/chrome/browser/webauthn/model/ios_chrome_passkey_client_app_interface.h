// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@interface IOSChromePasskeyClientAppInterface : NSObject

// Sets up a fake PasskeyKeychainProviderBridge for testing.
+ (void)setUpFakePasskeyKeychainProviderBridge;

@end

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_IOS_CHROME_PASSKEY_CLIENT_APP_INTERFACE_H_
