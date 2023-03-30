// Copyright 2021 The Chromium Authors
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
  EXPECT_EQ(0, media::dts::ParseTotalSampleCount(data, 0, AudioCodec::kDTSE));
  EXPECT_EQ(0, media::dts::ParseTotalSampleCount(data, 0, AudioCodec::kDTSXP2));
}

TEST_F(DTSUtilTest, IncompleteInputTestDTS) {
  base::FilePath file_path = GetTestDataFilePath("dts.bin");
  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  const uint8_t* data = stream.data();
  EXPECT_EQ(0, media::dts::ParseTotalSampleCount(data, stream.length() - 1,
                                                 AudioCodec::kDTS));
}

TEST_F(DTSUtilTest, IncompleteInputTestDTSXP2) {
  base::FilePath file_path = GetTestDataFilePath("dtsx.bin");
  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  const uint8_t* data = stream.data();
  EXPECT_EQ(0, media::dts::ParseTotalSampleCount(data, stream.length() - 1,
                                                 AudioCodec::kDTSXP2));
}

TEST_F(DTSUtilTest, NormalInputTestDTS) {
  base::FilePath file_path = GetTestDataFilePath("dts.bin");
  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();
  const uint8_t* data = stream.data();
  int total = media::dts::ParseTotalSampleCount(data, stream.length(),
                                                AudioCodec::kDTS);
  EXPECT_EQ(total, 512);
}

TEST_F(DTSUtilTest, GetDTSSamplesPerFrameTest) {
  EXPECT_EQ(512, media::dts::GetDTSSamplesPerFrame(AudioCodec::kDTS));
  EXPECT_EQ(1024, media::dts::GetDTSSamplesPerFrame(AudioCodec::kDTSXP2));
}

TEST_F(DTSUtilTest, WrapDTSWithIEC61937IncorrectInputTest) {
  constexpr uint8_t short_input[2048 - 7] = {0};
  constexpr uint8_t long_input[2048 + 3] = {0};
  std::vector<uint8_t> input_data;
  std::vector<uint8_t> output_data(2048);

  input_data =
      std::vector<uint8_t>(short_input, short_input + sizeof(short_input));
  EXPECT_EQ(0, media::dts::WrapDTSWithIEC61937(input_data, output_data,
                                               AudioCodec::kDTS));

  input_data =
      std::vector<uint8_t>(long_input, long_input + sizeof(long_input));
  EXPECT_EQ(0, media::dts::WrapDTSWithIEC61937(input_data, output_data,
                                               AudioCodec::kDTS));
}

TEST_F(DTSUtilTest, WrapDTSWithIEC61937NormalInputTest) {
  constexpr uint8_t header[8] = {0x72, 0xF8, 0x1F, 0x4E,
                                 0x0B, 0x00, 0x00, 0x20};
  constexpr uint8_t payload[4] = {1, 2, 3, 4};
  constexpr uint8_t swapped_payload[4] = {2, 1, 4, 3};
  uint8_t input[512] = {0};
  uint8_t output[2048] = {0};
  std::vector<uint8_t> output_data(2048);

  memcpy(input, payload, 4);
  std::vector<uint8_t> input_data(input, input + sizeof(input));
  EXPECT_EQ(2048, media::dts::WrapDTSWithIEC61937(input_data, output_data,
                                                  AudioCodec::kDTS));
  memcpy(output, header, 8);
  memcpy(output + 8, swapped_payload, 4);
  EXPECT_EQ(0, memcmp(output, output_data.data(), 2048));
}

}  // namespace media
