// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_KEYCHAIN_H_
#define DEVICE_FIDO_MAC_KEYCHAIN_H_

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>
#import <Security/Security.h>

#include "base/component_export.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/no_destructor.h"

namespace device {
namespace fido {
namespace mac {

// Keychain wraps some operations from the macOS Security framework to work with
// keys and keychain items.
//
// The Touch ID authenticator creates keychain items in the "iOS-style"
// keychain, which scopes item access based on the application-identifer or
// keychain-access-group entitlements, and therefore requires code signing with
// a real Apple developer ID. We therefore group these function here, so they
// can be mocked out in testing.
class COMPONENT_EXPORT(DEVICE_FIDO) Keychain {
 public:
  static Keychain& GetInstance();

  Keychain(const Keychain&) = delete;
  Keychain& operator=(const Keychain&) = delete;

  // KeyCreateRandomKey wraps the |SecKeyCreateRandomKey| function.
  virtual base::ScopedCFTypeRef<SecKeyRef> KeyCreateRandomKey(
      CFDictionaryRef params,
      CFErrorRef* error);
  // KeyCreateSignature wraps the |SecKeyCreateSignature| function.
  virtual base::ScopedCFTypeRef<CFDataRef> KeyCreateSignature(
      SecKeyRef key,
      SecKeyAlgorithm algorithm,
      CFDataRef data,
      CFErrorRef* error);
  // KeyCopyPublicKey wraps the |SecKeyCopyPublicKey| function.
  virtual base::ScopedCFTypeRef<SecKeyRef> KeyCopyPublicKey(SecKeyRef key);

  // ItemCopyMatching wraps the |SecItemCopyMatching| function.
  virtual OSStatus ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result);
  // ItemDelete wraps the |SecItemDelete| function.
  virtual OSStatus ItemDelete(CFDictionaryRef query);
  // ItemDelete wraps the |SecItemUpdate| function.
  virtual OSStatus ItemUpdate(
      CFDictionaryRef query,
      base::ScopedCFTypeRef<CFMutableDictionaryRef> keychain_data);

 protected:
  Keychain();
  virtual ~Keychain();

 protected:
  friend class base::NoDestructor<Keychain>;
  friend class ScopedTouchIdTestEnvironment;

  // Set an override to the singleton instance returned by |GetInstance|. The
  // caller keeps ownership of the injected keychain and must remove the
  // override by calling |ClearInstanceOverride| before deleting it.
  static void SetInstanceOverride(Keychain* keychain);
  static void ClearInstanceOverride();
};
}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_KEYCHAIN_H_
