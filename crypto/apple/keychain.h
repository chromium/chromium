// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_H_
#define CRYPTO_APPLE_KEYCHAIN_H_

#include <Security/Security.h>

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto::apple {

// Wraps the KeychainServices API in a very thin layer, to allow it to be
// mocked out for testing.

// See Keychain Services documentation for function documentation, as these call
// through directly to their Keychain Services equivalents (Foo ->
// SecKeychainFoo).
//
// TODO(https://crbug.com/441317288): Unify with KeychainV2.
class CRYPTO_EXPORT Keychain {
 public:
  // Returns an object suitable for accessing the platform's default type of
  // keychain.
  //
  // On macOS, this will access the default file-based keychain. On
  // iOS, this will access the application's data protection keychain.
  static std::unique_ptr<Keychain> DefaultKeychain();

  Keychain(const Keychain&) = delete;
  Keychain& operator=(const Keychain&) = delete;

  virtual ~Keychain();

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
  Keychain();
};

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_KEYCHAIN_H_
