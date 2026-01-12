// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/scoped_passkey_keychain_provider_override.h"

#import "base/check.h"

namespace {
ScopedPasskeyKeychainProviderOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedPasskeyKeychainProviderOverride>
ScopedPasskeyKeychainProviderOverride::MakeAndArmForTesting(  // IN-TEST
    std::unique_ptr<PasskeyKeychainProvider> passkey_keychain_provider) {
  DCHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedPasskeyKeychainProviderOverride> new_instance(
      new ScopedPasskeyKeychainProviderOverride);
  new_instance->passkey_keychain_provider =
      std::move(passkey_keychain_provider);
  g_instance = new_instance.get();
  return new_instance;
}

ScopedPasskeyKeychainProviderOverride::ScopedPasskeyKeychainProviderOverride() =
    default;

ScopedPasskeyKeychainProviderOverride::
    ~ScopedPasskeyKeychainProviderOverride() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

PasskeyKeychainProvider* ScopedPasskeyKeychainProviderOverride::Get() {
  if (g_instance) {
    return g_instance->passkey_keychain_provider.get();
  }
  return nullptr;
}
