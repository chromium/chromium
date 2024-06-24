// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/temporal_scalability_id_extractor.h"

#include "media/parsers/vp9_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using bytes = std::vector<uint8_t>;

TEST(TemporalScalabilityIdExtractorTest, AssignTemporalIdBySvcSpec_2layers) {
  TemporalScalabilityIdExtractor extractor(VideoCodec::kH264, 2);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(0), 0);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(1), 1);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(2), 0);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(3), 1);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(4), 0);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(5), 1);
}

TEST(TemporalScalabilityIdExtractorTest, AssignTemporalIdBySvcSpec_3layers) {
  TemporalScalabilityIdExtractor extractor(VideoCodec::kH264, 3);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(0), 0);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(1), 2);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(2), 1);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(3), 2);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(4), 0);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(5), 2);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(6), 1);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(7), 2);
  EXPECT_EQ(extractor.AssignTemporalIdBySvcSpec(8), 0);
}

// `frame_id` doesn't matter, `temporal_id` is populated from the stream
TEST(TemporalScalabilityIdExtractorTest, H264_SvcHeaders) {
  TemporalScalabilityIdExtractor extractor(VideoCodec::kH264, 3);
  TemporalScalabilityIdExtractor::BitstreamMetadata md;
  md.temporal_id = 99;
  EXPECT_TRUE(extractor.ParseChunk(
      bytes{0, 0, 0, 1, 0x6e, 0xc0, 0x80, 0x0f, 0x20}, 0, md));
  EXPECT_EQ(md.temporal_id, 0);

  EXPECT_TRUE(
      extractor.ParseChunk(bytes{0, 0, 0, 1, 0x0e, 0x82, 0x80, 0x4f}, 0, md));
  EXPECT_EQ(md.temporal_id, 2);

  EXPECT_TRUE(extractor.ParseChunk(
      bytes{0, 0, 0, 1, 0x6e, 0x81, 0x80, 0x2f, 0x20}, 0, md));
  EXPECT_EQ(md.temporal_id, 1);
}

// Bitstream is corrupted, `temporal_id` is populated from `frame_id`
TEST(TemporalScalabilityIdExtractorTest, H264_CorruptedStream) {
  TemporalScalabilityIdExtractor extractor(VideoCodec::kH264, 2);
  TemporalScalabilityIdExtractor::BitstreamMetadata md;
  md.temporal_id = 99;
  EXPECT_FALSE(
      extractor.ParseChunk(bytes{0, 0, 0, 1, 0xff, 0xff, 0xff}, 0, md));
  EXPECT_EQ(md.temporal_id, 0);

  EXPECT_FALSE(
      extractor.ParseChunk(bytes{0, 0, 0, 1, 0xff, 0xff, 0xff}, 1, md));
  EXPECT_EQ(md.temporal_id, 1);

  EXPECT_FALSE(
      extractor.ParseChunk(bytes{0, 0, 0, 1, 0xff, 0xff, 0xff}, 2, md));
  EXPECT_EQ(md.temporal_id, 0);
}

}  // namespace media
