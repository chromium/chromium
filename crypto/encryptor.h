// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_ENCRYPTOR_H_
#define CRYPTO_ENCRYPTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

class SymmetricKey;

// This class implements encryption without authentication, which is usually
// unsafe. Prefer crypto::Aead for new code. If using this class, prefer the
// base::span and std::vector overloads over the std::string_view and
// std::string overloads.
class CRYPTO_EXPORT Encryptor {
 public:
  enum Mode {
    CBC,
    CTR,
  };

  Encryptor();
  ~Encryptor();

  // Initializes the encryptor using |key| and |iv|. Returns false if either the
  // key or the initialization vector cannot be used.
  //
  // If |mode| is CBC, |iv| must not be empty; if it is CTR, then |iv| must be
  // empty.
  bool Init(const SymmetricKey* key, Mode mode, std::string_view iv);
  bool Init(const SymmetricKey* key, Mode mode, base::span<const uint8_t> iv);

  // Encrypts |plaintext| into |ciphertext|.  |plaintext| may only be empty if
  // the mode is CBC.
  bool Encrypt(std::string_view plaintext, std::string* ciphertext);
  bool Encrypt(base::span<const uint8_t> plaintext,
               std::vector<uint8_t>* ciphertext);

  // Decrypts |ciphertext| into |plaintext|.  |ciphertext| must not be empty.
  //
  // WARNING: In CBC mode, Decrypt() returns false if it detects the padding
  // in the decrypted plaintext is wrong. Padding errors can result from
  // tampered ciphertext or a wrong decryption key. But successful decryption
  // does not imply the authenticity of the data. The caller of Decrypt()
  // must either authenticate the ciphertext before decrypting it, or take
  // care to not report decryption failure. Otherwise it could inadvertently
  // be used as a padding oracle to attack the cryptosystem.
  bool Decrypt(std::string_view ciphertext, std::string* plaintext);
  bool Decrypt(base::span<const uint8_t> ciphertext,
               std::vector<uint8_t>* plaintext);

  // Sets the counter value when in CTR mode. Currently only 128-bits
  // counter value is supported.
  //
  // Returns true only if update was successful.
  bool SetCounter(std::string_view counter);
  bool SetCounter(base::span<const uint8_t> counter);

  // TODO(albertb): Support streaming encryption.

 private:
  raw_ptr<const SymmetricKey, DanglingUntriaged> key_;
  Mode mode_;

  bool CryptString(bool do_encrypt,
                   std::string_view input,
                   std::string* output);
  bool CryptBytes(bool do_encrypt,
                  base::span<const uint8_t> input,
                  std::vector<uint8_t>* output);

  // On success, these helper functions return the number of bytes written to
  // |output|.
  size_t MaxOutput(bool do_encrypt, size_t length);
  std::optional<size_t> Crypt(bool do_encrypt,
                              base::span<const uint8_t> input,
                              base::span<uint8_t> output);
  std::optional<size_t> CryptCTR(bool do_encrypt,
                                 base::span<const uint8_t> input,
                                 base::span<uint8_t> output);

  // In CBC mode, the IV passed to Init(). In CTR mode, the counter value passed
  // to SetCounter().
  std::vector<uint8_t> iv_;
};

}  // namespace crypto

#endif  // CRYPTO_ENCRYPTOR_H_
