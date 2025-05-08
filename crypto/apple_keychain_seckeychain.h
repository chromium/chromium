// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_SECKEYCHAIN_H_
#define CRYPTO_APPLE_KEYCHAIN_SECKEYCHAIN_H_

#include "crypto/apple_keychain.h"

namespace crypto {

// An implementation of AppleKeychain on top of the deprecated SecKeychain API.
//
// The underlying API was deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/1348251
class CRYPTO_EXPORT AppleKeychainSecKeychain : public AppleKeychain {
 public:
  AppleKeychainSecKeychain();
  ~AppleKeychainSecKeychain() override;

  base::expected<std::vector<uint8_t>, OSStatus> FindGenericPassword(
      std::string_view service_name,
      std::string_view account_name) const override;

  OSStatus AddGenericPassword(
      std::string_view service_name,
      std::string_view account_name,
      base::span<const uint8_t> password) const override;
};

}  // namespace crypto

#endif  // CRYPTO_APPLE_KEYCHAIN_SECKEYCHAIN_H_
