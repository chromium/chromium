// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "media/base/channel_layout.h"
#include "media/base/mock_media_log.h"
#include "media/formats/mp4/ac3.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

namespace mp4 {

class AC3Test : public testing::Test {
 public:
  AC3Test() = default;

  bool Parse(const std::vector<uint8_t>& data) {
    return ac3_.Parse(data, &media_log_);
  }

  StrictMock<MockMediaLog> media_log_;
  AC3 ac3_;
};

TEST_F(AC3Test, NoInputTest) {
  std::vector<uint8_t> data;
  EXPECT_FALSE(Parse(data));
}

TEST_F(AC3Test, ShortInvalidInputTest) {
  std::vector<uint8_t> data({0x50, 0x11});

  EXPECT_FALSE(Parse(data));
}

TEST_F(AC3Test, NormalInputTest) {
  std::vector<uint8_t> data({0x50, 0x11, 0x40});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 2u);
}

TEST_F(AC3Test, ChannelLayout_Mono_Test) {
  std::vector<uint8_t> data({0x10, 0x08, 0xc0});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 1u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_MONO);
}

TEST_F(AC3Test, ChannelLayout_Stereo_Test) {
  std::vector<uint8_t> data({0x10, 0x11, 0x40});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 2u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_STEREO);
}

TEST_F(AC3Test, ChannelLayout_Surround_Test) {
  std::vector<uint8_t> data({0x10, 0x19, 0xa0});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 3u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_SURROUND);
}

TEST_F(AC3Test, ChannelLayout_2Point1_Test) {
  std::vector<uint8_t> data({0x10, 0x15, 0x40});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 3u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_2POINT1);
}

TEST_F(AC3Test, ChannelLayout_2_2_Test) {
  std::vector<uint8_t> data({0x10, 0x31, 0xc0});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 4u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_2_2);
}

TEST_F(AC3Test, ChannelLayout_4_0_Test) {
  std::vector<uint8_t> data({0x10, 0x29, 0xc0});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 4u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_4_0);
}

TEST_F(AC3Test, ChannelLayout_5_0_Test) {
  std::vector<uint8_t> data({0x10, 0x39, 0xe0});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 5u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_5_0);
}

TEST_F(AC3Test, ChannelLayout_5_1_Test) {
  std::vector<uint8_t> data({0x10, 0x3d, 0xc0});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(ac3_.GetChannelCount(), 6u);
  EXPECT_EQ(ac3_.GetChannelLayout(), CHANNEL_LAYOUT_5_1);
}

}  // namespace mp4

}  // namespace media
