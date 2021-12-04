// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/formats/dts/dts_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::StrictMock;

namespace media {

class DTSUtilTest : public testing::Test {
 public:
  DTSUtilTest() = default;

  StrictMock<MockMediaLog> media_log_;
};

TEST_F(DTSUtilTest, NoInputTest) {
  constexpr uint8_t* data = nullptr;
  EXPECT_EQ(0, media::dts::ParseTotalSampleCount(data, 0, AudioCodec::kDTS));
}

TEST_F(DTSUtilTest, IncompleteInputTest) {
  base::FilePath file_path = GetTestDataFilePath("dts.bin");
  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  const uint8_t* data = stream.data();
  EXPECT_EQ(0, media::dts::ParseTotalSampleCount(data, stream.length() - 1,
                                                 AudioCodec::kDTS));
}

TEST_F(DTSUtilTest, NormalInputTest) {
  base::FilePath file_path = GetTestDataFilePath("dts.bin");
  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();
  const uint8_t* data = stream.data();
  int total = media::dts::ParseTotalSampleCount(data, stream.length(),
                                                AudioCodec::kDTS);
  EXPECT_EQ(total, 512);
}

}  // namespace media
