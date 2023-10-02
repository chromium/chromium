// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/mock_decrypter.h"

#include <limits>

#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_bug_tracker.h"

using quic::DiversificationNonce;
using quic::Perspective;
using quic::QuicPacketNumber;

namespace net {

namespace {

const size_t kPaddingSize = 12;

}  // namespace

MockDecrypter::MockDecrypter(Perspective perspective) {}

bool MockDecrypter::SetKey(std::string_view key) {
  return key.empty();
}

bool MockDecrypter::SetNoncePrefix(std::string_view nonce_prefix) {
  return nonce_prefix.empty();
}

bool MockDecrypter::SetIV(std::string_view iv) {
  return iv.empty();
}

bool MockDecrypter::SetHeaderProtectionKey(std::string_view key) {
  return key.empty();
}

size_t MockDecrypter::GetKeySize() const {
  return 0;
}

size_t MockDecrypter::GetIVSize() const {
  return 0;
}

size_t MockDecrypter::GetNoncePrefixSize() const {
  return 0;
}

bool MockDecrypter::SetPreliminaryKey(std::string_view key) {
  LOG(DFATAL) << "Should not be called";
  return false;
}

bool MockDecrypter::SetDiversificationNonce(const DiversificationNonce& nonce) {
  LOG(DFATAL) << "Should not be called";
  return true;
}

bool MockDecrypter::DecryptPacket(uint64_t /*packet_number*/,
                                  std::string_view associated_data,
                                  std::string_view ciphertext,
                                  char* output,
                                  size_t* output_length,
                                  size_t max_output_length) {
  if (ciphertext.length() < kPaddingSize) {
    return false;
  }
  size_t plaintext_size = ciphertext.length() - kPaddingSize;
  if (plaintext_size > max_output_length) {
    return false;
  }

  memcpy(output, ciphertext.data(), plaintext_size);
  *output_length = plaintext_size;
  return true;
}

std::string MockDecrypter::GenerateHeaderProtectionMask(
    quic::QuicDataReader* sample_reader) {
  return std::string(5, 0);
}

uint32_t MockDecrypter::cipher_id() const {
  return 0;
}

quic::QuicPacketCount MockDecrypter::GetIntegrityLimit() const {
  return std::numeric_limits<quic::QuicPacketCount>::max();
}

std::string_view MockDecrypter::GetKey() const {
  return std::string_view();
}

std::string_view MockDecrypter::GetNoncePrefix() const {
  return std::string_view();
}

}  // namespace net
