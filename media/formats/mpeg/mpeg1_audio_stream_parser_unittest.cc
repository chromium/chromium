// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"

#include <stdint.h>

#include <memory>

#include "media/base/test_data_util.h"
#include "media/formats/common/stream_parser_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MPEG1AudioStreamParserTest
    : public StreamParserTestBase, public testing::Test {
 public:
  MPEG1AudioStreamParserTest()
      : StreamParserTestBase(std::make_unique<MPEG1AudioStreamParser>()) {}
};

// Test appending and parsing with small prime sized chunks to smoke out "power
// of 2" field size assumptions.
TEST_F(MPEG1AudioStreamParserTest, UnalignedAppend) {
  const std::string expected =
      "NewSegment"
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
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 17));
  EXPECT_GT(last_audio_config().codec_delay(), 0);
}

TEST_F(MPEG1AudioStreamParserTest, UnalignedAppendMP2) {
  const std::string expected =
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp2", 17));
  EXPECT_GT(last_audio_config().codec_delay(), 0);
}

// Test appending and parsing with larger piece sizes to verify that multiple
// buffers are passed to `new_buffer_cb_`.
TEST_F(MPEG1AudioStreamParserTest, UnalignedAppend512) {
  const std::string expected =
      "NewSegment"
      "{ 0K 26K 52K }"
      "{ 0K }"
      "EndOfSegment"
      "NewSegment"
      "{ 0K 26K 52K }"
      "{ 0K 26K 52K 78K }"
      "{ 0K }"
      "EndOfSegment";
  EXPECT_EQ(expected, ParseFile("sfx.mp3", 512));
  EXPECT_GT(last_audio_config().codec_delay(), 0);
}

TEST_F(MPEG1AudioStreamParserTest, MetadataParsing) {
  scoped_refptr<DecoderBuffer> buffer = ReadTestDataFile("sfx.mp3");
  int offset = 0;

  // The first 32 bytes of sfx.mp3 are an ID3 tag, so no segments should be
  // extracted after appending those bytes.
  const int kId3TagSize = 32;
  EXPECT_EQ("", ParseData(buffer->AsSpan().subspan(offset, kId3TagSize)));
  EXPECT_FALSE(last_audio_config().IsValidConfig());
  offset += kId3TagSize;

  // The next 417 bytes are a Xing frame; with the identifier 21 bytes into
  // the frame.  Appending less than 21 bytes, should result in no segments
  // nor an AudioDecoderConfig being created.
  const int kXingTagPosition = 21;
  EXPECT_EQ("", ParseData(buffer->AsSpan().subspan(offset, kXingTagPosition)));
  EXPECT_FALSE(last_audio_config().IsValidConfig());
  offset += kXingTagPosition;

  // Appending the rests of the Xing frame should result in no segments, but
  // should generate a valid AudioDecoderConfig.
  const int kXingRemainingSize = 417 - kXingTagPosition;
  EXPECT_EQ("",
            ParseData(buffer->AsSpan().subspan(offset, kXingRemainingSize)));
  EXPECT_TRUE(last_audio_config().IsValidConfig());
  offset += kXingRemainingSize;

  // Append the first real frame and ensure we get a segment.
  const int kFirstRealFrameSize = 182;
  EXPECT_EQ("NewSegment{ 0K }EndOfSegment",
            ParseData(buffer->AsSpan().subspan(offset, kFirstRealFrameSize)));
  EXPECT_TRUE(last_audio_config().IsValidConfig());
}

}  // namespace media
