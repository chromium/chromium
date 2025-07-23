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
};

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_FAKE_KEYCHAIN_V2_H_
