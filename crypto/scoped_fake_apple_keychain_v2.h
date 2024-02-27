// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_FAKE_APPLE_KEYCHAIN_V2_H_
#define CRYPTO_SCOPED_FAKE_APPLE_KEYCHAIN_V2_H_

#include <memory>
#include <string>

#include "crypto/crypto_export.h"

namespace crypto {

class FakeAppleKeychainV2;

// ScopedFakeAppleKeychainV2 installs itself as testing override for
// `AppleKeychainV2::GetInstance()`.
class CRYPTO_EXPORT ScopedFakeAppleKeychainV2 {
 public:
  explicit ScopedFakeAppleKeychainV2(const std::string& keychain_access_group);
  ~ScopedFakeAppleKeychainV2();

  FakeAppleKeychainV2* keychain() { return keychain_.get(); }

 private:
  std::unique_ptr<FakeAppleKeychainV2> keychain_;
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_FAKE_APPLE_KEYCHAIN_V2_H_
