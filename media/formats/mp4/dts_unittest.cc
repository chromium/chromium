// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "media/base/mock_media_log.h"
#include "media/formats/mp4/dts.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

namespace mp4 {

class DTSTest : public testing::Test {
 public:
  DTSTest() = default;

  bool Parse(const std::vector<uint8_t>& data) {
    return dts_.Parse(data, &media_log_);
  }

  StrictMock<MockMediaLog> media_log_;
  DTS dts_;
};

TEST_F(DTSTest, NoInputTest) {
  std::vector<uint8_t> data;
  EXPECT_FALSE(Parse(data));
}

TEST_F(DTSTest, ShortInvalidInputTest) {
  std::vector<uint8_t> data({0x32, 0x44});

  EXPECT_FALSE(Parse(data));
}

TEST_F(DTSTest, NormalInputTest) {
  std::vector<uint8_t> data({0x00, 0x00, 0xbb, 0x80, 0x00, 0x0b, 0xb8, 0x00,
                             0x00, 0x0b, 0xb8, 0x00, 0x18, 0x03, 0x24, 0x40});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(dts_.GetDtsSamplingFrequency(), 48000u);
  EXPECT_EQ(dts_.GetMaxBitrate(), 768000u);
  EXPECT_EQ(dts_.GetAvgBitrate(), 768000u);
  EXPECT_EQ(dts_.GetPcmSampleDepth(), 24u);
  EXPECT_EQ(dts_.GetFrameDuration(), 512);
}

}  // namespace mp4

}  // namespace media
