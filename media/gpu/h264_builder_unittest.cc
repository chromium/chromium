// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h264_builder.h"

#include "base/logging.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/parsers/h264_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class H264BuilderTest : public ::testing::Test {
 public:
  // bear.mp4 without VUI parameters.
  static H264SPS MakeTestSPS() {
    H264SPS sps;
    sps.profile_idc = 100;
    sps.level_idc = 13;
    sps.chroma_format_idc = 1;
    memset(sps.scaling_list4x4, 16, sizeof(sps.scaling_list4x4));
    memset(sps.scaling_list8x8, 16, sizeof(sps.scaling_list8x8));
    sps.log2_max_frame_num_minus4 = 5;
    sps.log2_max_pic_order_cnt_lsb_minus4 = 6;
    sps.max_num_ref_frames = 4;
    sps.pic_width_in_mbs_minus1 = 19;
    sps.pic_height_in_map_units_minus1 = 11;
    sps.frame_mbs_only_flag = true;
    sps.direct_8x8_inference_flag = true;
    sps.frame_cropping_flag = true;
    sps.frame_crop_bottom_offset = 6;
    sps.chroma_array_type = 1;
    return sps;
  }

  static H264PPS MakeTestPPS() {
    H264PPS pps;
    pps.entropy_coding_mode_flag = true;
    pps.weighted_bipred_idc = 2;
    pps.chroma_qp_index_offset = -2;
    pps.deblocking_filter_control_present_flag = true;
    pps.transform_8x8_mode_flag = true;
    pps.second_chroma_qp_index_offset = -2;
    return pps;
  }
};

TEST_F(H264BuilderTest, H264BuildParseIdentity) {
  H264SPS sps = MakeTestSPS();
  H264PPS pps = MakeTestPPS();

  H26xAnnexBBitstreamBuilder bitstream_builder(
      /*insert_emulation_prevention_bytes=*/true);
  BuildPackedH264SPS(bitstream_builder, sps);
  BuildPackedH264PPS(bitstream_builder, sps, pps);

  H264Parser parser;
  parser.SetStream(bitstream_builder.data(), bitstream_builder.BytesInBuffer());
  H264NALU nalu;
  EXPECT_EQ(parser.AdvanceToNextNALU(&nalu), H264Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H264NALU::kSPS);
  int sps_id;
  EXPECT_EQ(parser.ParseSPS(&sps_id), H264Parser::Result::kOk);

  EXPECT_EQ(memcmp(parser.GetSPS(sps_id), &sps, sizeof(sps)), 0);

  EXPECT_EQ(parser.AdvanceToNextNALU(&nalu), H264Parser::Result::kOk);
  EXPECT_EQ(nalu.nal_unit_type, H264NALU::kPPS);
  int pps_id;
  EXPECT_EQ(parser.ParsePPS(&pps_id), H264Parser::Result::kOk);

  EXPECT_EQ(memcmp(parser.GetPPS(pps_id), &pps, sizeof(pps)), 0);
}

}  // namespace media
