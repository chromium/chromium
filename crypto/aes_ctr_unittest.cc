// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/aes_ctr.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(AesCtrTests, KnownAnswers) {
  // To reproduce this test case, because openssl doesn't have a builtin
  // aes-128-ctr mode:
  //   echo [counter hex] | xxd -r -p | openssl aes-128-ecb -K [key hex]
  // That emits a block of key stream, which you can xor with plaintext to
  // produce a sample ciphertext.
  // clang-format off
  constexpr auto kKey = std::to_array<uint8_t>({
    0xff, 0x00, 0xee, 0x11, 0xdd, 0x22, 0xcc, 0x33,
    0xbb, 0x44, 0xaa, 0x55, 0x99, 0x66, 0x88, 0x77,
  });
  constexpr auto kCounter = std::to_array<uint8_t>({
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
  });
  constexpr auto kPlaintext = std::to_array<uint8_t>({
    0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
  });
  constexpr auto kExpectedCiphertext = std::to_array<uint8_t>({
    0xd7, 0x07, 0x48, 0x4f, 0x58,
  });
  // clang-format on
  std::array<uint8_t, std::size(kPlaintext)> ciphertext;
  std::array<uint8_t, std::size(kPlaintext)> plaintext;

  crypto::aes_ctr::Encrypt(kKey, kCounter, kPlaintext, ciphertext);
  crypto::aes_ctr::Decrypt(kKey, kCounter, ciphertext, plaintext);

  EXPECT_EQ(kPlaintext, plaintext);
  EXPECT_EQ(kExpectedCiphertext, ciphertext);
}
