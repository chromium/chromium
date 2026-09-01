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
  sps.sps_temporal_id_nesting_flag = true;
  sps.profile_tier_level.general_profile_idc = 1;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x40000000;
  sps.profile_tier_level.general_progressive_source_flag = true;
  sps.profile_tier_level.general_interlaced_source_flag = false;
  sps.profile_tier_level.general_non_packed_constraint_flag = true;
  sps.profile_tier_level.general_frame_only_constraint_flag = true;
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
  sps.sps_temporal_id_nesting_flag = true;
  sps.profile_tier_level.general_profile_idc = 1;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x40000000;
  sps.profile_tier_level.general_progressive_source_flag = true;
  sps.profile_tier_level.general_interlaced_source_flag = false;
  sps.profile_tier_level.general_non_packed_constraint_flag = true;
  sps.profile_tier_level.general_frame_only_constraint_flag = true;
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

TEST_F(H265BuilderTest, BuildHDRStaticMetadataSEI) {
  H26xSEIMasteringDisplayInfo mdcv;
  mdcv.display_primaries[0] = {13250, 34500};  // G
  mdcv.display_primaries[1] = {7500, 3000};    // B
  mdcv.display_primaries[2] = {34000, 16000};  // R
  mdcv.white_points = {15635, 16450};
  mdcv.max_luminance = 10000000;
  // Contains a 0x000001 sequence, exercising emulation prevention.
  mdcv.min_luminance = 500;

  H26xSEIContentLightLevelInfo clli;
  clli.max_content_light_level = 1000;
  clli.max_picture_average_light_level = 400;

  H26xAnnexBBitstreamBuilder builder(
      /*insert_emulation_prevention_bytes=*/true);
  BuildPackedH265SEI(builder, mdcv, clli);
  builder.Flush();

  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  ASSERT_EQ(H265NALU::PREFIX_SEI_NUT, nalu.nal_unit_type);

  H265SEI sei;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSEI(&sei));
  bool found_mdcv = false;
  bool found_clli = false;
  for (const auto& sei_msg : sei.msgs) {
    if (const auto* info = std::get_if<H26xSEIMasteringDisplayInfo>(&sei_msg)) {
      found_mdcv = true;
      EXPECT_EQ(info->display_primaries, mdcv.display_primaries);
      EXPECT_EQ(info->white_points, mdcv.white_points);
      EXPECT_EQ(info->max_luminance, mdcv.max_luminance);
      EXPECT_EQ(info->min_luminance, mdcv.min_luminance);
    } else if (const auto* clli_info =
                   std::get_if<H26xSEIContentLightLevelInfo>(&sei_msg)) {
      found_clli = true;
      EXPECT_EQ(clli_info->max_content_light_level,
                clli.max_content_light_level);
      EXPECT_EQ(clli_info->max_picture_average_light_level,
                clli.max_picture_average_light_level);
    }
  }
  EXPECT_TRUE(found_mdcv);
  EXPECT_TRUE(found_clli);
}

TEST_F(H265BuilderTest, BuildRangeExtensionProfileTierLevel) {
  // Main 4:4:4 10 profile: profile_idc 4 with the compatibility flag for the
  // range extensions profile set, and the constraint indicator flags from
  // H.265 Table A.4 signalling 10 bit 4:4:4 support.
  H265SPS sps = {};
  sps.sps_video_parameter_set_id = 0;
  sps.sps_max_sub_layers_minus1 = 0;
  sps.sps_temporal_id_nesting_flag = true;
  sps.profile_tier_level.general_profile_idc = 4;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x08000000;
  sps.profile_tier_level.general_progressive_source_flag = true;
  sps.profile_tier_level.general_interlaced_source_flag = false;
  sps.profile_tier_level.general_non_packed_constraint_flag = true;
  sps.profile_tier_level.general_frame_only_constraint_flag = true;
  sps.profile_tier_level.general_max_12bit_constraint_flag = false;
  sps.profile_tier_level.general_max_10bit_constraint_flag = true;
  sps.profile_tier_level.general_max_8bit_constraint_flag = false;
  sps.profile_tier_level.general_max_422chroma_constraint_flag = false;
  sps.profile_tier_level.general_max_420chroma_constraint_flag = false;
  sps.profile_tier_level.general_max_monochrome_constraint_flag = false;
  sps.profile_tier_level.general_intra_constraint_flag = false;
  sps.profile_tier_level.general_one_picture_only_constraint_flag = false;
  sps.profile_tier_level.general_lower_bit_rate_constraint_flag = true;
  sps.profile_tier_level.general_max_14bit_constraint_flag = false;
  sps.profile_tier_level.general_level_idc = 120;
  sps.sps_seq_parameter_set_id = 0;
  sps.chroma_format_idc = 3;
  sps.pic_width_in_luma_samples = 1920;
  sps.pic_height_in_luma_samples = 1080;
  sps.bit_depth_luma_minus8 = 2;
  sps.bit_depth_chroma_minus8 = 2;
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
  sps.amp_enabled_flag = false;
  sps.sample_adaptive_offset_enabled_flag = false;
  sps.long_term_ref_pics_present_flag = false;
  sps.sps_temporal_mvp_enabled_flag = false;
  sps.strong_intra_smoothing_enabled_flag = false;

  H26xAnnexBBitstreamBuilder builder;
  BuildPackedH265SPS(builder, sps);
  builder.Flush();

  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);

  int sps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSPS(&sps_id));
  const H265SPS* parsed_sps = parser_->GetSPS(sps_id);
  ASSERT_TRUE(parsed_sps);

  EXPECT_EQ(sps.chroma_format_idc, parsed_sps->chroma_format_idc);
  EXPECT_EQ(sps.bit_depth_luma_minus8, parsed_sps->bit_depth_luma_minus8);
  EXPECT_EQ(sps.bit_depth_chroma_minus8, parsed_sps->bit_depth_chroma_minus8);
  const H265ProfileTierLevel& parsed_ptl = parsed_sps->profile_tier_level;
  EXPECT_EQ(sps.profile_tier_level.general_profile_idc,
            parsed_ptl.general_profile_idc);
  EXPECT_EQ(sps.profile_tier_level.general_profile_compatibility_flags,
            parsed_ptl.general_profile_compatibility_flags);
  EXPECT_EQ(sps.profile_tier_level.general_max_12bit_constraint_flag,
            parsed_ptl.general_max_12bit_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_max_10bit_constraint_flag,
            parsed_ptl.general_max_10bit_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_max_8bit_constraint_flag,
            parsed_ptl.general_max_8bit_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_max_422chroma_constraint_flag,
            parsed_ptl.general_max_422chroma_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_max_420chroma_constraint_flag,
            parsed_ptl.general_max_420chroma_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_max_monochrome_constraint_flag,
            parsed_ptl.general_max_monochrome_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_intra_constraint_flag,
            parsed_ptl.general_intra_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_one_picture_only_constraint_flag,
            parsed_ptl.general_one_picture_only_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_lower_bit_rate_constraint_flag,
            parsed_ptl.general_lower_bit_rate_constraint_flag);
  EXPECT_EQ(sps.profile_tier_level.general_max_14bit_constraint_flag,
            parsed_ptl.general_max_14bit_constraint_flag);
}

TEST_F(H265BuilderTest, BuildSPSRangeExtension) {
  // Main 4:4:4 10 SPS carrying the sps_range_extension() syntax with a mix of
  // enabled and disabled flags, to catch any ordering error in the round trip.
  H265SPS sps = {};
  sps.sps_video_parameter_set_id = 0;
  sps.sps_max_sub_layers_minus1 = 0;
  sps.sps_temporal_id_nesting_flag = true;
  sps.profile_tier_level.general_profile_idc = 4;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x08000000;
  sps.profile_tier_level.general_progressive_source_flag = true;
  sps.profile_tier_level.general_non_packed_constraint_flag = true;
  sps.profile_tier_level.general_frame_only_constraint_flag = true;
  sps.profile_tier_level.general_max_10bit_constraint_flag = true;
  sps.profile_tier_level.general_lower_bit_rate_constraint_flag = true;
  sps.profile_tier_level.general_level_idc = 120;
  sps.sps_seq_parameter_set_id = 0;
  sps.chroma_format_idc = 3;
  sps.pic_width_in_luma_samples = 1920;
  sps.pic_height_in_luma_samples = 1080;
  sps.bit_depth_luma_minus8 = 2;
  sps.bit_depth_chroma_minus8 = 2;
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.sps_max_dec_pic_buffering_minus1[0] = 1;
  sps.log2_diff_max_min_luma_coding_block_size = 3;
  sps.log2_diff_max_min_luma_transform_block_size = 3;

  sps.sps_extension_present_flag = true;
  sps.sps_range_extension_flag = true;
  sps.transform_skip_rotation_enabled_flag = true;
  sps.transform_skip_context_enabled_flag = false;
  sps.implicit_rdpcm_enabled_flag = true;
  sps.explicit_rdpcm_enabled_flag = false;
  sps.extended_precision_processing_flag = false;
  sps.intra_smoothing_disabled_flag = true;
  sps.high_precision_offsets_enabled_flag = false;
  sps.persistent_rice_adaptation_enabled_flag = true;
  sps.cabac_bypass_alignment_enabled_flag = false;

  H26xAnnexBBitstreamBuilder builder;
  BuildPackedH265SPS(builder, sps);
  builder.Flush();

  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);
  int sps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSPS(&sps_id));
  const H265SPS* parsed = parser_->GetSPS(sps_id);
  ASSERT_TRUE(parsed);

  EXPECT_TRUE(parsed->sps_range_extension_flag);
  EXPECT_EQ(sps.transform_skip_rotation_enabled_flag,
            parsed->transform_skip_rotation_enabled_flag);
  EXPECT_EQ(sps.transform_skip_context_enabled_flag,
            parsed->transform_skip_context_enabled_flag);
  EXPECT_EQ(sps.implicit_rdpcm_enabled_flag,
            parsed->implicit_rdpcm_enabled_flag);
  EXPECT_EQ(sps.explicit_rdpcm_enabled_flag,
            parsed->explicit_rdpcm_enabled_flag);
  EXPECT_EQ(sps.extended_precision_processing_flag,
            parsed->extended_precision_processing_flag);
  EXPECT_EQ(sps.intra_smoothing_disabled_flag,
            parsed->intra_smoothing_disabled_flag);
  EXPECT_EQ(sps.high_precision_offsets_enabled_flag,
            parsed->high_precision_offsets_enabled_flag);
  EXPECT_EQ(sps.persistent_rice_adaptation_enabled_flag,
            parsed->persistent_rice_adaptation_enabled_flag);
  EXPECT_EQ(sps.cabac_bypass_alignment_enabled_flag,
            parsed->cabac_bypass_alignment_enabled_flag);
}

TEST_F(H265BuilderTest, BuildPPSRangeExtension) {
  // A 12-bit 4:4:4 SPS so the PPS can carry a non-zero log2_sao_offset_scale
  // (which the parser bounds by bit_depth - 2).
  H265SPS sps = {};
  sps.sps_temporal_id_nesting_flag = true;
  sps.profile_tier_level.general_profile_idc = 4;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x08000000;
  sps.profile_tier_level.general_progressive_source_flag = true;
  sps.profile_tier_level.general_non_packed_constraint_flag = true;
  sps.profile_tier_level.general_frame_only_constraint_flag = true;
  sps.profile_tier_level.general_max_12bit_constraint_flag = true;
  sps.profile_tier_level.general_lower_bit_rate_constraint_flag = true;
  sps.profile_tier_level.general_level_idc = 120;
  sps.chroma_format_idc = 3;
  sps.pic_width_in_luma_samples = 1920;
  sps.pic_height_in_luma_samples = 1080;
  sps.bit_depth_luma_minus8 = 4;
  sps.bit_depth_chroma_minus8 = 4;
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.sps_max_dec_pic_buffering_minus1[0] = 1;
  sps.log2_diff_max_min_luma_coding_block_size = 3;
  sps.log2_diff_max_min_luma_transform_block_size = 3;

  H265PPS pps = {};
  pps.pps_pic_parameter_set_id = 0;
  pps.pps_seq_parameter_set_id = 0;
  pps.transform_skip_enabled_flag = true;
  pps.pps_extension_present_flag = true;
  pps.pps_range_extension_flag = true;
  pps.log2_max_transform_skip_block_size_minus2 = 2;
  pps.chroma_qp_offset_list_enabled_flag = true;
  pps.diff_cu_chroma_qp_offset_depth = 1;
  pps.chroma_qp_offset_list_len_minus1 = 1;
  pps.cb_qp_offset_list[0] = -5;
  pps.cr_qp_offset_list[0] = 7;
  pps.cb_qp_offset_list[1] = 12;
  pps.cr_qp_offset_list[1] = -12;
  pps.log2_sao_offset_scale_luma = 1;
  pps.log2_sao_offset_scale_chroma = 2;

  H26xAnnexBBitstreamBuilder builder;
  BuildPackedH265SPS(builder, sps);
  BuildPackedH265PPS(builder, pps);
  builder.Flush();

  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  ASSERT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);
  int sps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSPS(&sps_id));

  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  ASSERT_EQ(H265NALU::PPS_NUT, nalu.nal_unit_type);
  int pps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParsePPS(nalu, &pps_id));
  const H265PPS* parsed = parser_->GetPPS(pps_id);
  ASSERT_TRUE(parsed);

  EXPECT_TRUE(parsed->pps_range_extension_flag);
  EXPECT_EQ(pps.log2_max_transform_skip_block_size_minus2,
            parsed->log2_max_transform_skip_block_size_minus2);
  EXPECT_TRUE(parsed->chroma_qp_offset_list_enabled_flag);
  EXPECT_EQ(pps.diff_cu_chroma_qp_offset_depth,
            parsed->diff_cu_chroma_qp_offset_depth);
  EXPECT_EQ(pps.chroma_qp_offset_list_len_minus1,
            parsed->chroma_qp_offset_list_len_minus1);
  EXPECT_EQ(pps.cb_qp_offset_list[0], parsed->cb_qp_offset_list[0]);
  EXPECT_EQ(pps.cr_qp_offset_list[0], parsed->cr_qp_offset_list[0]);
  EXPECT_EQ(pps.cb_qp_offset_list[1], parsed->cb_qp_offset_list[1]);
  EXPECT_EQ(pps.cr_qp_offset_list[1], parsed->cr_qp_offset_list[1]);
  EXPECT_EQ(pps.log2_sao_offset_scale_luma, parsed->log2_sao_offset_scale_luma);
  EXPECT_EQ(pps.log2_sao_offset_scale_chroma,
            parsed->log2_sao_offset_scale_chroma);
}

TEST_F(H265BuilderTest, BuildSPSRangeExtensionInfersExtensionPresentFlag) {
  // sps_range_extension() lives inside sps_extension_data(), so the builder
  // must infer sps_extension_present_flag from sps_range_extension_flag rather
  // than emitting the range extension payload after extension_present = 0,
  // which would desync conformant parsers.
  H265SPS sps = {};
  sps.sps_video_parameter_set_id = 0;
  sps.sps_max_sub_layers_minus1 = 0;
  sps.sps_temporal_id_nesting_flag = true;
  sps.profile_tier_level.general_profile_idc = 4;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x08000000;
  sps.profile_tier_level.general_progressive_source_flag = true;
  sps.profile_tier_level.general_non_packed_constraint_flag = true;
  sps.profile_tier_level.general_frame_only_constraint_flag = true;
  sps.profile_tier_level.general_max_10bit_constraint_flag = true;
  sps.profile_tier_level.general_lower_bit_rate_constraint_flag = true;
  sps.profile_tier_level.general_level_idc = 120;
  sps.sps_seq_parameter_set_id = 0;
  sps.chroma_format_idc = 3;
  sps.pic_width_in_luma_samples = 1920;
  sps.pic_height_in_luma_samples = 1080;
  sps.bit_depth_luma_minus8 = 2;
  sps.bit_depth_chroma_minus8 = 2;
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.sps_max_dec_pic_buffering_minus1[0] = 1;
  sps.log2_diff_max_min_luma_coding_block_size = 3;
  sps.log2_diff_max_min_luma_transform_block_size = 3;

  // When sps_range_extension_flag is set, we will ignore infer the
  // sps_extension_present_flag to true internally.
  sps.sps_extension_present_flag = false;
  sps.sps_range_extension_flag = true;
  sps.transform_skip_rotation_enabled_flag = true;
  sps.transform_skip_context_enabled_flag = false;
  sps.implicit_rdpcm_enabled_flag = true;
  sps.explicit_rdpcm_enabled_flag = false;
  sps.extended_precision_processing_flag = false;
  sps.intra_smoothing_disabled_flag = true;
  sps.high_precision_offsets_enabled_flag = false;
  sps.persistent_rice_adaptation_enabled_flag = true;
  sps.cabac_bypass_alignment_enabled_flag = false;

  H26xAnnexBBitstreamBuilder builder;
  BuildPackedH265SPS(builder, sps);
  builder.Flush();

  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  EXPECT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);
  int sps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSPS(&sps_id));
  const H265SPS* parsed = parser_->GetSPS(sps_id);
  ASSERT_TRUE(parsed);

  // The range extension payload must be preceded by a set
  // sps_extension_present_flag, so the whole extension round-trips.
  EXPECT_TRUE(parsed->sps_extension_present_flag);
  EXPECT_TRUE(parsed->sps_range_extension_flag);
  EXPECT_EQ(sps.transform_skip_rotation_enabled_flag,
            parsed->transform_skip_rotation_enabled_flag);
  EXPECT_EQ(sps.transform_skip_context_enabled_flag,
            parsed->transform_skip_context_enabled_flag);
  EXPECT_EQ(sps.implicit_rdpcm_enabled_flag,
            parsed->implicit_rdpcm_enabled_flag);
  EXPECT_EQ(sps.explicit_rdpcm_enabled_flag,
            parsed->explicit_rdpcm_enabled_flag);
  EXPECT_EQ(sps.extended_precision_processing_flag,
            parsed->extended_precision_processing_flag);
  EXPECT_EQ(sps.intra_smoothing_disabled_flag,
            parsed->intra_smoothing_disabled_flag);
  EXPECT_EQ(sps.high_precision_offsets_enabled_flag,
            parsed->high_precision_offsets_enabled_flag);
  EXPECT_EQ(sps.persistent_rice_adaptation_enabled_flag,
            parsed->persistent_rice_adaptation_enabled_flag);
  EXPECT_EQ(sps.cabac_bypass_alignment_enabled_flag,
            parsed->cabac_bypass_alignment_enabled_flag);
}

TEST_F(H265BuilderTest, BuildPPSRangeExtensionInfersExtensionPresentFlag) {
  // Mirror of BuildPPSRangeExtension for the PPS: pps_range_extension() lives
  // inside pps_extension_data(), so pps_extension_present_flag must be inferred
  // from pps_range_extension_flag instead of emitting the payload after
  // extension_present = 0.
  H265SPS sps = {};
  sps.sps_temporal_id_nesting_flag = true;
  sps.profile_tier_level.general_profile_idc = 4;
  sps.profile_tier_level.general_profile_compatibility_flags = 0x08000000;
  sps.profile_tier_level.general_progressive_source_flag = true;
  sps.profile_tier_level.general_non_packed_constraint_flag = true;
  sps.profile_tier_level.general_frame_only_constraint_flag = true;
  sps.profile_tier_level.general_max_12bit_constraint_flag = true;
  sps.profile_tier_level.general_lower_bit_rate_constraint_flag = true;
  sps.profile_tier_level.general_level_idc = 120;
  sps.chroma_format_idc = 3;
  sps.pic_width_in_luma_samples = 1920;
  sps.pic_height_in_luma_samples = 1080;
  sps.bit_depth_luma_minus8 = 4;
  sps.bit_depth_chroma_minus8 = 4;
  sps.log2_max_pic_order_cnt_lsb_minus4 = 4;
  sps.sps_max_dec_pic_buffering_minus1[0] = 1;
  sps.log2_diff_max_min_luma_coding_block_size = 3;
  sps.log2_diff_max_min_luma_transform_block_size = 3;

  H265PPS pps = {};
  pps.pps_pic_parameter_set_id = 0;
  pps.pps_seq_parameter_set_id = 0;
  pps.transform_skip_enabled_flag = true;

  // When pps_range_extension_flag is set, we will infer the
  // pps_extension_present_flag to true internally.
  pps.pps_extension_present_flag = false;
  pps.pps_range_extension_flag = true;
  pps.log2_max_transform_skip_block_size_minus2 = 2;
  pps.chroma_qp_offset_list_enabled_flag = true;
  pps.diff_cu_chroma_qp_offset_depth = 1;
  pps.chroma_qp_offset_list_len_minus1 = 1;
  pps.cb_qp_offset_list[0] = -5;
  pps.cr_qp_offset_list[0] = 7;
  pps.cb_qp_offset_list[1] = 12;
  pps.cr_qp_offset_list[1] = -12;
  pps.log2_sao_offset_scale_luma = 1;
  pps.log2_sao_offset_scale_chroma = 2;

  H26xAnnexBBitstreamBuilder builder;
  BuildPackedH265SPS(builder, sps);
  BuildPackedH265PPS(builder, pps);
  builder.Flush();

  parser_->SetStream(builder.data());
  H265NALU nalu;
  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  ASSERT_EQ(H265NALU::SPS_NUT, nalu.nal_unit_type);
  int sps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParseSPS(&sps_id));

  ASSERT_EQ(H265Parser::kOk, parser_->AdvanceToNextNALU(&nalu));
  ASSERT_EQ(H265NALU::PPS_NUT, nalu.nal_unit_type);
  int pps_id;
  ASSERT_EQ(H265Parser::kOk, parser_->ParsePPS(nalu, &pps_id));
  const H265PPS* parsed = parser_->GetPPS(pps_id);
  ASSERT_TRUE(parsed);

  // The range extension payload must be preceded by a set
  // pps_extension_present_flag, so the whole extension round-trips.
  EXPECT_TRUE(parsed->pps_extension_present_flag);
  EXPECT_TRUE(parsed->pps_range_extension_flag);
  EXPECT_EQ(pps.log2_max_transform_skip_block_size_minus2,
            parsed->log2_max_transform_skip_block_size_minus2);
  EXPECT_TRUE(parsed->chroma_qp_offset_list_enabled_flag);
  EXPECT_EQ(pps.diff_cu_chroma_qp_offset_depth,
            parsed->diff_cu_chroma_qp_offset_depth);
  EXPECT_EQ(pps.chroma_qp_offset_list_len_minus1,
            parsed->chroma_qp_offset_list_len_minus1);
  EXPECT_EQ(pps.cb_qp_offset_list[0], parsed->cb_qp_offset_list[0]);
  EXPECT_EQ(pps.cr_qp_offset_list[0], parsed->cr_qp_offset_list[0]);
  EXPECT_EQ(pps.cb_qp_offset_list[1], parsed->cb_qp_offset_list[1]);
  EXPECT_EQ(pps.cr_qp_offset_list[1], parsed->cr_qp_offset_list[1]);
  EXPECT_EQ(pps.log2_sao_offset_scale_luma, parsed->log2_sao_offset_scale_luma);
  EXPECT_EQ(pps.log2_sao_offset_scale_chroma,
            parsed->log2_sao_offset_scale_chroma);
}

}  // namespace media
