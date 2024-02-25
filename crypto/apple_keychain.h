// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_H_
#define CRYPTO_APPLE_KEYCHAIN_H_

#include <Security/Security.h>

#include <optional>

#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

#if BUILDFLAG(IS_IOS)
using AppleSecKeychainItemRef = void*;
#else
using AppleSecKeychainItemRef = SecKeychainItemRef;
#endif

// DEPRECATED: use `AppleKeychainV2` instead.
// Wraps the KeychainServices API in a very thin layer, to allow it to be
// mocked out for testing.

// See Keychain Services documentation for function documentation, as these call
// through directly to their Keychain Services equivalents (Foo ->
// SecKeychainFoo). The only exception is Free, which should be used for
// anything returned from this class that would normally be freed with
// CFRelease (to aid in testing).
//
// The underlying API was deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251
// New code should use AppleKeychainV2.
class CRYPTO_EXPORT AppleKeychain {
 public:
  AppleKeychain();

  AppleKeychain(const AppleKeychain&) = delete;
  AppleKeychain& operator=(const AppleKeychain&) = delete;

  virtual ~AppleKeychain();

  virtual OSStatus FindGenericPassword(UInt32 service_name_length,
                                       const char* service_name,
                                       UInt32 account_name_length,
                                       const char* account_name,
                                       UInt32* password_length,
                                       void** password_data,
                                       AppleSecKeychainItemRef* item) const;

  virtual OSStatus ItemFreeContent(void* data) const;

  virtual OSStatus AddGenericPassword(UInt32 service_name_length,
                                      const char* service_name,
                                      UInt32 account_name_length,
                                      const char* account_name,
                                      UInt32 password_length,
                                      const void* password_data,
                                      AppleSecKeychainItemRef* item) const;

#if BUILDFLAG(IS_MAC)
  virtual OSStatus ItemDelete(AppleSecKeychainItemRef item) const;
#endif  // !BUILDFLAG(IS_MAC)
};

#if BUILDFLAG(IS_MAC)

// Sets whether Keychain Services is permitted to display UI if needed by
// calling SecKeychainSetUserInteractionAllowed. This operates in a scoped
// fashion: on destruction, the previous state will be restored. This is useful
// to interact with the Keychain on a best-effort basis, without displaying any
// Keychain Services UI (which is beyond the application's control) to the user.
class CRYPTO_EXPORT ScopedKeychainUserInteractionAllowed {
 public:
  ScopedKeychainUserInteractionAllowed(
      const ScopedKeychainUserInteractionAllowed&) = delete;
  ScopedKeychainUserInteractionAllowed& operator=(
      const ScopedKeychainUserInteractionAllowed&) = delete;

  explicit ScopedKeychainUserInteractionAllowed(Boolean allowed,
                                                OSStatus* status = nullptr);

  ~ScopedKeychainUserInteractionAllowed();

 private:
  std::optional<Boolean> was_allowed_;
};

#endif  // BUILDFLAG(IS_MAC)

}  // namespace crypto

#endif  // CRYPTO_APPLE_KEYCHAIN_H_
