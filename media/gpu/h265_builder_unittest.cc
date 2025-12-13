// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h265_builder.h"

#include "media/filters/h26x_annex_b_bitstream_builder.h"
#include "media/parsers/h265_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class H265BuilderTest : public testing::Test {
 protected:
  void SetUp() override { parser_ = std::make_unique<H265Parser>(); }

  std::unique_ptr<H265Parser> parser_;
};

TEST_F(H265BuilderTest, BuildVUIParametersInSPS) {
  // Create an SPS with VUI parameters
  H265SPS sps = {};
  sps.sps_video_parameter_set_id = 0;
  sps.sps_max_sub_layers_minus1 = 0;
  sps.sps_temporal_id_nesting_flag = 1;
  sps.profile_tier_level.general_profile_idc = 1;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x40000000;
  sps.profile_tier_level.general_progressive_source_flag = 1;
  sps.profile_tier_level.general_interlaced_source_flag = 0;
  sps.profile_tier_level.general_non_packed_constraint_flag = 1;
  sps.profile_tier_level.general_frame_only_constraint_flag = 1;
  sps.profile_tier_level.general_level_idc = 120;
  sps.sps_seq_parameter_set_id = 0;
  sps.chroma_format_idc = 1;
  sps.pic_width_in_luma_samples = 1920;
  sps.pic_height_in_luma_samples = 1080;
  sps.bit_depth_luma_minus8 = 0;
  sps.bit_depth_chroma_minus8 = 0;
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.sps_max_dec_pic_buffering_minus1[0] = 1;
  sps.sps_max_num_reorder_pics[0] = 0;
  sps.sps_max_latency_increase_plus1[0] = 0;
  sps.log2_min_luma_coding_block_size_minus3 = 0;
  sps.log2_diff_max_min_luma_coding_block_size = 3;
  sps.log2_min_luma_transform_block_size_minus2 = 0;
  sps.log2_diff_max_min_luma_transform_block_size = 3;
  sps.max_transform_hierarchy_depth_inter = 0;
  sps.max_transform_hierarchy_depth_intra = 0;
  sps.scaling_list_enabled_flag = false;
  sps.amp_enabled_flag = true;
  sps.sample_adaptive_offset_enabled_flag = true;
  sps.pcm_enabled_flag = false;
  sps.num_short_term_ref_pic_sets = 0;
  sps.long_term_ref_pics_present_flag = false;
  sps.sps_temporal_mvp_enabled_flag = true;
  sps.strong_intra_smoothing_enabled_flag = true;

  // Set VUI parameters
  sps.vui_parameters_present_flag = true;
  sps.vui_parameters.colour_description_present_flag = true;
  sps.vui_parameters.video_full_range_flag = false;
  sps.vui_parameters.colour_primaries = 1;          // Rec.709
  sps.vui_parameters.transfer_characteristics = 1;  // Rec.709
  sps.vui_parameters.matrix_coeffs = 1;             // Rec.709

  // Build the SPS
  H26xAnnexBBitstreamBuilder builder;
  BuildPackedH265SPS(builder, sps);
  builder.Flush();

  // Parse the generated bitstream
  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);

  int sps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSPS(&sps_id));

  const H265SPS* parsed_sps = parser_->GetSPS(sps_id);
  ASSERT_TRUE(parsed_sps);

  // Verify VUI parameters were correctly encoded and parsed
  EXPECT_TRUE(parsed_sps->vui_parameters_present_flag);
  EXPECT_TRUE(parsed_sps->vui_parameters.colour_description_present_flag);
  EXPECT_EQ(sps.vui_parameters.video_full_range_flag,
            parsed_sps->vui_parameters.video_full_range_flag);
  EXPECT_EQ(sps.vui_parameters.colour_primaries,
            parsed_sps->vui_parameters.colour_primaries);
  EXPECT_EQ(sps.vui_parameters.transfer_characteristics,
            parsed_sps->vui_parameters.transfer_characteristics);
  EXPECT_EQ(sps.vui_parameters.matrix_coeffs,
            parsed_sps->vui_parameters.matrix_coeffs);

  // Verify other SPS fields
  EXPECT_EQ(sps.pic_width_in_luma_samples,
            parsed_sps->pic_width_in_luma_samples);
  EXPECT_EQ(sps.pic_height_in_luma_samples,
            parsed_sps->pic_height_in_luma_samples);
  EXPECT_EQ(sps.chroma_format_idc, parsed_sps->chroma_format_idc);
}

TEST_F(H265BuilderTest, BuildSPSWithoutVUIParameters) {
  // Create an SPS without VUI parameters
  H265SPS sps = {};
  sps.sps_video_parameter_set_id = 0;
  sps.sps_max_sub_layers_minus1 = 0;
  sps.sps_temporal_id_nesting_flag = 1;
  sps.profile_tier_level.general_profile_idc = 1;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x40000000;
  sps.profile_tier_level.general_progressive_source_flag = 1;
  sps.profile_tier_level.general_interlaced_source_flag = 0;
  sps.profile_tier_level.general_non_packed_constraint_flag = 1;
  sps.profile_tier_level.general_frame_only_constraint_flag = 1;
  sps.profile_tier_level.general_level_idc = 120;
  sps.sps_seq_parameter_set_id = 0;
  sps.chroma_format_idc = 1;
  sps.pic_width_in_luma_samples = 1280;
  sps.pic_height_in_luma_samples = 720;
  sps.bit_depth_luma_minus8 = 0;
  sps.bit_depth_chroma_minus8 = 0;
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.sps_max_dec_pic_buffering_minus1[0] = 1;
  sps.sps_max_num_reorder_pics[0] = 0;
  sps.sps_max_latency_increase_plus1[0] = 0;
  sps.log2_min_luma_coding_block_size_minus3 = 0;
  sps.log2_diff_max_min_luma_coding_block_size = 3;
  sps.log2_min_luma_transform_block_size_minus2 = 0;
  sps.log2_diff_max_min_luma_transform_block_size = 3;
  sps.max_transform_hierarchy_depth_inter = 0;
  sps.max_transform_hierarchy_depth_intra = 0;
  sps.scaling_list_enabled_flag = false;
  sps.amp_enabled_flag = true;
  sps.sample_adaptive_offset_enabled_flag = true;
  sps.pcm_enabled_flag = false;
  sps.num_short_term_ref_pic_sets = 0;
  sps.long_term_ref_pics_present_flag = false;
  sps.sps_temporal_mvp_enabled_flag = true;
  sps.strong_intra_smoothing_enabled_flag = true;
  sps.vui_parameters_present_flag = false;

  // Build the SPS
  H26xAnnexBBitstreamBuilder builder;
  BuildPackedH265SPS(builder, sps);
  builder.Flush();

  // Parse the generated bitstream
  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);

  int sps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSPS(&sps_id));

  const H265SPS* parsed_sps = parser_->GetSPS(sps_id);
  ASSERT_TRUE(parsed_sps);

  // Verify VUI parameters are not present
  EXPECT_FALSE(parsed_sps->vui_parameters_present_flag);

  // Verify other SPS fields
  EXPECT_EQ(sps.pic_width_in_luma_samples,
            parsed_sps->pic_width_in_luma_samples);
  EXPECT_EQ(sps.pic_height_in_luma_samples,
            parsed_sps->pic_height_in_luma_samples);
  EXPECT_EQ(sps.chroma_format_idc, parsed_sps->chroma_format_idc);
}

}  // namespace media
