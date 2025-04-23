// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/canvas_interventions/noise_hash.h"

#include <bit>
#include <cstdint>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace blink {

namespace {
// FNV constants
// https://datatracker.ietf.org/doc/html/draft-eastlake-fnv#name-fnv-constants
constexpr uint64_t kFnvPrime = 0x00000100000001b3;
constexpr uint64_t kFnvOffset = 0xcbf29ce484222325;
}  // namespace

NoiseHash::NoiseHash(const uint64_t token, const String& partition) {
  token_hash_ = kFnvOffset;
  auto hasher = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  hasher->Update(base::U64ToLittleEndian(token));
  hasher->Update(partition.RawByteSpan());
  std::array<uint8_t, crypto::kSHA256Length> digest;
  hasher->Finish(digest);
  Update(base::U64FromLittleEndian(base::span(digest).first<8>()));
}

void NoiseHash::Update(const uint64_t value) {
  token_hash_ ^= value;
  token_hash_ *= kFnvPrime;
  remaining_bits_ = 64;
}

int NoiseHash::GetValueBelow(const int max_value) {
  if (max_value <= 0) {
    return 0;
  }
  int required_bits = std::bit_width(base::checked_cast<uint64_t>(max_value));
  CHECK(remaining_bits_ >= required_bits);

  int value = (token_hash_ >> (64 - remaining_bits_)) % max_value;
  remaining_bits_ -= required_bits;
  return value;
}

uint64_t NoiseHash::GetTokenHashForTesting() const {
  return token_hash_;
}

}  // namespace blink
