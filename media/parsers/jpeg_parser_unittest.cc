// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include "base/files/memory_mapped_file.h"
#include "base/path_service.h"
#include "media/base/test_data_util.h"
#include "media/parsers/jpeg_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(JpegParserTest, Parsing) {
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));

  // This sample frame is captured from Chromebook Pixel
  base::FilePath file_path = data_dir.AppendASCII("media")
                                 .AppendASCII("test")
                                 .AppendASCII("data")
                                 .AppendASCII("pixel-1280x720.jpg");

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  JpegParseResult result;
  ASSERT_TRUE(ParseJpegPicture(stream.bytes(), &result));

  // Verify selected fields

  // SOF fields
  EXPECT_EQ(1280, result.frame_header.visible_width);
  EXPECT_EQ(720, result.frame_header.visible_height);
  EXPECT_EQ(1280, result.frame_header.coded_width);
  EXPECT_EQ(720, result.frame_header.coded_height);
  EXPECT_EQ(3, result.frame_header.num_components);
  EXPECT_EQ(1, result.frame_header.components[0].id);
  EXPECT_EQ(2, result.frame_header.components[0].horizontal_sampling_factor);
  EXPECT_EQ(1, result.frame_header.components[0].vertical_sampling_factor);
  EXPECT_EQ(0, result.frame_header.components[0].quantization_table_selector);
  EXPECT_EQ(2, result.frame_header.components[1].id);
  EXPECT_EQ(1, result.frame_header.components[1].horizontal_sampling_factor);
  EXPECT_EQ(1, result.frame_header.components[1].vertical_sampling_factor);
  EXPECT_EQ(1, result.frame_header.components[1].quantization_table_selector);
  EXPECT_EQ(3, result.frame_header.components[2].id);
  EXPECT_EQ(1, result.frame_header.components[2].horizontal_sampling_factor);
  EXPECT_EQ(1, result.frame_header.components[2].vertical_sampling_factor);
  EXPECT_EQ(1, result.frame_header.components[2].quantization_table_selector);

  // DRI fields
  EXPECT_EQ(0, result.restart_interval);

  // DQT fields
  EXPECT_TRUE(result.q_table[0].valid);
  EXPECT_TRUE(result.q_table[1].valid);
  EXPECT_FALSE(result.q_table[2].valid);
  EXPECT_FALSE(result.q_table[3].valid);

  // DHT fields (no DHT marker)
  EXPECT_FALSE(result.dc_table[0].valid);
  EXPECT_FALSE(result.ac_table[0].valid);
  EXPECT_FALSE(result.dc_table[1].valid);
  EXPECT_FALSE(result.ac_table[1].valid);

  // SOS fields
  EXPECT_EQ(3, result.scan.num_components);
  EXPECT_EQ(1, result.scan.components[0].component_selector);
  EXPECT_EQ(0, result.scan.components[0].dc_selector);
  EXPECT_EQ(0, result.scan.components[0].ac_selector);
  EXPECT_EQ(2, result.scan.components[1].component_selector);
  EXPECT_EQ(1, result.scan.components[1].dc_selector);
  EXPECT_EQ(1, result.scan.components[1].ac_selector);
  EXPECT_EQ(3, result.scan.components[2].component_selector);
  EXPECT_EQ(1, result.scan.components[2].dc_selector);
  EXPECT_EQ(1, result.scan.components[2].ac_selector);
  EXPECT_EQ(121148u, result.data_size);
  EXPECT_EQ(121358u, result.image_size);
}

TEST(JpegParserTest, TrailingZerosShouldBeIgnored) {
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));
  base::FilePath file_path =
      data_dir.AppendASCII("media")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("pixel-1280x720-trailing-zeros.jpg");

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  JpegParseResult result;
  ASSERT_TRUE(ParseJpegPicture(stream.bytes(), &result));

  // Verify selected fields

  // SOS fields
  EXPECT_EQ(121148u, result.data_size);
  EXPECT_EQ(121358u, result.image_size);
}

TEST(JpegParserTest, CodedSizeNotEqualVisibleSize) {
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &data_dir));

  base::FilePath file_path = data_dir.AppendASCII("media")
                                 .AppendASCII("test")
                                 .AppendASCII("data")
                                 .AppendASCII("blank-1x1.jpg");

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  JpegParseResult result;
  ASSERT_TRUE(ParseJpegPicture(stream.bytes(), &result));

  EXPECT_EQ(1, result.frame_header.visible_width);
  EXPECT_EQ(1, result.frame_header.visible_height);
  // The sampling factor of the given image is 2:2, so coded size is 16x16
  EXPECT_EQ(16, result.frame_header.coded_width);
  EXPECT_EQ(16, result.frame_header.coded_height);
  EXPECT_EQ(2, result.frame_header.components[0].horizontal_sampling_factor);
  EXPECT_EQ(2, result.frame_header.components[0].vertical_sampling_factor);
}

TEST(JpegParserTest, ParsingFail) {
  const uint8_t data[] = {0, 1, 2, 3};  // not jpeg
  JpegParseResult result;
  ASSERT_FALSE(ParseJpegPicture(data, &result));
}

}  // namespace media
