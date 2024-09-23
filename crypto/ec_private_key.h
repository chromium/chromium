// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_EC_PRIVATE_KEY_H_
#define CRYPTO_EC_PRIVATE_KEY_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto {

// Encapsulates an elliptic curve (EC) private key. Can be used to generate new
// keys, export keys to other formats, or to extract a public key.
// TODO(mattm): make this and RSAPrivateKey implement some PrivateKey interface.
// (The difference in types of key() and public_key() make this a little
// tricky.)
class CRYPTO_EXPORT ECPrivateKey {
 public:
  ECPrivateKey(const ECPrivateKey&) = delete;
  ECPrivateKey& operator=(const ECPrivateKey&) = delete;

  ~ECPrivateKey();

  // Creates a new random instance. Can return nullptr if initialization fails.
  // The created key will use the NIST P-256 curve.
  static std::unique_ptr<ECPrivateKey> Create();

  // Create a new instance by importing an existing private key. The format is
  // an ASN.1-encoded PrivateKeyInfo block from PKCS #8. This can return
  // nullptr if initialization fails.
  static std::unique_ptr<ECPrivateKey> CreateFromPrivateKeyInfo(
      base::span<const uint8_t> input);

  // Creates a new instance by importing an existing key pair.
  // The key pair is given as an ASN.1-encoded PKCS #8 EncryptedPrivateKeyInfo
  // block with empty password and an X.509 SubjectPublicKeyInfo block.
  // Returns nullptr if initialization fails.
  //
  // This function is deprecated. Use CreateFromPrivateKeyInfo for new code.
  // See https://crbug.com/603319.
  static std::unique_ptr<ECPrivateKey> CreateFromEncryptedPrivateKeyInfo(
      base::span<const uint8_t> encrypted_private_key_info);

  // Returns a copy of the object.
  std::unique_ptr<ECPrivateKey> Copy() const;

  EVP_PKEY* key() const { return key_.get(); }

  // Exports the private key to a PKCS #8 PrivateKeyInfo block.
  bool ExportPrivateKey(std::vector<uint8_t>* output) const;

  // Exports the public key to an X.509 SubjectPublicKeyInfo block.
  bool ExportPublicKey(std::vector<uint8_t>* output) const;

  // Exports the public key as an EC point in X9.62 uncompressed form. Note this
  // includes the leading 0x04 byte.
  bool ExportRawPublicKey(std::string* output) const;

 private:
  // Constructor is private. Use one of the Create*() methods above instead.
  ECPrivateKey();

  bssl::UniquePtr<EVP_PKEY> key_;
};

}  // namespace crypto

#endif  // CRYPTO_EC_PRIVATE_KEY_H_
