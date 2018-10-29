// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_CRYPTO_AEAD_BASE_ENCRYPTER_H_
#define NET_THIRD_PARTY_QUIC_CORE_CRYPTO_AEAD_BASE_ENCRYPTER_H_

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "third_party/boringssl/src/include/openssl/aead.h"

namespace quic {

// AeadBaseEncrypter is the base class of AEAD QuicEncrypter subclasses.
class QUIC_EXPORT_PRIVATE AeadBaseEncrypter : public QuicEncrypter {
 public:
  // This takes the function pointer rather than the EVP_AEAD itself so
  // subclasses do not need to call CRYPTO_library_init.
  AeadBaseEncrypter(const EVP_AEAD* (*aead_getter)(),
                    size_t key_size,
                    size_t auth_tag_size,
                    size_t nonce_size,
                    bool use_ietf_nonce_construction);
  AeadBaseEncrypter(const AeadBaseEncrypter&) = delete;
  AeadBaseEncrypter& operator=(const AeadBaseEncrypter&) = delete;
  ~AeadBaseEncrypter() override;

  // QuicEncrypter implementation
  bool SetKey(QuicStringPiece key) override;
  bool SetNoncePrefix(QuicStringPiece nonce_prefix) override;
  bool SetIV(QuicStringPiece iv) override;
  bool EncryptPacket(QuicTransportVersion version,
                     QuicPacketNumber packet_number,
                     QuicStringPiece associated_data,
                     QuicStringPiece plaintext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override;
  size_t GetKeySize() const override;
  size_t GetNoncePrefixSize() const override;
  size_t GetIVSize() const override;
  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override;
  size_t GetCiphertextSize(size_t plaintext_size) const override;
  QuicStringPiece GetKey() const override;
  QuicStringPiece GetNoncePrefix() const override;

  // Necessary so unit tests can explicitly specify a nonce, instead of an IV
  // (or nonce prefix) and packet number.
  bool Encrypt(QuicStringPiece nonce,
               QuicStringPiece associated_data,
               QuicStringPiece plaintext,
               unsigned char* output);

 protected:
  // Make these constants available to the subclasses so that the subclasses
  // can assert at compile time their key_size_ and nonce_size_ do not
  // exceed the maximum.
  static const size_t kMaxKeySize = 32;
  enum : size_t { kMaxNonceSize = 12 };

 private:
  const EVP_AEAD* const aead_alg_;
  const size_t key_size_;
  const size_t auth_tag_size_;
  const size_t nonce_size_;
  const bool use_ietf_nonce_construction_;

  // The key.
  unsigned char key_[kMaxKeySize];
  // The IV used to construct the nonce.
  unsigned char iv_[kMaxNonceSize];

  bssl::ScopedEVP_AEAD_CTX ctx_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_CRYPTO_AEAD_BASE_ENCRYPTER_H_
