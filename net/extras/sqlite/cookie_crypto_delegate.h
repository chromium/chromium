// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SQLITE_COOKIE_CRYPTO_DELEGATE_H_
#define NET_EXTRAS_SQLITE_COOKIE_CRYPTO_DELEGATE_H_

#include <string>

#include "base/component_export.h"

namespace net {

// Implements encryption and decryption for the persistent cookie store.
class COMPONENT_EXPORT(NET_EXTRAS) CookieCryptoDelegate {
 public:
  virtual ~CookieCryptoDelegate() = default;

  // Encrypt `plaintext` string and store the result in `ciphertext`. Returns
  // true if the encryption succeeded.
  virtual bool EncryptString(const std::string& plaintext,
                             std::string* ciphertext) = 0;

  // Decrypt `ciphertext` string and store the result in `plaintext`. Returns
  // true if the decryption succeeded.
  virtual bool DecryptString(const std::string& ciphertext,
                             std::string* plaintext) = 0;
};

}  // namespace net

#endif  // NET_EXTRAS_SQLITE_COOKIE_CRYPTO_DELEGATE_H_
