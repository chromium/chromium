// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SYMMETRIC_KEY_H_
#define CRYPTO_SYMMETRIC_KEY_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

// A SymmetricKey is an array of bytes which is used for symmetric cryptography
// (encryption only).
//
// This whole type is deprecated: prefer to use raw std::array<uint8_t>,
// std::vector<uint8_t>, or base::span<uint8_t> instead. This type has no
// behavior or particular meaning.
//
// TODO(https://issues.chromium.org/issues/370724578): get rid of this.
class CRYPTO_EXPORT SymmetricKey {
 public:
  // Defines the algorithm that a key will be used with. See also
  // class Encryptor.
  enum Algorithm {
    AES,
  };

  SymmetricKey() = delete;

  // Wrap the given span of bytes as a SymmetricKey.
  explicit SymmetricKey(base::span<const uint8_t> key_bytes);
  virtual ~SymmetricKey();

  SymmetricKey(const SymmetricKey&);
  SymmetricKey& operator=(const SymmetricKey&);

  // Generates a random key suitable to be used with |algorithm| and of
  // |key_size_in_bits| bits. |key_size_in_bits| must be a multiple of 8.
  //
  // Deprecated: use the value version below that does not take an algorithm.
  static std::unique_ptr<SymmetricKey> GenerateRandomKey(
      Algorithm algorithm,
      size_t key_size_in_bits);

  static SymmetricKey RandomKey(size_t key_size_in_bits);

  // Imports an array of key bytes in |raw_key|. The raw key must be of a valid
  // size - see IsValidKeySize() in the source for details, although in general
  // you should not need to choose key sizes yourself. Returns nullptr if the
  // key is not of valid size.
  //
  // Deprecated: use the regular constructor that accepts a span of bytes, and
  // validate that the key is of whatever length your client code expects before
  // doing so.
  static std::unique_ptr<SymmetricKey> Import(Algorithm algorithm,
                                              const std::string& raw_key);

  // Returns the internal key storage.
  const std::string& key() const { return key_; }

 private:
  std::string key_;
};

}  // namespace crypto

#endif  // CRYPTO_SYMMETRIC_KEY_H_
