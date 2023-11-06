// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/keychain.h"

#import <Foundation/Foundation.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/no_destructor.h"

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

base::apple::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<SecKeyRef>(
      SecKeyCreateRandomKey(params, error));
}

base::apple::ScopedCFTypeRef<CFDataRef> Keychain::KeyCreateSignature(
    SecKeyRef key,
    SecKeyAlgorithm algorithm,
    CFDataRef data,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<CFDataRef>(
      SecKeyCreateSignature(key, algorithm, data, error));
}

base::apple::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCopyPublicKey(
    SecKeyRef key) {
  return base::apple::ScopedCFTypeRef<SecKeyRef>(SecKeyCopyPublicKey(key));
}

OSStatus Keychain::ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) {
  return SecItemCopyMatching(query, result);
}

OSStatus Keychain::ItemDelete(CFDictionaryRef query) {
  return SecItemDelete(query);
}

OSStatus Keychain::ItemUpdate(
    CFDictionaryRef query,
    base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> keychain_data) {
  return SecItemUpdate(query, keychain_data.get());
}

}  // namespace device::fido::mac
