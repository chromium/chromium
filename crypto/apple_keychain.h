// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_H_
#define CRYPTO_APPLE_KEYCHAIN_H_

#include <Security/Security.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

// DEPRECATED: use `AppleKeychainV2` instead.
// Wraps the KeychainServices API in a very thin layer, to allow it to be
// mocked out for testing.

// See Keychain Services documentation for function documentation, as these call
// through directly to their Keychain Services equivalents (Foo ->
// SecKeychainFoo).
//
// New code should use AppleKeychainV2.
class CRYPTO_EXPORT AppleKeychain {
 public:
  // Returns an object suitable for accessing the platform's default type of
  // keychain.
  //
  // On macOS, this will access the default file-based keychain. On
  // iOS, this will access the application's data protection keychain.
  static std::unique_ptr<AppleKeychain> DefaultKeychain();

  AppleKeychain(const AppleKeychain&) = delete;
  AppleKeychain& operator=(const AppleKeychain&) = delete;

  virtual ~AppleKeychain();

  // Note that even though OSStatus has a noError value, that can never be
  // returned in the OSStatus arm of FindGenericPassword() - in that case, the
  // std::vector<uint8_t> arm is populated instead.
  virtual base::expected<std::vector<uint8_t>, OSStatus> FindGenericPassword(
      std::string_view service_name,
      std::string_view account_name) const = 0;

  virtual OSStatus AddGenericPassword(
      std::string_view service_name,
      std::string_view account_name,
      base::span<const uint8_t> password) const = 0;

 protected:
  AppleKeychain();
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
  // The previous value of whether user interaction was allowed, for
  // restoration. If this is nullopt, this scoper did not succeed in its
  // constructor, so it must not attempt to restore the value.
  std::optional<Boolean> was_allowed_;
};

#endif  // BUILDFLAG(IS_MAC)

}  // namespace crypto

#endif  // CRYPTO_APPLE_KEYCHAIN_H_
