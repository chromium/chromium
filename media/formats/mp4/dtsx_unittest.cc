// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "media/base/mock_media_log.h"
#include "media/formats/mp4/dtsx.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

namespace mp4 {

class DTSXTest : public testing::Test {
 public:
  DTSXTest() = default;

  bool Parse(const std::vector<uint8_t>& data) {
    return dtsx_.Parse(data, &media_log_);
  }

  StrictMock<MockMediaLog> media_log_;
  DTSX dtsx_;
};

TEST_F(DTSXTest, NoInputTest) {
  std::vector<uint8_t> data;
  EXPECT_FALSE(Parse(data));
}

TEST_F(DTSXTest, ShortInvalidInputTest) {
  std::vector<uint8_t> data({0x32, 0x44});

  EXPECT_FALSE(Parse(data));
}

TEST_F(DTSXTest, NormalInputTest) {
  std::vector<uint8_t> data({0x01, 0x20, 0x00, 0x00, 0x00, 0x3f, 0x80, 0x00});

  EXPECT_TRUE(Parse(data));
  EXPECT_EQ(dtsx_.GetDecoderProfileCode(), 0u);
  EXPECT_EQ(dtsx_.GetFrameDuration(), 1024);
  EXPECT_EQ(dtsx_.GetMaxPayload(), 4096);
  EXPECT_EQ(dtsx_.GetNumPresentations(), 0);
  EXPECT_EQ(dtsx_.GetChannelMask(), 63u);
  EXPECT_EQ(dtsx_.GetSamplingFrequency(), 48000);
}

}  // namespace mp4

}  // namespace media
