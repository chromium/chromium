// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/temporal_scalability_id_extractor.h"

#include "base/files/file_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/parsers/ivf_parser.h"
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

std::vector<scoped_refptr<media::DecoderBuffer>> ReadIVF(
    const std::string& fname) {
  std::string ivf_data;
  auto input_file = media::GetTestDataFilePath(fname);
  EXPECT_TRUE(base::ReadFileToString(input_file, &ivf_data));

  media::IvfParser ivf_parser;
  media::IvfFileHeader ivf_header{};
  EXPECT_TRUE(
      ivf_parser.Initialize(reinterpret_cast<const uint8_t*>(ivf_data.data()),
                            ivf_data.size(), &ivf_header));

  std::vector<scoped_refptr<media::DecoderBuffer>> buffers;
  media::IvfFrameHeader ivf_frame_header{};
  const uint8_t* data;
  while (ivf_parser.ParseNextFrame(&ivf_frame_header, &data)) {
    buffers.push_back(media::DecoderBuffer::CopyFrom(
        // TODO(crbug.com/40284755): Spanify `ParseNextFrame`.
        UNSAFE_TODO(base::span(data, ivf_frame_header.frame_size))));
  }
  return buffers;
}

struct AV1TemporalScalabilityData {
  int frame_index;
  uint8_t temporal_idx;
  uint8_t reference_idx_flags;
  uint8_t refresh_frame_flags;
};

// `temporal_id`, `reference_frame_indices`, and `refresh_frame_flags` are
// populated from the bitstream.
TEST(TemporalScalabilityIdExtractorTest, AV1_TwoTemporalLayers) {
  TemporalScalabilityIdExtractor extractor(VideoCodec::kAV1, 2);
  TemporalScalabilityIdExtractor::BitstreamMetadata md;

  const std::string kTLStream("av1-svc-L1T2.ivf");
  auto buffers = ReadIVF(kTLStream);
  ASSERT_FALSE(buffers.empty());

  const std::vector<AV1TemporalScalabilityData> expected{
      {0, 0, 0b00000000, 0b11111111}, {1, 1, 0b01111111, 0b00000000},
      {2, 0, 0b01111111, 0b00000100}, {3, 1, 0b01111111, 0b00000000},
      {4, 0, 0b01111111, 0b00000010}, {5, 1, 0b01111111, 0b00000000},
      {6, 0, 0b01111111, 0b00000001}, {7, 1, 0b01111111, 0b00000000},
  };
  for (int i = 0; i < static_cast<int>(buffers.size()); i++) {
    EXPECT_TRUE(extractor.ParseChunk(buffers[i]->AsSpan(), i, md));
    EXPECT_EQ(md.temporal_id, expected[i].temporal_idx);
    EXPECT_EQ(md.reference_idx_flags, expected[i].reference_idx_flags);
    EXPECT_EQ(md.refresh_frame_flags, expected[i].refresh_frame_flags);
  }
}

}  // namespace media
