// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_MOCK_DECRYPTER_H_
#define NET_QUIC_MOCK_DECRYPTER_H_

#include <cstddef>
#include <cstdint>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/third_party/quiche/src/common/platform/api/quiche_string_piece.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_decrypter.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"

namespace net {

// A MockDecrypter is a QuicDecrypter that strips the last 12 bytes of
// ciphertext (which should be zeroes, but are ignored), and returns the
// remaining ciphertext untouched and ignores the associated data. This is used
// to allow fuzzing to mutate plaintext packets.
class MockDecrypter : public quic::QuicDecrypter {
 public:
  explicit MockDecrypter(quic::Perspective perspective);
  ~MockDecrypter() override {}

  // QuicCrypter implementation
  bool SetKey(absl::string_view key) override;
  bool SetNoncePrefix(absl::string_view nonce_prefix) override;
  bool SetIV(absl::string_view iv) override;
  bool SetHeaderProtectionKey(absl::string_view key) override;
  size_t GetKeySize() const override;
  size_t GetIVSize() const override;
  size_t GetNoncePrefixSize() const override;

  // QuicDecrypter implementation
  bool SetPreliminaryKey(absl::string_view key) override;
  bool SetDiversificationNonce(
      const quic::DiversificationNonce& nonce) override;
  bool DecryptPacket(uint64_t packet_number,
                     absl::string_view associated_data,
                     absl::string_view ciphertext,
                     char* output,
                     size_t* output_length,
                     size_t max_output_length) override;
  std::string GenerateHeaderProtectionMask(
      quic::QuicDataReader* sample_reader) override;
  uint32_t cipher_id() const override;
  quic::QuicPacketCount GetIntegrityLimit() const override;
  absl::string_view GetKey() const override;
  absl::string_view GetNoncePrefix() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDecrypter);
};

}  // namespace net

#endif  // NET_QUIC_MOCK_DECRYPTER_H_
