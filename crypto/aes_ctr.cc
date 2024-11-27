// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aes_ctr.h"

#include <vector>

#include "base/containers/span.h"
#include "third_party/boringssl/src/include/openssl/aes.h"

namespace crypto::aes_ctr {

namespace {

void DoCTR(base::span<const uint8_t> key,
           base::span<const uint8_t, kCounterSize> counter,
           base::span<const uint8_t> in,
           base::span<uint8_t> out) {
  AES_KEY aes_key;
  CHECK_EQ(AES_set_encrypt_key(key.data(), key.size() * 8, &aes_key), 0);

  std::array<uint8_t, kCounterSize> ignored_counter_copy;
  std::array<uint8_t, AES_BLOCK_SIZE> ignored_out_ctr;
  unsigned int ignored_offset = 0;

  base::span(ignored_counter_copy).copy_from(counter);

  AES_ctr128_encrypt(in.data(), out.data(), in.size(), &aes_key,
                     ignored_counter_copy.data(), ignored_out_ctr.data(),
                     &ignored_offset);
}

}  // namespace

void Encrypt(base::span<const uint8_t> key,
             base::span<const uint8_t, kCounterSize> counter,
             base::span<const uint8_t> in,
             base::span<uint8_t> out) {
  DoCTR(key, counter, in, out);
}

void Decrypt(base::span<const uint8_t> key,
             base::span<const uint8_t, kCounterSize> counter,
             base::span<const uint8_t> in,
             base::span<uint8_t> out) {
  DoCTR(key, counter, in, out);
}

std::vector<uint8_t> Encrypt(base::span<const uint8_t> key,
                             base::span<const uint8_t, kCounterSize> counter,
                             base::span<const uint8_t> in) {
  std::vector<uint8_t> out(in.size());
  Encrypt(key, counter, in, out);
  return out;
}

std::vector<uint8_t> Decrypt(base::span<const uint8_t> key,
                             base::span<const uint8_t, kCounterSize> counter,
                             base::span<const uint8_t> in) {
  std::vector<uint8_t> out(in.size());
  Decrypt(key, counter, in, out);
  return out;
}

}  // namespace crypto::aes_ctr
