// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_SECITEM_H_
#define CRYPTO_APPLE_KEYCHAIN_SECITEM_H_

#include "crypto/apple_keychain.h"

namespace crypto {

// An implementation of AppleKeychain on top of the SecItem API.
class CRYPTO_EXPORT AppleKeychainSecItem : public AppleKeychain {
 public:
  AppleKeychainSecItem();
  ~AppleKeychainSecItem() override;

  base::expected<std::vector<uint8_t>, OSStatus> FindGenericPassword(
      std::string_view service_name,
      std::string_view account_name) const override;

  OSStatus AddGenericPassword(
      std::string_view service_name,
      std::string_view account_name,
      base::span<const uint8_t> password) const override;
};

}  // namespace crypto

#endif  // CRYPTO_APPLE_KEYCHAIN_SECITEM_H_
