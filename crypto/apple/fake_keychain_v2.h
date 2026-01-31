// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_FAKE_KEYCHAIN_V2_H_
#define CRYPTO_APPLE_FAKE_KEYCHAIN_V2_H_

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "crypto/crypto_export.h"

namespace crypto::apple {

// FakeKeychainV2 is an implementation of KeychainV2 for testing. It works
// around behavior that can't be relied on in tests, such as writing to the
// actual Keychain or using functionality that requires code-signed, entitled
// builds.
class CRYPTO_EXPORT FakeKeychainV2 : public KeychainV2 {
 public:
  using UVMethod = ScopedFakeKeychainV2::UVMethod;

  explicit FakeKeychainV2(const std::string& keychain_access_group);
  FakeKeychainV2(const FakeKeychainV2&) = delete;
  FakeKeychainV2& operator=(const FakeKeychainV2&) = delete;
  ~FakeKeychainV2() override;

  const std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>& items() {
    return items_;
  }

  void set_secure_enclave_available(bool is_secure_enclave_available) {
    is_secure_enclave_available_ = is_secure_enclave_available;
  }

  void set_uv_method(UVMethod uv_method) { uv_method_ = uv_method; }

  // KeychainV2:
  NSArray* GetTokenIDs() override;
  base::apple::ScopedCFTypeRef<SecKeyRef> KeyCreateRandomKey(
      CFDictionaryRef params,
      CFErrorRef* error) override;
  base::apple::ScopedCFTypeRef<CFDictionaryRef> KeyCopyAttributes(
      SecKeyRef key) override;
  OSStatus ItemAdd(CFDictionaryRef attributes, CFTypeRef* result) override;
  OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) override;
  OSStatus ItemDelete(CFDictionaryRef query) override;
  OSStatus ItemUpdate(CFDictionaryRef query,
                      CFDictionaryRef keychain_data) override;
  OSStatus AddGenericPassword(std::string_view service_name,
                              std::string_view account_name,
                              base::span<const uint8_t> password) override;
  base::expected<std::vector<uint8_t>, OSStatus> FindGenericPassword(
      std::string_view service_name,
      std::string_view account_name) override;

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

#if !BUILDFLAG(IS_IOS)
  base::apple::ScopedCFTypeRef<CFTypeRef> TaskCopyValueForEntitlement(
      SecTaskRef task,
      CFStringRef entitlement,
      CFErrorRef* error) override;
#endif  // !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_IOS_TVOS)
  BOOL LAContextCanEvaluatePolicy(LAPolicy policy,
                                  NSError* __autoreleasing* error) override;
#endif  // !BUILDFLAG(IS_IOS_TVOS)

 private:
  bool is_secure_enclave_available_ = true;

  UVMethod uv_method_ = UVMethod::kBiometrics;

  // items_ contains the keychain items created by `KeyCreateRandomKey`.
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> items_;
  // keychain_access_group_ is the value of `kSecAttrAccessGroup` that this
  // keychain expects to operate on.
  base::apple::ScopedCFTypeRef<CFStringRef> keychain_access_group_;

  // Result code for the |FindGenericPassword()| method.
  OSStatus find_generic_result_ = noErr;

  // Records whether |AddGenericPassword()| gets called.
  bool called_add_generic_ = false;
};

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_FAKE_KEYCHAIN_V2_H_
