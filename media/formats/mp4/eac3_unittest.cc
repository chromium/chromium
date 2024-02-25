// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "media/base/mock_media_log.h"
#include "media/formats/mp4/eac3.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

namespace mp4 {

class EAC3Test : public testing::Test {
 public:
  EAC3Test() = default;

  bool Parse(const std::vector<uint8_t>& data) {
    return eac3_.Parse(data, &media_log_);
  }

  StrictMock<MockMediaLog> media_log_;
  EAC3 eac3_;
};

TEST_F(EAC3Test, NoInputTest) {
  std::vector<uint8_t> data;
  EXPECT_FALSE(Parse(data));
}

TEST_F(EAC3Test, ShortInvalidInputTest) {
  std::vector<uint8_t> data({0x06, 0xC8});

  EXPECT_FALSE(Parse(data));
}

TEST_F(EAC3Test, NormalInputTest) {
  std::vector<uint8_t> data({0x06, 0xC8, 0x60, 0x04, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 2u);
}

TEST_F(EAC3Test, ChannelLayout_Mono_Test) {
  std::vector<uint8_t> data({0x03, 0x00, 0x20, 0x02, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 1u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_MONO);
}

TEST_F(EAC3Test, ChannelLayout_Stereo_Test) {
  std::vector<uint8_t> data({0x06, 0x00, 0x20, 0x04, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 2u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_STEREO);
}

TEST_F(EAC3Test, ChannelLayout_Surround_Test) {
  std::vector<uint8_t> data({0x0a, 0x00, 0x20, 0x06, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 3u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_SURROUND);
}

TEST_F(EAC3Test, ChannelLayout_2Point1_Test) {
  std::vector<uint8_t> data({0x06, 0x00, 0x20, 0x05, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 3u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_2POINT1);
}

TEST_F(EAC3Test, ChannelLayout_2_2_Test) {
  std::vector<uint8_t> data({0x0c, 0x00, 0x20, 0x0c, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 4u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_2_2);
}

TEST_F(EAC3Test, ChannelLayout_4_0_Test) {
  std::vector<uint8_t> data({0x0c, 0x00, 0x20, 0x0a, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 4u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_4_0);
}

TEST_F(EAC3Test, ChannelLayout_5_0_Test) {
  std::vector<uint8_t> data({0x0e, 0x00, 0x20, 0x0e, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 5u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_5_0);
}

TEST_F(EAC3Test, ChannelLayout_5_1_Test) {
  std::vector<uint8_t> data({0x0e, 0x00, 0x20, 0x0f, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 6u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_5_1);
}

TEST_F(EAC3Test, ChannelLayout_7_1_Test) {
  std::vector<uint8_t> data({0x08, 0xa8, 0x20, 0x0f, 0x02, 0x02});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(eac3_.GetChannelCount(), 8u);
  EXPECT_EQ(eac3_.GetChannelLayout(), CHANNEL_LAYOUT_7_1);
}

}  // namespace mp4

}  // namespace media
