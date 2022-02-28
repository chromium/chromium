// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/h264_parser.h"

#include <limits>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/subsample_entry.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class H264SPSTest : public ::testing::Test {
 public:
  // An exact clone of an SPS from Big Buck Bunny 480p.
  std::unique_ptr<H264SPS> MakeSPS_BBB480p() {
    std::unique_ptr<H264SPS> sps = std::make_unique<H264SPS>();
    sps->profile_idc = 100;
    sps->level_idc = 30;
    sps->chroma_format_idc = 1;
    sps->log2_max_pic_order_cnt_lsb_minus4 = 2;
    sps->max_num_ref_frames = 5;
    sps->pic_width_in_mbs_minus1 = 52;
    sps->pic_height_in_map_units_minus1 = 29;
    sps->frame_mbs_only_flag = true;
    sps->direct_8x8_inference_flag = true;
    sps->vui_parameters_present_flag = true;
    sps->timing_info_present_flag = true;
    sps->num_units_in_tick = 1;
    sps->time_scale = 48;
    sps->fixed_frame_rate_flag = true;
    sps->bitstream_restriction_flag = true;
    // These next three fields are not part of our SPS struct yet.
    // sps->motion_vectors_over_pic_boundaries_flag = true;
    // sps->log2_max_mv_length_horizontal = 10;
    // sps->log2_max_mv_length_vertical = 10;
    sps->max_num_reorder_frames = 2;
    sps->max_dec_frame_buffering = 5;

    // Computed field, matches |chroma_format_idc| in this case.
    // TODO(sandersd): Extract that computation from the parsing step.
    sps->chroma_array_type = 1;

    return sps;
  }
};

TEST_F(H264SPSTest, GetCodedSize) {
  std::unique_ptr<H264SPS> sps = MakeSPS_BBB480p();
  EXPECT_EQ(gfx::Size(848, 480), sps->GetCodedSize());

  // Overflow.
  sps->pic_width_in_mbs_minus1 = std::numeric_limits<int>::max();
  EXPECT_EQ(absl::nullopt, sps->GetCodedSize());
}

TEST_F(H264SPSTest, GetVisibleRect) {
  std::unique_ptr<H264SPS> sps = MakeSPS_BBB480p();
  EXPECT_EQ(gfx::Rect(0, 0, 848, 480), sps->GetVisibleRect());

  // Add some cropping.
  sps->frame_cropping_flag = true;
  sps->frame_crop_left_offset = 1;
  sps->frame_crop_right_offset = 2;
  sps->frame_crop_top_offset = 3;
  sps->frame_crop_bottom_offset = 4;
  EXPECT_EQ(gfx::Rect(2, 6, 848 - 6, 480 - 14), sps->GetVisibleRect());

  // Not quite invalid.
  sps->frame_crop_left_offset = 422;
  sps->frame_crop_right_offset = 1;
  sps->frame_crop_top_offset = 0;
  sps->frame_crop_bottom_offset = 0;
  EXPECT_EQ(gfx::Rect(844, 0, 2, 480), sps->GetVisibleRect());

  // Invalid crop.
  sps->frame_crop_left_offset = 423;
  sps->frame_crop_right_offset = 1;
  sps->frame_crop_top_offset = 0;
  sps->frame_crop_bottom_offset = 0;
  EXPECT_EQ(absl::nullopt, sps->GetVisibleRect());

  // Overflow.
  sps->frame_crop_left_offset = std::numeric_limits<int>::max() / 2 + 1;
  sps->frame_crop_right_offset = 0;
  sps->frame_crop_top_offset = 0;
  sps->frame_crop_bottom_offset = 0;
  EXPECT_EQ(absl::nullopt, sps->GetVisibleRect());
}

TEST(H264ParserTest, StreamFileParsing) {
  base::FilePath file_path = GetTestDataFilePath("test-25fps.h264");
  // Number of NALUs in the test stream to be parsed.
  int num_nalus = 759;

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  H264Parser parser;
  parser.SetStream(stream.data(), stream.length());

  // Parse until the end of stream/unsupported stream/error in stream is found.
  int num_parsed_nalus = 0;
  while (true) {
    media::H264SliceHeader shdr;
    media::H264SEIMessage sei_msg;
    H264NALU nalu;
    H264Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res == H264Parser::kEOStream) {
      DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
               << num_parsed_nalus;
      ASSERT_EQ(num_nalus, num_parsed_nalus);
      return;
    }
    ASSERT_EQ(res, H264Parser::kOk);

    ++num_parsed_nalus;

    int id;
    switch (nalu.nal_unit_type) {
      case H264NALU::kIDRSlice:
      case H264NALU::kNonIDRSlice:
        ASSERT_EQ(parser.ParseSliceHeader(nalu, &shdr), H264Parser::kOk);
        break;

      case H264NALU::kSPS:
        ASSERT_EQ(parser.ParseSPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kPPS:
        ASSERT_EQ(parser.ParsePPS(&id), H264Parser::kOk);
        break;

      case H264NALU::kSEIMessage:
        ASSERT_EQ(parser.ParseSEI(&sei_msg), H264Parser::kOk);
        break;

      default:
        // Skip unsupported NALU.
        DVLOG(4) << "Skipping unsupported NALU";
        break;
    }
  }
}

TEST(H264ParserTest, ParseNALUsFromStreamFile) {
  base::FilePath file_path = GetTestDataFilePath("test-25fps.h264");
  // Number of NALUs in the test stream to be parsed.
  const size_t num_nalus = 759;

  base::MemoryMappedFile stream;
  ASSERT_TRUE(stream.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  std::vector<H264NALU> nalus;
  ASSERT_TRUE(H264Parser::ParseNALUs(stream.data(), stream.length(), &nalus));
  ASSERT_EQ(num_nalus, nalus.size());
}

// Verify that GetCurrentSubsamples works.
TEST(H264ParserTest, GetCurrentSubsamplesNormal) {
  const uint8_t kStream[] = {
      // First NALU.
      // Clear bytes = 4.
      0x00, 0x00, 0x01,  // start code.
      0x65,              // Nalu type = 5, IDR slice.
      // Below is bogus data.
      // Encrypted bytes = 15.
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06,
      // Clear bytes = 5.
      0x07, 0x00, 0x01, 0x02, 0x03,
      // Encrypted until next NALU. Encrypted bytes = 20.
      0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Note that this is still in the encrypted region but looks like a start
      // code.
      0x00, 0x00, 0x01, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Second NALU. Completely clear.
      // Clear bytes = 10.
      0x00, 0x00, 0x01,  // start code.
      0x06,              // nalu type = 6, SEI.
      // Bogus data.
      0xff, 0xfe, 0xfd, 0xee, 0x12, 0x33,
  };
  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(4u, 15u);
  subsamples.emplace_back(5u, 20u);
  subsamples.emplace_back(10u, 0u);
  H264Parser parser;
  parser.SetEncryptedStream(kStream, std::size(kStream), subsamples);

  H264NALU nalu;
  ASSERT_EQ(H264Parser::kOk, parser.AdvanceToNextNALU(&nalu));
  auto nalu_subsamples = parser.GetCurrentSubsamples();
  ASSERT_EQ(2u, nalu_subsamples.size());

  // Note that nalu->data starts from the NALU header, i.e. does not include
  // the start code.
  EXPECT_EQ(1u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(15u, nalu_subsamples[0].cypher_bytes);
  EXPECT_EQ(5u, nalu_subsamples[1].clear_bytes);
  EXPECT_EQ(20u, nalu_subsamples[1].cypher_bytes);

  // Make sure that it reached the next NALU.
  EXPECT_EQ(H264Parser::kOk, parser.AdvanceToNextNALU(&nalu));
  nalu_subsamples = parser.GetCurrentSubsamples();
  ASSERT_EQ(1u, nalu_subsamples.size());

  EXPECT_EQ(7u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[0].cypher_bytes);
}

// Verify that subsamples starting at non-NALU boundary also works.
TEST(H264ParserTest, GetCurrentSubsamplesSubsampleNotStartingAtNaluBoundary) {
  const uint8_t kStream[] = {
      // First NALU.
      // Clear bytes = 4.
      0x00, 0x00, 0x01,  // start code.
      0x65,              // Nalu type = 5, IDR slice.
      // Below is bogus data.
      // Encrypted bytes = 24.
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03,
      0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Clear bytes = 18. The rest is in the clear. Note that this is not at
      // a NALU boundary and a NALU starts below.
      0xaa, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      // Second NALU. Completely clear.
      0x00, 0x00, 0x01,  // start code.
      0x06,              // nalu type = 6, SEI.
      // Bogus data.
      0xff, 0xfe, 0xfd, 0xee, 0x12, 0x33,
  };

  std::vector<SubsampleEntry> subsamples;
  subsamples.emplace_back(4u, 24u);
  subsamples.emplace_back(18, 0);
  H264Parser parser;
  parser.SetEncryptedStream(kStream, std::size(kStream), subsamples);

  H264NALU nalu;
  ASSERT_EQ(H264Parser::kOk, parser.AdvanceToNextNALU(&nalu));
  auto nalu_subsamples = parser.GetCurrentSubsamples();
  ASSERT_EQ(2u, nalu_subsamples.size());

  // Note that nalu->data starts from the NALU header, i.e. does not include
  // the start code.
  EXPECT_EQ(1u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(24u, nalu_subsamples[0].cypher_bytes);

  // The nalu ends with 8 more clear bytes. The last 10 bytes should be
  // associated with the next nalu.
  EXPECT_EQ(8u, nalu_subsamples[1].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[1].cypher_bytes);

  ASSERT_EQ(H264Parser::kOk, parser.AdvanceToNextNALU(&nalu));
  nalu_subsamples = parser.GetCurrentSubsamples();
  ASSERT_EQ(1u, nalu_subsamples.size());

  // Although the input had 10 more bytes, since nalu->data starts from the nalu
  // header, there's only 7 more bytes left.
  EXPECT_EQ(7u, nalu_subsamples[0].clear_bytes);
  EXPECT_EQ(0u, nalu_subsamples[0].cypher_bytes);
}

}  // namespace media
