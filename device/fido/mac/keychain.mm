// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/keychain.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/no_destructor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace device::fido::mac {

static Keychain* g_keychain_instance_override = nullptr;

// static
Keychain& Keychain::GetInstance() {
  if (g_keychain_instance_override) {
    return *g_keychain_instance_override;
  }
  static base::NoDestructor<Keychain> k;
  return *k;
}

// static
void Keychain::SetInstanceOverride(Keychain* keychain) {
  CHECK(!g_keychain_instance_override);
  g_keychain_instance_override = keychain;
}

// static
void Keychain::ClearInstanceOverride() {
  CHECK(g_keychain_instance_override);
  g_keychain_instance_override = nullptr;
}

Keychain::Keychain() = default;
Keychain::~Keychain() = default;

base::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  return base::ScopedCFTypeRef<SecKeyRef>(SecKeyCreateRandomKey(params, error));
}

base::ScopedCFTypeRef<CFDataRef> Keychain::KeyCreateSignature(
    SecKeyRef key,
    SecKeyAlgorithm algorithm,
    CFDataRef data,
    CFErrorRef* error) {
  return base::ScopedCFTypeRef<CFDataRef>(
      SecKeyCreateSignature(key, algorithm, data, error));
}

base::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCopyPublicKey(SecKeyRef key) {
  return base::ScopedCFTypeRef<SecKeyRef>(SecKeyCopyPublicKey(key));
}

OSStatus Keychain::ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) {
  return SecItemCopyMatching(query, result);
}

OSStatus Keychain::ItemDelete(CFDictionaryRef query) {
  return SecItemDelete(query);
}

OSStatus Keychain::ItemUpdate(
    CFDictionaryRef query,
    base::ScopedCFTypeRef<CFMutableDictionaryRef> keychain_data) {
  return SecItemUpdate(query, keychain_data);
}

}  // namespace device::fido::mac
