// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/public/scoped_passkey_keychain_provider_override.h"

#import "base/check.h"

namespace {
ScopedPasskeyKeychainProviderBridgeOverride* g_instance = nullptr;
}

// static
std::unique_ptr<ScopedPasskeyKeychainProviderBridgeOverride>
ScopedPasskeyKeychainProviderBridgeOverride::MakeAndArmForTesting(  // IN-TEST
    PasskeyKeychainProviderBridge* passkey_keychain_provider_bridge) {
  DCHECK(!g_instance);
  // Using new instead of make_unique to access private constructor.
  std::unique_ptr<ScopedPasskeyKeychainProviderBridgeOverride> new_instance(
      new ScopedPasskeyKeychainProviderBridgeOverride);
  new_instance->passkey_keychain_provider_bridge =
      passkey_keychain_provider_bridge;
  g_instance = new_instance.get();
  return new_instance;
}

ScopedPasskeyKeychainProviderBridgeOverride::
    ScopedPasskeyKeychainProviderBridgeOverride() = default;

ScopedPasskeyKeychainProviderBridgeOverride::
    ~ScopedPasskeyKeychainProviderBridgeOverride() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

PasskeyKeychainProviderBridge*
ScopedPasskeyKeychainProviderBridgeOverride::Get() {
  if (g_instance) {
    return g_instance->passkey_keychain_provider_bridge;
  }
  return nil;
}
