// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_SCOPED_FAKE_KEYCHAIN_V2_H_
#define CRYPTO_APPLE_SCOPED_FAKE_KEYCHAIN_V2_H_

#include <memory>
#include <string>

#include "crypto/crypto_export.h"

namespace crypto::apple {

class FakeKeychainV2;

// ScopedFakeKeychainV2 installs itself as testing override for
// `KeychainV2::GetInstance()`.
class CRYPTO_EXPORT ScopedFakeKeychainV2 {
 public:
  // Supported types of user verification, reported by
  // LAContextCanEvaluatePolicy.
  enum class UVMethod {
    kNone,
    kPasswordOnly,
    kBiometrics,
  };

  explicit ScopedFakeKeychainV2(const std::string& keychain_access_group);
  ~ScopedFakeKeychainV2();

  FakeKeychainV2* keychain() { return keychain_.get(); }

  void SetUVMethod(UVMethod uv_method);

 private:
  std::unique_ptr<FakeKeychainV2> keychain_;
};

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_SCOPED_FAKE_KEYCHAIN_V2_H_
