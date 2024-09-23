// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_FAKE_APPLE_KEYCHAIN_V2_H_
#define CRYPTO_FAKE_APPLE_KEYCHAIN_V2_H_

#import <Foundation/Foundation.h>

#include <string>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"

namespace crypto {

// FakeAppleKeychainV2 is an implementation of AppleKeychainV2 for testing. It
// works around behavior that can't be relied on in tests, such as writing to
// the actual Keychain or using functionality that requires code-signed,
// entitled builds.
class CRYPTO_EXPORT FakeAppleKeychainV2 : public AppleKeychainV2 {
 public:
  using UVMethod = ScopedFakeAppleKeychainV2::UVMethod;

  explicit FakeAppleKeychainV2(const std::string& keychain_access_group);
  FakeAppleKeychainV2(const FakeAppleKeychainV2&) = delete;
  FakeAppleKeychainV2& operator=(const FakeAppleKeychainV2&) = delete;
  ~FakeAppleKeychainV2() override;

  const std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>>& items() {
    return items_;
  }

  void set_secure_enclave_available(bool is_secure_enclave_available) {
    is_secure_enclave_available_ = is_secure_enclave_available;
  }

  void set_uv_method(UVMethod uv_method) { uv_method_ = uv_method; }

  // AppleKeychainV2:
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
  BOOL LAContextCanEvaluatePolicy(LAPolicy policy,
                                  NSError* __autoreleasing* error) override;

 private:
  bool is_secure_enclave_available_ = true;

  UVMethod uv_method_ = UVMethod::kBiometrics;

  // items_ contains the keychain items created by `KeyCreateRandomKey`.
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> items_;
  // keychain_access_group_ is the value of `kSecAttrAccessGroup` that this
  // keychain expects to operate on.
  base::apple::ScopedCFTypeRef<CFStringRef> keychain_access_group_;
};

}  // namespace crypto

#endif  // CRYPTO_FAKE_APPLE_KEYCHAIN_V2_H_
