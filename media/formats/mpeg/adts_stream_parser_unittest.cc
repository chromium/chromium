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

}  // namespace media
