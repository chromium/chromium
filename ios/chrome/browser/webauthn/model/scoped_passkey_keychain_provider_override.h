// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBAUTHN_MODEL_SCOPED_PASSKEY_KEYCHAIN_PROVIDER_OVERRIDE_H_
#define IOS_CHROME_BROWSER_WEBAUTHN_MODEL_SCOPED_PASSKEY_KEYCHAIN_PROVIDER_OVERRIDE_H_

#import <memory>

#import "base/functional/callback.h"
#import "ios/chrome/common/credential_provider/passkey_keychain_provider.h"

// Scoped class to override the PasskeyKeychainProvider in tests.
class ScopedPasskeyKeychainProviderOverride {
 public:
  ~ScopedPasskeyKeychainProviderOverride();

  // Returns a provider if set, or nullptr.
  static PasskeyKeychainProvider* Get();

  // Creates a scoped override so that the provided fake passkey keychain
  // provider will be used in place of the production implementation.
  static std::unique_ptr<ScopedPasskeyKeychainProviderOverride>
  MakeAndArmForTesting(
      std::unique_ptr<PasskeyKeychainProvider> passkey_keychain_provider);

  // The passkey keychain provider to use for testing.
  std::unique_ptr<PasskeyKeychainProvider> passkey_keychain_provider;

 private:
  ScopedPasskeyKeychainProviderOverride();
};

#endif  // IOS_CHROME_BROWSER_WEBAUTHN_MODEL_SCOPED_PASSKEY_KEYCHAIN_PROVIDER_OVERRIDE_H_
