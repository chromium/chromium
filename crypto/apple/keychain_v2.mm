// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/keychain_v2.h"

#import <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/no_destructor.h"

namespace crypto::apple {

static KeychainV2* g_keychain_instance_override = nullptr;

// static
KeychainV2& KeychainV2::GetInstance() {
  if (g_keychain_instance_override) {
    return *g_keychain_instance_override;
  }
  static base::NoDestructor<KeychainV2> k;
  return *k;
}

// static
void KeychainV2::SetInstanceOverride(KeychainV2* KeychainV2) {
  CHECK(!g_keychain_instance_override);
  g_keychain_instance_override = KeychainV2;
}

// static
void KeychainV2::ClearInstanceOverride() {
  CHECK(g_keychain_instance_override);
  g_keychain_instance_override = nullptr;
}

KeychainV2::KeychainV2() = default;
KeychainV2::~KeychainV2() = default;

NSArray* KeychainV2::GetTokenIDs() {
  return [[TKTokenWatcher alloc] init].tokenIDs;
}

base::apple::ScopedCFTypeRef<SecKeyRef> KeychainV2::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<SecKeyRef>(
      SecKeyCreateRandomKey(params, error));
}

base::apple::ScopedCFTypeRef<CFDataRef> KeychainV2::KeyCreateSignature(
    SecKeyRef key,
    SecKeyAlgorithm algorithm,
    CFDataRef data,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<CFDataRef>(
      SecKeyCreateSignature(key, algorithm, data, error));
}

base::apple::ScopedCFTypeRef<SecKeyRef> KeychainV2::KeyCopyPublicKey(
    SecKeyRef key) {
  return base::apple::ScopedCFTypeRef<SecKeyRef>(SecKeyCopyPublicKey(key));
}

base::apple::ScopedCFTypeRef<CFDataRef>
KeychainV2::KeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<CFDataRef>(
      SecKeyCopyExternalRepresentation(key, error));
}

base::apple::ScopedCFTypeRef<CFDictionaryRef> KeychainV2::KeyCopyAttributes(
    SecKeyRef key) {
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      SecKeyCopyAttributes(key));
}

OSStatus KeychainV2::ItemAdd(CFDictionaryRef attributes, CFTypeRef* result) {
  return SecItemAdd(attributes, result);
}

OSStatus KeychainV2::ItemCopyMatching(CFDictionaryRef query,
                                      CFTypeRef* result) {
  return SecItemCopyMatching(query, result);
}

OSStatus KeychainV2::ItemDelete(CFDictionaryRef query) {
  return SecItemDelete(query);
}

OSStatus KeychainV2::ItemUpdate(CFDictionaryRef query,
                                CFDictionaryRef keychain_data) {
  return SecItemUpdate(query, keychain_data);
}

#if !BUILDFLAG(IS_IOS)
base::apple::ScopedCFTypeRef<CFTypeRef> KeychainV2::TaskCopyValueForEntitlement(
    SecTaskRef task,
    CFStringRef entitlement,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<CFTypeRef>(
      SecTaskCopyValueForEntitlement(task, entitlement, error));
}
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_IOS_TVOS)
BOOL KeychainV2::LAContextCanEvaluatePolicy(LAPolicy policy, NSError** error) {
  LAContext* context = [[LAContext alloc] init];
  return [context canEvaluatePolicy:policy error:error];
}
#endif  // !BUILDFLAG(IS_IOS_TVOS)

}  // namespace crypto::apple
