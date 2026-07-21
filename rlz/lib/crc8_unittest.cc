// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Uniitest for data encryption functions.

#include "rlz/lib/crc8.h"

#include <stddef.h>

#include <string_view>

#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(Crc8Unittest, TestCrc8) {
  struct Data {
    char string[10];
    // Externally calculated checksums use
    // http://www.zorc.breitbandkatze.de/crc.html
    // with the ATM HEC paramters:
    // CRC-8, Polynomial 0x07, Initial value 0x00, Final XOR value 0x55
    // (direct, don't reverse data byes, don't reverse CRC before final XOR)
    unsigned char external_crc;
    int random_byte;
    unsigned char corrupt_value;
  } data[] = {
    {"Google",    0x01, 2, 0x53},
    {"GOOGLE",    0xA6, 4, 0x11},
    {"My CRC 8!", 0xDC, 0, 0x50},
  };

  uint8_t crc;
  bool matches;
  for (auto& item : data) {
    std::string_view str_view(item.string);
    auto bytes = base::as_byte_span(str_view);
    crc = 0;
    matches = false;

    // Calculate CRC and compare against external value.
    rlz_lib::Crc8::Generate(bytes, &crc);
    EXPECT_EQ(crc, item.external_crc);
    rlz_lib::Crc8::Verify(bytes, crc, &matches);
    EXPECT_TRUE(matches);

    // Corrupt string and see if CRC still matches.
    auto chars = base::span(item.string);
    chars[item.random_byte] = item.corrupt_value;
    rlz_lib::Crc8::Verify(bytes, crc, &matches);
    EXPECT_FALSE(matches);
  }
}

TEST(Crc8Unittest, TestEmptySpan) {
  uint8_t crc = 0;
  bool matches = false;

  // Generating CRC for empty span should fail.
  EXPECT_FALSE(rlz_lib::Crc8::Generate(base::span<const uint8_t>(), &crc));

  // Verifying CRC for empty span should fail.
  EXPECT_FALSE(
      rlz_lib::Crc8::Verify(base::span<const uint8_t>(), 0x55, &matches));
}
