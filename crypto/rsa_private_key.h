// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This interface is deprecated and being removed: https://crbug.com/425863216.
// New users should use crypto/keypair instead.

#ifndef CRYPTO_RSA_PRIVATE_KEY_H_
#define CRYPTO_RSA_PRIVATE_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto {

// Encapsulates an RSA private key. Can be used to export keys to other formats
// or to extract a public key.
// TODO(https://crbug.com/425863216): Delete this.
class CRYPTO_EXPORT RSAPrivateKey {
 public:
  RSAPrivateKey(const RSAPrivateKey&) = delete;
  RSAPrivateKey& operator=(const RSAPrivateKey&) = delete;

  ~RSAPrivateKey();

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return NULL if
  // initialization fails.
  static std::unique_ptr<RSAPrivateKey> CreateFromPrivateKeyInfo(
      base::span<const uint8_t> input);

  EVP_PKEY* key() const { return key_.get(); }

  // Creates a copy of the object.
  std::unique_ptr<RSAPrivateKey> Copy() const;

  // Exports the private key to a PKCS #8 PrivateKeyInfo block.
  bool ExportPrivateKey(std::vector<uint8_t>* output) const;

  // Exports the public key to an X509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8_t>* output) const;

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  RSAPrivateKey();

  bssl::UniquePtr<EVP_PKEY> key_;
};

}  // namespace crypto

#endif  // CRYPTO_RSA_PRIVATE_KEY_H_
