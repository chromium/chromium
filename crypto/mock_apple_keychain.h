// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_MOCK_APPLE_KEYCHAIN_H_
#define CRYPTO_MOCK_APPLE_KEYCHAIN_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "crypto/apple_keychain.h"

namespace crypto {

// Mock Keychain wrapper for testing code that interacts with the OS X
// Keychain.
//
// Note that "const" is pretty much meaningless for this class; the const-ness
// of AppleKeychain doesn't apply to the actual keychain data, so all of the
// Mock data is mutable; don't assume that it won't change over the life of
// tests.
class CRYPTO_EXPORT MockAppleKeychain : public AppleKeychain {
 public:
  MockAppleKeychain();

  MockAppleKeychain(const MockAppleKeychain&) = delete;
  MockAppleKeychain& operator=(const MockAppleKeychain&) = delete;

  ~MockAppleKeychain() override;

  // AppleKeychain implementation.
  base::expected<std::vector<uint8_t>, OSStatus> FindGenericPassword(
      std::string_view service_name,
      std::string_view account_name) const override;

  OSStatus AddGenericPassword(
      std::string_view service_name,
      std::string_view account_name,
      base::span<const uint8_t> password) const override;

  // Returns the password that OSCrypt uses to generate its encryption key.
  std::string GetEncryptionPassword() const;

  // |FindGenericPassword()| can return different results depending on user
  // interaction with the system Keychain.  For mocking purposes we allow the
  // user of this class to specify the result code of the
  // |FindGenericPassword()| call so we can simulate the result of different
  // user interactions.
  void set_find_generic_result(OSStatus result) {
    find_generic_result_ = result;
  }

  // Returns the true if |AddGenericPassword()| was called.
  bool called_add_generic() const { return called_add_generic_; }

 private:
  // Result code for the |FindGenericPassword()| method.
  OSStatus find_generic_result_ = noErr;

  // Records whether |AddGenericPassword()| gets called.
  mutable bool called_add_generic_ = false;
};

}  // namespace crypto

#endif  // CRYPTO_MOCK_APPLE_KEYCHAIN_H_
