// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/adts_stream_parser.h"

#include <memory>

#include "media/formats/common/stream_parser_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class ADTSStreamParserTest : public StreamParserTestBase, public testing::Test {
 public:
  ADTSStreamParserTest()
      : StreamParserTestBase(std::make_unique<ADTSStreamParser>()) {}
};

// Test appending and parsing with small prime sized chunks to smoke out "power
// of 2" field size assumptions.
TEST_F(ADTSStreamParserTest, UnalignedAppend) {
  const std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.adts", 17));
}

// Test appending and parsing with larger piece sizes to verify that multiple
// buffers are passed to `new_buffer_cb_`.
TEST_F(ADTSStreamParserTest, UnalignedAppend512) {
  const std::string expected =
      "NewSegment"
      "{ 0K 23K 46K }"
      "{ 0K 23K 46K 69K 92K }"
      "{ 0K 23K 46K 69K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.adts", 512));
}

// Test that the ADTS channel layout mapping table in Rust matches C++
// ChannelLayout enum values.
TEST_F(ADTSStreamParserTest, ChannelLayoutMapping) {
  struct TestCase {
    int channel_config;
    std::vector<uint8_t> header;
    ChannelLayout expected_layout;
  };

  // Pre-calculated ADTS headers for sample_rate = 44100, frame_size = 7, VBR.
  std::vector<TestCase> test_cases = {
      {1, {0xFF, 0xF1, 0x50, 0x40, 0x00, 0xFF, 0xFC}, CHANNEL_LAYOUT_MONO},
      {2, {0xFF, 0xF1, 0x50, 0x80, 0x00, 0xFF, 0xFC}, CHANNEL_LAYOUT_STEREO},
      {3, {0xFF, 0xF1, 0x50, 0xC0, 0x00, 0xFF, 0xFC}, CHANNEL_LAYOUT_SURROUND},
      {4, {0xFF, 0xF1, 0x51, 0x00, 0x00, 0xFF, 0xFC}, CHANNEL_LAYOUT_4_0},
      {5, {0xFF, 0xF1, 0x51, 0x40, 0x00, 0xFF, 0xFC}, CHANNEL_LAYOUT_5_0_BACK},
      {6, {0xFF, 0xF1, 0x51, 0x80, 0x00, 0xFF, 0xFC}, CHANNEL_LAYOUT_5_1_BACK},
      {7, {0xFF, 0xF1, 0x51, 0xC0, 0x00, 0xFF, 0xFC}, CHANNEL_LAYOUT_7_1},
  };

  for (const auto& test_case : test_cases) {
    auto header = ADTSStreamParser::ParseHeader(test_case.header);
    ASSERT_TRUE(header.has_value())
        << "Failed to parse header for channel_config "
        << test_case.channel_config;
    EXPECT_EQ(header->channel_layout, test_case.expected_layout)
        << "Mismatch for channel_config " << test_case.channel_config;
  }
}

}  // namespace media
