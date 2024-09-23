// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A test for ZLib's checksum function.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/lib/crc32.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(Crc32Unittest, ByteTest) {
  struct {
    const char* data;
    int len;
    // Externally calculated at http://crc32-checksum.waraxe.us/
    int crc;
  } kData[] = {
      {"Hello", 5, static_cast<int>(0xF7D18982)},
      {"Google", 6, 0x62B0F067},
      {"", 0, 0x0},
      {"One more string.", 16, 0x0CA14970},
      {nullptr, 0, 0x0},
  };

  for (int i = 0; kData[i].data; i++)
    EXPECT_EQ(kData[i].crc,
        rlz_lib::Crc32(reinterpret_cast<const unsigned char*>(kData[i].data),
                       kData[i].len));
}

TEST(Crc32Unittest, CharTest) {
  struct {
    const char* data;
    // Externally calculated at http://crc32-checksum.waraxe.us/
    int crc;
  } kData[] = {
      {"Hello", static_cast<int>(0xF7D18982)},
      {"Google", 0x62B0F067},
      {"", 0x0},
      {"One more string.", 0x0CA14970},
      {"Google\r\n", static_cast<int>(0x83A3E860)},
      {nullptr, 0x0},
  };

  int crc;
  for (int i = 0; kData[i].data; i++) {
    EXPECT_TRUE(rlz_lib::Crc32(kData[i].data, &crc));
    EXPECT_EQ(kData[i].crc, crc);
  }
}
