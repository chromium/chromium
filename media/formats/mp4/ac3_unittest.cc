// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

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

}  // namespace mp4

}  // namespace media
