// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_V2_H_
#define CRYPTO_APPLE_KEYCHAIN_V2_H_

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include "crypto/crypto_export.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/no_destructor.h"

namespace crypto {

// AppleKeychainV2 wraps iOS-style operations from the macOS Security framework
// to work with keys and keychain items. These functions are grouped here so
// they can be mocked out in testing.
class CRYPTO_EXPORT AppleKeychainV2 {
 public:
  static AppleKeychainV2& GetInstance();

  AppleKeychainV2(const AppleKeychainV2&) = delete;
  AppleKeychainV2& operator=(const AppleKeychainV2&) = delete;

  // KeyCreateRandomKey wraps the |SecKeyCreateRandomKey| function.
  virtual base::apple::ScopedCFTypeRef<SecKeyRef> KeyCreateRandomKey(
      CFDictionaryRef params,
      CFErrorRef* error);
  // KeyCreateSignature wraps the |SecKeyCreateSignature| function.
  virtual base::apple::ScopedCFTypeRef<CFDataRef> KeyCreateSignature(
      SecKeyRef key,
      SecKeyAlgorithm algorithm,
      CFDataRef data,
      CFErrorRef* error);
  // KeyCopyPublicKey wraps the |SecKeyCopyPublicKey| function.
  virtual base::apple::ScopedCFTypeRef<SecKeyRef> KeyCopyPublicKey(
      SecKeyRef key);

  // ItemCopyMatching wraps the |SecItemCopyMatching| function.
  virtual OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result);
  // ItemDelete wraps the |SecItemDelete| function.
  virtual OSStatus ItemDelete(CFDictionaryRef query);
  // ItemDelete wraps the |SecItemUpdate| function.
  virtual OSStatus ItemUpdate(
      CFDictionaryRef query,
      base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> keychain_data);

 protected:
  AppleKeychainV2();
  virtual ~AppleKeychainV2();

 protected:
  friend class base::NoDestructor<AppleKeychainV2>;
  friend class ScopedTouchIdTestEnvironment;

  // Set an override to the singleton instance returned by |GetInstance|. The
  // caller keeps ownership of the injected keychain and must remove the
  // override by calling |ClearInstanceOverride| before deleting it.
  static void SetInstanceOverride(AppleKeychainV2* keychain);
  static void ClearInstanceOverride();
};

}  // namespace crypto

#endif  // CRYPTO_APPLE_KEYCHAIN_V2_H_
