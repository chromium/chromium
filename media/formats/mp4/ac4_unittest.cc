// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "media/base/mock_media_log.h"
#include "media/formats/mp4/ac4.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

namespace media {

namespace mp4 {

class AC4Test : public testing::Test {
 public:
  AC4Test() = default;

  bool Parse(const std::vector<uint8_t>& data) {
    return ac4_.Parse(data, &media_log_);
  }

  StrictMock<MockMediaLog> media_log_;
  AC4 ac4_;
};

TEST_F(AC4Test, NoInputTest) {
  constexpr uint8_t kTestData[]{};

  EXPECT_FALSE(Parse({kTestData, kTestData}));
}

TEST_F(AC4Test, ShortInvalidInputTest) {
  constexpr uint8_t kTestData[]{0x06, 0xC8};

  EXPECT_FALSE(
      Parse({kTestData, kTestData + std::size(kTestData) / sizeof(uint8_t)}));
}

TEST_F(AC4Test, ChannelBasedCodingInputTest) {
  // Format              : AC-4
  // Bitstream version   : 2
  // Presentation version: 1
  // Presentation level  : 1
  // Channel mode        : 5.1
  // Channel layout      : L R C LFE Ls Rs
  constexpr uint8_t kTestData[]{0x20, 0x9a, 0x01, 0x60, 0x00, 0x00, 0x00,
                                0x1f, 0xff, 0xff, 0xff, 0xe0, 0x01, 0x0e,
                                0xf9, 0x00, 0x00, 0x09, 0x00, 0x00, 0x11,
                                0xca, 0x02, 0x00, 0x00, 0x11, 0xc0, 0x80};

  EXPECT_TRUE(
      Parse({kTestData, kTestData + std::size(kTestData) / sizeof(uint8_t)}));

  std::vector<uint8_t> extra_data = ac4_.StreamInfo();
  AC4StreamInfo* stream_info = (AC4StreamInfo*)extra_data.data();
  EXPECT_EQ(stream_info->bitstream_version, 0x02);
  EXPECT_EQ(stream_info->presentation_version, 0x01);
  EXPECT_EQ(stream_info->presentation_level, 0x01);
  EXPECT_EQ(stream_info->is_ajoc, 0);
  EXPECT_EQ(stream_info->is_ims, 0);
  EXPECT_EQ(stream_info->channels, 6);
}

TEST_F(AC4Test, IMSInputTest) {
  // Format              : AC-4
  // Bitstream version   : 2
  // Presentation version: 2
  // Presentation level  : 0
  // Channel mode        : Stereo
  // Channel layout      : L R
  constexpr uint8_t kTestData[]{
      0x20, 0xba, 0x02, 0x40, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xe0,
      0x02, 0x0f, 0xf8, 0x80, 0x00, 0x00, 0x42, 0x00, 0x00, 0x02, 0x50, 0x10,
      0x00, 0x00, 0x03, 0x08, 0xc0, 0x01, 0x0f, 0xf8, 0x80, 0x00, 0x00, 0x42,
      0x00, 0x00, 0x02, 0x50, 0x10, 0x00, 0x00, 0x03, 0x00, 0xc0};

  EXPECT_TRUE(
      Parse({kTestData, kTestData + std::size(kTestData) / sizeof(uint8_t)}));

  std::vector<uint8_t> extra_data = ac4_.StreamInfo();
  AC4StreamInfo* stream_info = (AC4StreamInfo*)extra_data.data();
  EXPECT_EQ(stream_info->bitstream_version, 0x02);
  EXPECT_EQ(stream_info->presentation_version, 0x02);
  EXPECT_EQ(stream_info->presentation_level, 0x00);
  EXPECT_EQ(stream_info->is_ajoc, 0);
  EXPECT_EQ(stream_info->is_ims, 1);
  EXPECT_EQ(stream_info->channels, 2);
}

TEST_F(AC4Test, AJOCInputTest) {
  // Format              : AC-4
  // Bitstream version   : 2
  // Presentation version: 1
  // Presentation level  : 4
  constexpr uint8_t kTestData[]{0x20, 0xba, 0x01, 0x40, 0x00, 0x00, 0x00,
                                0x1f, 0xff, 0xff, 0xff, 0xe0, 0x01, 0x0b,
                                0xfc, 0x80, 0x00, 0x00, 0x08, 0x02, 0x28,
                                0x3d, 0x20, 0x00, 0x00};

  EXPECT_TRUE(
      Parse({kTestData, kTestData + std::size(kTestData) / sizeof(uint8_t)}));

  std::vector<uint8_t> extra_data = ac4_.StreamInfo();
  AC4StreamInfo* stream_info = (AC4StreamInfo*)extra_data.data();
  EXPECT_EQ(stream_info->bitstream_version, 0x02);
  EXPECT_EQ(stream_info->presentation_version, 0x01);
  EXPECT_EQ(stream_info->presentation_level, 0x04);
  EXPECT_EQ(stream_info->is_ajoc, 1);
  EXPECT_EQ(stream_info->is_ims, 0);
  EXPECT_EQ(stream_info->channels, 0);
}

}  // namespace mp4

}  // namespace media
