// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Uniitest for data encryption functions.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "rlz/lib/crc8.h"

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

  unsigned char* bytes;
  unsigned char crc;
  bool matches;
  int length;
  for (size_t i = 0; i < sizeof(data) / sizeof(data[0]); ++i) {
    bytes = reinterpret_cast<unsigned char*>(data[i].string);
    crc = 0;
    matches = false;
    length = strlen(data[i].string);

    // Calculate CRC and compare against external value.
    rlz_lib::Crc8::Generate(bytes, length, &crc);
    EXPECT_TRUE(crc == data[i].external_crc);
    rlz_lib::Crc8::Verify(bytes, length, crc, &matches);
    EXPECT_TRUE(matches);

    // Corrupt string and see if CRC still matches.
    data[i].string[data[i].random_byte] = data[i].corrupt_value;
    rlz_lib::Crc8::Verify(bytes, length, crc, &matches);
    EXPECT_FALSE(matches);
  }
}
