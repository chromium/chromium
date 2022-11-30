// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_KEYCHAIN_TEST_UTIL_MAC_H_
#define NET_TEST_KEYCHAIN_TEST_UTIL_MAC_H_

#include <Security/SecKeychain.h>

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/mac/scoped_cftyperef.h"

namespace net {

class X509Certificate;

// Manages a temporary keychain.
class ScopedTestKeychain {
 public:
  ScopedTestKeychain();
  ~ScopedTestKeychain();

  // Initializes the temp dir and keychain, returning true on success.
  bool Initialize();

  // Returns the SecKeychainRef. Initialize() must have been called first.
  SecKeychainRef keychain() const { return keychain_.get(); }

 private:
  base::ScopedTempDir keychain_dir_;
  base::ScopedCFTypeRef<SecKeychainRef> keychain_;
};

// Import the |cert| and matching key in unencrypted |pkcs8| into |keychain|
// and return the SecIdentityRef for |cert| and its key.
base::ScopedCFTypeRef<SecIdentityRef> ImportCertAndKeyToKeychain(
    const X509Certificate* cert,
    const std::string pkcs8,
    SecKeychainRef keychain);

}  // namespace net

#endif  // NET_TEST_KEYCHAIN_TEST_UTIL_MAC_H_
