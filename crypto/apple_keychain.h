// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_H_
#define CRYPTO_APPLE_KEYCHAIN_H_

#include <Security/Security.h>

#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

#if defined(OS_IOS)
using AppleSecKeychainItemRef = void*;
#else
using AppleSecKeychainItemRef = SecKeychainItemRef;
#endif

// Wraps the KeychainServices API in a very thin layer, to allow it to be
// mocked out for testing.

// See Keychain Services documentation for function documentation, as these call
// through directly to their Keychain Services equivalents (Foo ->
// SecKeychainFoo). The only exception is Free, which should be used for
// anything returned from this class that would normally be freed with
// CFRelease (to aid in testing).
class CRYPTO_EXPORT AppleKeychain {
 public:
  AppleKeychain();

  AppleKeychain(const AppleKeychain&) = delete;
  AppleKeychain& operator=(const AppleKeychain&) = delete;

  virtual ~AppleKeychain();

  virtual OSStatus FindGenericPassword(UInt32 serviceNameLength,
                                       const char* serviceName,
                                       UInt32 accountNameLength,
                                       const char* accountName,
                                       UInt32* passwordLength,
                                       void** passwordData,
                                       AppleSecKeychainItemRef* itemRef) const;

  virtual OSStatus ItemFreeContent(void* data) const;

  virtual OSStatus AddGenericPassword(UInt32 serviceNameLength,
                                      const char* serviceName,
                                      UInt32 accountNameLength,
                                      const char* accountName,
                                      UInt32 passwordLength,
                                      const void* passwordData,
                                      AppleSecKeychainItemRef* itemRef) const;

#if !defined(OS_IOS)
  virtual OSStatus ItemDelete(AppleSecKeychainItemRef itemRef) const;
#endif  // !defined(OS_IOS)
};

}  // namespace crypto

#endif  // CRYPTO_APPLE_KEYCHAIN_H_
