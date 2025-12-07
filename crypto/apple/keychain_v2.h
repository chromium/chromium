// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_V2_H_
#define CRYPTO_APPLE_KEYCHAIN_V2_H_

#import <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/no_destructor.h"
#include "crypto/crypto_export.h"

#if !BUILDFLAG(IS_IOS_TVOS)
#import <LocalAuthentication/LocalAuthentication.h>
#endif

namespace crypto::apple {

// KeychainV2 wraps iOS-style operations from the macOS Security framework
// to work with keys and keychain items. These functions are grouped here so
// they can be mocked out in testing.
//
// TODO(https://crbug.com/441317288): Unify with Keychain.
class CRYPTO_EXPORT KeychainV2 {
 public:
  static KeychainV2& GetInstance();

  KeychainV2(const KeychainV2&) = delete;
  KeychainV2& operator=(const KeychainV2&) = delete;

  // Wraps the |TKTokenWatcher.tokenIDs| property.
  virtual NSArray* GetTokenIDs();

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
  // KeyCopyExternalRepresentation wraps the |SecKeyCopyExternalRepresentation|
  // function.
  virtual base::apple::ScopedCFTypeRef<CFDataRef> KeyCopyExternalRepresentation(
      SecKeyRef key,
      CFErrorRef* error);
  // KeyCopyAttributes wraps the |SecKeyCopyAttributes| function.
  virtual base::apple::ScopedCFTypeRef<CFDictionaryRef> KeyCopyAttributes(
      SecKeyRef key);

  // ItemAdd wraps the |SecItemAdd| function.
  virtual OSStatus ItemAdd(CFDictionaryRef attributes, CFTypeRef* result);
  // ItemCopyMatching wraps the |SecItemCopyMatching| function.
  virtual OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result);
  // ItemDelete wraps the |SecItemDelete| function.
  virtual OSStatus ItemDelete(CFDictionaryRef query);
  // ItemDelete wraps the |SecItemUpdate| function.
  virtual OSStatus ItemUpdate(CFDictionaryRef query,
                              CFDictionaryRef keychain_data);

#if !BUILDFLAG(IS_IOS)
  // TaskCopyValueForEntitlement wraps the |SecTaskCopyValueForEntitlement|
  // function. Not available on iOS.
  virtual base::apple::ScopedCFTypeRef<CFTypeRef> TaskCopyValueForEntitlement(
      SecTaskRef task,
      CFStringRef entitlement,
      CFErrorRef* error);
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_IOS_TVOS)
  // LAContextCanEvaluatePolicy wraps LAContext's canEvaluatePolicy method.
  virtual BOOL LAContextCanEvaluatePolicy(LAPolicy policy, NSError** error);
#endif  // !BUILDFLAG(IS_IOS_TVOS)

 protected:
  KeychainV2();
  virtual ~KeychainV2();

 protected:
  friend class base::NoDestructor<KeychainV2>;
  friend class ScopedTouchIdTestEnvironment;
  friend class ScopedFakeKeychainV2;

  // Set an override to the singleton instance returned by |GetInstance|. The
  // caller keeps ownership of the injected keychain and must remove the
  // override by calling |ClearInstanceOverride| before deleting it.
  static void SetInstanceOverride(KeychainV2* keychain);
  static void ClearInstanceOverride();
};

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_KEYCHAIN_V2_H_
