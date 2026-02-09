// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_SCOPED_PASSKEY_KEYCHAIN_PROVIDER_OVERRIDE_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_SCOPED_PASSKEY_KEYCHAIN_PROVIDER_OVERRIDE_H_

#import <memory>

#import "base/functional/callback.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider_bridge.h"

@class PasskeyKeychainProviderBridge;

// Scoped class to override the PasskeyKeychainProviderBridge in tests.
class ScopedPasskeyKeychainProviderBridgeOverride {
 public:
  ~ScopedPasskeyKeychainProviderBridgeOverride();

  // Returns a bridge if set, or nullptr.
  static PasskeyKeychainProviderBridge* Get();

  // Creates a scoped override so that the provided fake passkey keychain
  // provider bridge will be used in place of the production implementation.
  static std::unique_ptr<ScopedPasskeyKeychainProviderBridgeOverride>
  MakeAndArmForTesting(
      PasskeyKeychainProviderBridge* passkey_keychain_provider_bridge);

  // The passkey keychain provider bridge to use for testing.
  PasskeyKeychainProviderBridge* passkey_keychain_provider_bridge = nil;

 private:
  ScopedPasskeyKeychainProviderBridgeOverride();
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_PUBLIC_SCOPED_PASSKEY_KEYCHAIN_PROVIDER_OVERRIDE_H_
