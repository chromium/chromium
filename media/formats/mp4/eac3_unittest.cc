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

}  // namespace mp4

}  // namespace media
