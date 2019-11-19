// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_ENCRYPTER_H_
#define NET_QUIC_MOCK_ENCRYPTER_H_

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_encrypter.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace net {

// A MockEncrypter is a QuicEncrypter that returns this plaintext followed by 12
// bytes of zeroes. No encryption or MAC is applied. This is used to allow
// fuzzing to mutate plaintext packets.
class MockEncrypter : public quic::QuicEncrypter {
 public:
  explicit MockEncrypter(quic::Perspective perspective);
  ~MockEncrypter() override {}

  // QuicEncrypter implementation
  bool SetKey(quic::QuicStringPiece key) override;
  bool SetNoncePrefix(quic::QuicStringPiece nonce_prefix) override;
  bool SetHeaderProtectionKey(quic::QuicStringPiece key) override;
  bool SetIV(quic::QuicStringPiece iv) override;
  bool EncryptPacket(uint64_t packet_number,
                     quic::QuicStringPiece associated_data,
                     quic::QuicStringPiece plaintext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override;
  std::string GenerateHeaderProtectionMask(
      quic::QuicStringPiece sample) override;
  size_t GetKeySize() const override;
  size_t GetNoncePrefixSize() const override;
  size_t GetIVSize() const override;
  size_t GetMaxPlaintextSize(size_t ciphertext_size) const override;
  size_t GetCiphertextSize(size_t plaintext_size) const override;
  quic::QuicStringPiece GetKey() const override;
  quic::QuicStringPiece GetNoncePrefix() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEncrypter);
};

}  // namespace net

#endif  // NET_QUIC_MOCK_ENCRYPTER_H_
