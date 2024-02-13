// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_FAKE_APPLE_KEYCHAIN_V2_H_
#define CRYPTO_FAKE_APPLE_KEYCHAIN_V2_H_

#include <string>
#include <vector>

#import <Foundation/Foundation.h>

#include "base/apple/scoped_cftyperef.h"
#include "crypto/apple_keychain_v2.h"
#include "crypto/crypto_export.h"

namespace crypto {

// FakeAppleKeychainV2 is an implementation of AppleKeychainV2 for testing. It
// works around behavior that can't be relied on in tests, such as writing to
// the actual Keychain or using functionality that requires code-signed,
// entitled builds.
class CRYPTO_EXPORT FakeAppleKeychainV2 : public AppleKeychainV2 {
 public:
  explicit FakeAppleKeychainV2(const std::string& keychain_access_group);
  FakeAppleKeychainV2(const FakeAppleKeychainV2&) = delete;
  FakeAppleKeychainV2& operator=(const FakeAppleKeychainV2&) = delete;
  ~FakeAppleKeychainV2() override;

  // FakeAppleKeychainV2:
  base::apple::ScopedCFTypeRef<SecKeyRef> KeyCreateRandomKey(
      CFDictionaryRef params,
      CFErrorRef* error) override;
  OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) override;
  OSStatus ItemDelete(CFDictionaryRef query) override;
  OSStatus ItemUpdate(CFDictionaryRef query,
                      base::apple::ScopedCFTypeRef<CFMutableDictionaryRef>
                          keychain_data) override;

 private:
  // items_ contains the keychain items created by `KeyCreateRandomKey`.
  std::vector<base::apple::ScopedCFTypeRef<CFDictionaryRef>> items_;
  // keychain_access_group_ is the value of `kSecAttrAccessGroup` that this
  // keychain expects to operate on.
  base::apple::ScopedCFTypeRef<CFStringRef> keychain_access_group_;
};

// ScopedFakeAppleKeychainV2 installs itself as testing override for
// `AppleKeychainV2::GetInstance()`.
class CRYPTO_EXPORT ScopedFakeAppleKeychainV2 : public FakeAppleKeychainV2 {
 public:
  explicit ScopedFakeAppleKeychainV2(const std::string& keychain_access_group);
  ~ScopedFakeAppleKeychainV2() override;
};

}  // namespace crypto

#endif  // CRYPTO_FAKE_APPLE_KEYCHAIN_V2_H_
