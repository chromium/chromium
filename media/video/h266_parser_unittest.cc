// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/video/h266_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
struct VvcTestData {
  std::string file_name;
  // Number of NALUs in the test stream to be parsed.
  int num_nalus;
};

}  // namespace

class H266ParserTest : public ::testing::Test {
 protected:
  void LoadParserFile(std::string file_name) {
    parser_.Reset();
    base::FilePath file_path = GetTestDataFilePath(file_name);
    stream_ = std::make_unique<base::MemoryMappedFile>();
    ASSERT_TRUE(stream_->Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();
    parser_.SetStream(stream_->data(), stream_->length());
  }
  bool ParseNalusUntilNut(H266NALU* target_nalu, H266NALU::Type nalu_type) {
    while (true) {
      H266Parser::Result res = parser_.AdvanceToNextNALU(target_nalu);
      if (res == H266Parser::kEndOfStream) {
        return false;
      }
      EXPECT_EQ(res, H266Parser::kOk);
      if (target_nalu->nal_unit_type == nalu_type) {
        return true;
      }
    }
  }
  H266Parser parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;
};

TEST_F(H266ParserTest, RawVvcStreamFileParsingShouldSucceed) {
  VvcTestData test_data[] = {{"bear_180p.vvc", 54},
                             {"bbb_360p.vvc", 87},
                             {"basketball_2_layers.vvc", 48}};
  for (const auto& data : test_data) {
    LoadParserFile(data.file_name);
    // Parse until the end of stream/unsupported stream/error in stream is
    // found.
    int num_parsed_nalus = 0;
    while (true) {
      H266NALU nalu;
      H266Parser::Result res = parser_.AdvanceToNextNALU(&nalu);
      if (res == H266Parser::kEndOfStream) {
        DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
                 << num_parsed_nalus;
        EXPECT_EQ(data.num_nalus, num_parsed_nalus);
        break;
      }
      EXPECT_EQ(res, H266Parser::kOk);
      ++num_parsed_nalus;
      DVLOG(4) << "Found NALU " << nalu.nal_unit_type;
      switch (nalu.nal_unit_type) {
        case H266NALU::kVPS:
          int vps_id;
          res = parser_.ParseVPS(&vps_id);
          EXPECT_TRUE(!!parser_.GetVPS(vps_id));
          break;
        case H266NALU::kSPS:
          int sps_id;
          res = parser_.ParseSPS(nalu, &sps_id);
          EXPECT_TRUE(!!parser_.GetSPS(sps_id));
          break;
        // TODO(crbugs.com/1417910): add more NALU types.
        default:
          break;
      }
      EXPECT_EQ(res, H266Parser::kOk);
    }
  }
}

TEST_F(H266ParserTest, VpsWithTwolayersParsingShouldGetCorrectSyntaxValues) {
  LoadParserFile("basketball_2_layers.vvc");
  H266NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kVPS));
  int vps_id;
  EXPECT_EQ(H266Parser::kOk, parser_.ParseVPS(&vps_id));
  const H266VPS* vps = parser_.GetVPS(vps_id);
  EXPECT_TRUE(!!vps);
  EXPECT_EQ(vps->vps_video_parameter_set_id, 1);
  EXPECT_EQ(vps->vps_max_layers_minus1, 1);
  EXPECT_EQ(vps->vps_max_sublayers_minus1, 0);
  for (int i = 0; i <= vps->vps_max_layers_minus1; i++) {
    EXPECT_EQ(vps->vps_layer_id[i], i);
  }
  EXPECT_TRUE(vps->vps_direct_ref_layer_flag[1][0]);
  EXPECT_EQ(vps->vps_ols_mode_idc, 2);
  EXPECT_TRUE(vps->vps_ols_output_layer_flag[1][0]);
  EXPECT_TRUE(vps->vps_ols_output_layer_flag[1][1]);
  EXPECT_EQ(vps->vps_num_ptls_minus1, 1);
  EXPECT_TRUE(vps->vps_pt_present_flag[0]);
  // vps->vps_pt_present_flag[1] = 0, so profile_tier_level[1] should
  // be copied from profile_tier_level[0].
  for (int i = 0; i <= vps->vps_num_ptls_minus1; i++) {
    EXPECT_EQ(vps->profile_tier_level[i].general_profile_idc, 17);
    EXPECT_EQ(vps->profile_tier_level[i].general_level_idc, 51);
    EXPECT_TRUE(vps->profile_tier_level[i].ptl_frame_only_constraint_flag);
  }
  EXPECT_EQ(vps->vps_num_dpb_params_minus1, 0);
  EXPECT_EQ(vps->dpb_parameters[0].dpb_max_dec_pic_buffering_minus1[0], 9);
  EXPECT_EQ(vps->dpb_parameters[0].dpb_max_num_reorder_pics[0], 9);
  EXPECT_EQ(vps->dpb_parameters[0].dpb_max_latency_increase_plus1[0], 0);
  EXPECT_EQ(vps->vps_ols_dpb_pic_width[0], 832);
  EXPECT_EQ(vps->vps_ols_dpb_pic_height[0], 480);
  EXPECT_EQ(vps->vps_ols_dpb_chroma_format[0], 1);
  EXPECT_EQ(vps->vps_ols_dpb_bitdepth_minus8[0], 2);
}

TEST_F(H266ParserTest, GetVPSForStreamWithoutVPSShouldReturnNull) {
  LoadParserFile("bear_180p.vvc");
  H266NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!vps);
}

TEST_F(H266ParserTest, GetVPSWithoutVPSParsingShouldReturnNull) {
  LoadParserFile("basketball_2_layers.vvc");
  H266NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  const H266VPS* vps = parser_.GetVPS(1);
  EXPECT_TRUE(!vps);
}

TEST_F(H266ParserTest, ParseSPSWithoutVPSInStreamShouldGetCorrectSyntaxValues) {
  LoadParserFile("bear_180p.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  EXPECT_EQ(sps->sps_seq_parameter_set_id, 0);
  EXPECT_EQ(sps->sps_video_parameter_set_id, 0);
  EXPECT_EQ(sps->sps_max_sublayers_minus1, 5);
  EXPECT_EQ(sps->sps_chroma_format_idc, 1);
  EXPECT_EQ(sps->sps_log2_ctu_size_minus5, 2);
  EXPECT_TRUE(sps->sps_ptl_dpb_hrd_params_present_flag);
  EXPECT_EQ(sps->profile_tier_level.general_profile_idc, 1);
  EXPECT_EQ(sps->profile_tier_level.general_tier_flag, 0);
  EXPECT_EQ(sps->profile_tier_level.general_level_idc, 32);
  EXPECT_TRUE(sps->profile_tier_level.ptl_frame_only_constraint_flag);
  EXPECT_FALSE(sps->profile_tier_level.ptl_multilayer_enabled_flag);
  EXPECT_FALSE(
      sps->profile_tier_level.general_constraints_info.gci_present_flag);
  for (int i = 0; i <= 4; i++) {
    EXPECT_FALSE(sps->profile_tier_level.ptl_sublayer_level_present_flag[i]);
  }
  EXPECT_EQ(sps->profile_tier_level.ptl_num_sub_profiles, 0);
  EXPECT_TRUE(sps->sps_gdr_enabled_flag);
  EXPECT_FALSE(sps->sps_ref_pic_resampling_enabled_flag);
  EXPECT_EQ(sps->sps_pic_width_max_in_luma_samples, 320);
  EXPECT_EQ(sps->sps_pic_height_max_in_luma_samples, 184);
  EXPECT_TRUE(sps->sps_conformance_window_flag);
  EXPECT_EQ(sps->sps_conf_win_left_offset, 0);
  EXPECT_EQ(sps->sps_conf_win_right_offset, 0);
  EXPECT_EQ(sps->sps_conf_win_top_offset, 0);
  EXPECT_EQ(sps->sps_conf_win_bottom_offset, 2);
  EXPECT_FALSE(sps->sps_subpic_info_present_flag);
  EXPECT_EQ(sps->sps_bitdepth_minus8, 2);
  EXPECT_FALSE(sps->sps_entropy_coding_sync_enabled_flag);
  EXPECT_TRUE(sps->sps_entry_point_offsets_present_flag);
  EXPECT_EQ(sps->sps_log2_max_pic_order_cnt_lsb_minus4, 4);
  EXPECT_FALSE(sps->sps_poc_msb_cycle_flag);
  EXPECT_EQ(sps->sps_num_extra_ph_bytes, 0);
  EXPECT_EQ(sps->sps_num_extra_sh_bytes, 0);
  EXPECT_FALSE(sps->sps_sublayer_dpb_params_flag);
  EXPECT_EQ(sps->dpb_params.dpb_max_dec_pic_buffering_minus1[5], 6);
  EXPECT_EQ(sps->dpb_params.dpb_max_num_reorder_pics[5], 5);
  EXPECT_EQ(sps->dpb_params.dpb_max_latency_increase_plus1[5], 0);
  EXPECT_EQ(sps->sps_log2_min_luma_coding_block_size_minus2, 0);
  EXPECT_TRUE(sps->sps_partition_constraints_override_enabled_flag);
  EXPECT_EQ(sps->sps_log2_diff_min_qt_min_cb_intra_slice_luma, 1);
  EXPECT_EQ(sps->sps_max_mtt_hierarchy_depth_intra_slice_luma, 2);
  EXPECT_EQ(sps->sps_log2_diff_max_bt_min_qt_intra_slice_luma, 2);
  EXPECT_EQ(sps->sps_log2_diff_max_tt_min_qt_intra_slice_luma, 2);
  EXPECT_TRUE(sps->sps_qtbtt_dual_tree_intra_flag);
  EXPECT_EQ(sps->sps_log2_diff_min_qt_min_cb_intra_slice_chroma, 2);
  EXPECT_EQ(sps->sps_max_mtt_hierarchy_depth_intra_slice_chroma, 2);
  EXPECT_EQ(sps->sps_log2_diff_max_bt_min_qt_intra_slice_chroma, 2);
  EXPECT_EQ(sps->sps_log2_diff_max_tt_min_qt_intra_slice_chroma, 1);
  EXPECT_EQ(sps->sps_log2_diff_min_qt_min_cb_inter_slice, 1);
  EXPECT_EQ(sps->sps_max_mtt_hierarchy_depth_inter_slice, 3);
  EXPECT_EQ(sps->sps_log2_diff_max_bt_min_qt_inter_slice, 4);
  EXPECT_EQ(sps->sps_log2_diff_max_tt_min_qt_inter_slice, 3);
  EXPECT_TRUE(sps->sps_max_luma_transform_size_64_flag);
  EXPECT_TRUE(sps->sps_transform_skip_enabled_flag);
  EXPECT_EQ(sps->sps_log2_transform_skip_max_size_minus2, 2);
  EXPECT_TRUE(sps->sps_bdpcm_enabled_flag);
  EXPECT_TRUE(sps->sps_mts_enabled_flag);
  EXPECT_FALSE(sps->sps_explicit_mts_intra_enabled_flag);
  EXPECT_FALSE(sps->sps_explicit_mts_inter_enabled_flag);
  EXPECT_TRUE(sps->sps_lfnst_enabled_flag);
  EXPECT_TRUE(sps->sps_joint_cbcr_enabled_flag);
  EXPECT_TRUE(sps->sps_same_qp_table_for_chroma_flag);
  EXPECT_EQ(sps->sps_qp_table_start_minus26[0], -9);
  EXPECT_EQ(sps->sps_num_points_in_qp_table_minus1[0], 2);
  EXPECT_EQ(sps->sps_delta_qp_in_val_minus1[0][0], 4);
  EXPECT_EQ(sps->sps_delta_qp_diff_val[0][0], 2);
  EXPECT_EQ(sps->sps_delta_qp_in_val_minus1[0][1], 11);
  EXPECT_EQ(sps->sps_delta_qp_diff_val[0][1], 7);
  EXPECT_EQ(sps->sps_delta_qp_in_val_minus1[0][2], 7);
  EXPECT_EQ(sps->sps_delta_qp_diff_val[0][2], 3);
  EXPECT_TRUE(sps->sps_sao_enabled_flag);
  EXPECT_TRUE(sps->sps_alf_enabled_flag);
  EXPECT_TRUE(sps->sps_ccalf_enabled_flag);
  EXPECT_EQ(sps->sps_num_ref_pic_lists[0], 37);
  EXPECT_TRUE(sps->sps_temporal_mvp_enabled_flag);
  EXPECT_TRUE(sps->sps_sbtmvp_enabled_flag);
  EXPECT_TRUE(sps->sps_amvr_enabled_flag);
  EXPECT_TRUE(sps->sps_bdof_enabled_flag);
  EXPECT_TRUE(sps->sps_bdof_control_present_in_ph_flag);
  EXPECT_TRUE(sps->sps_smvd_enabled_flag);
  EXPECT_TRUE(sps->sps_dmvr_enabled_flag);
  EXPECT_TRUE(sps->sps_dmvr_control_present_in_ph_flag);
  EXPECT_TRUE(sps->sps_mmvd_enabled_flag);
  EXPECT_TRUE(sps->sps_mmvd_fullpel_only_enabled_flag);
  EXPECT_TRUE(sps->sps_affine_enabled_flag);
  EXPECT_EQ(sps->sps_five_minus_max_num_subblock_merge_cand, 0);
  EXPECT_TRUE(sps->sps_6param_affine_enabled_flag);
  EXPECT_FALSE(sps->sps_affine_amvr_enabled_flag);
  EXPECT_TRUE(sps->sps_affine_prof_enabled_flag);
  EXPECT_TRUE(sps->sps_prof_control_present_in_ph_flag);
  EXPECT_FALSE(sps->sps_bcw_enabled_flag);
  EXPECT_FALSE(sps->sps_ciip_enabled_flag);
  EXPECT_TRUE(sps->sps_gpm_enabled_flag);
  EXPECT_EQ(sps->sps_max_num_merge_cand_minus_max_num_gpm_cand, 1);
  EXPECT_EQ(sps->sps_log2_parallel_merge_level_minus2, 0);
  EXPECT_TRUE(sps->sps_isp_enabled_flag);
  EXPECT_TRUE(sps->sps_mrl_enabled_flag);
  EXPECT_TRUE(sps->sps_cclm_enabled_flag);
  EXPECT_TRUE(sps->sps_chroma_horizontal_collocated_flag);
  EXPECT_FALSE(sps->sps_palette_enabled_flag);
  EXPECT_EQ(sps->sps_min_qp_prime_ts, 2);
  EXPECT_TRUE(sps->sps_ibc_enabled_flag);
  EXPECT_FALSE(sps->sps_explicit_scaling_list_enabled_flag);
  EXPECT_TRUE(sps->sps_dep_quant_enabled_flag);
  EXPECT_FALSE(sps->sps_virtual_boundaries_enabled_flag);
  EXPECT_TRUE(sps->sps_timing_hrd_params_present_flag);

  // General timing HRD params.
  EXPECT_EQ(sps->general_timing_hrd_parameters.num_units_in_tick, 1u);
  EXPECT_EQ(sps->general_timing_hrd_parameters.time_scale, 15u);
  EXPECT_TRUE(
      sps->general_timing_hrd_parameters.general_nal_hrd_params_present_flag);
  EXPECT_TRUE(
      sps->general_timing_hrd_parameters.general_vcl_hrd_params_present_flag);
  EXPECT_TRUE(sps->general_timing_hrd_parameters
                  .general_same_pic_timing_in_all_ols_flag);
  EXPECT_FALSE(
      sps->general_timing_hrd_parameters.general_du_hrd_params_present_flag);
  EXPECT_EQ(sps->general_timing_hrd_parameters.bit_rate_scale, 0);
  EXPECT_EQ(sps->general_timing_hrd_parameters.cpb_size_scale, 1);
  EXPECT_EQ(sps->general_timing_hrd_parameters.hrd_cpb_cnt_minus1, 0);
  EXPECT_FALSE(sps->sps_sublayer_cpb_params_present_flag);

  // OLS timing HRD params with sublayer info. This stream is with
  // max_sublayer_minus1 of 5. Since sps_sublayer_cpb_params_present_flag is 0,
  // all ols_timing_hrd_parameters() members for sublayer 0 to 4 should be
  // copied from that of sublayer 5.
  for (int i = 0; i < sps->sps_max_sublayers_minus1; i++) {
    EXPECT_TRUE(sps->ols_timing_hrd_parameters.fixed_pic_rate_general_flag[i]);
    EXPECT_TRUE(
        sps->ols_timing_hrd_parameters.fixed_pic_rate_within_cvs_flag[i]);
    EXPECT_EQ(sps->ols_timing_hrd_parameters.element_duration_in_tc_minus1[i],
              0);
    EXPECT_EQ(sps->ols_timing_hrd_parameters.nal_sublayer_hrd_parameters[i]
                  .bit_rate_value_minus1[0],
              15624);
    EXPECT_EQ(sps->ols_timing_hrd_parameters.nal_sublayer_hrd_parameters[i]
                  .cpb_size_value_minus1[0],
              46874);
    EXPECT_FALSE(sps->ols_timing_hrd_parameters.nal_sublayer_hrd_parameters[i]
                     .cbr_flag[0]);
    EXPECT_EQ(sps->ols_timing_hrd_parameters.vcl_sublayer_hrd_parameters[i]
                  .bit_rate_value_minus1[0],
              15624);
    EXPECT_EQ(sps->ols_timing_hrd_parameters.vcl_sublayer_hrd_parameters[i]
                  .cpb_size_value_minus1[0],
              46874);
    EXPECT_FALSE(sps->ols_timing_hrd_parameters.vcl_sublayer_hrd_parameters[i]
                     .cbr_flag[0]);
  }

  EXPECT_FALSE(sps->sps_field_seq_flag);
  EXPECT_FALSE(sps->sps_vui_parameters_present_flag);
  EXPECT_FALSE(sps->sps_extension_flag);
}

// Verify SPS parsing of bitstreams that contains two layers, as a result
// it has VPS, and also two SPSes with different ids.
TEST_F(H266ParserTest, ParseSPSWithVPSInStreamShouldGetCorrectSyntaxValues) {
  LoadParserFile("basketball_2_layers.vvc");
  H266NALU target_nalu;
  int vps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kVPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseVPS(&vps_id));
  const H266VPS* vps = parser_.GetVPS(vps_id);
  EXPECT_TRUE(!!vps);

  // Go to the first SPS for layer 0.
  int sps_id_layer0;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id_layer0));
  EXPECT_EQ(sps_id_layer0, 0);
  const H266SPS* sps_layer0 = parser_.GetSPS(sps_id_layer0);
  EXPECT_TRUE(!!sps_layer0);
  EXPECT_EQ(sps_layer0->profile_tier_level.general_profile_idc, 17);
  EXPECT_EQ(sps_layer0->profile_tier_level.general_level_idc, 35);
  EXPECT_TRUE(sps_layer0->sps_ref_pic_resampling_enabled_flag);
  EXPECT_EQ(sps_layer0->sps_pic_width_max_in_luma_samples, 208);
  EXPECT_EQ(sps_layer0->sps_pic_height_max_in_luma_samples, 120);

  // Go to the second SPS for layer 1.
  int sps_id_layer1;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id_layer1));
  EXPECT_EQ(sps_id_layer1, 1);
  const H266SPS* sps_layer1 = parser_.GetSPS(sps_id_layer1);
  EXPECT_TRUE(!!sps_layer1);
  EXPECT_EQ(sps_layer1->profile_tier_level.general_profile_idc, 17);
  EXPECT_EQ(sps_layer1->profile_tier_level.general_level_idc, 51);
  EXPECT_TRUE(sps_layer1->sps_ref_pic_resampling_enabled_flag);
  EXPECT_EQ(sps_layer1->sps_pic_width_max_in_luma_samples, 832);
  EXPECT_EQ(sps_layer1->sps_pic_height_max_in_luma_samples, 480);
}

// Verify the SPS parser correctly parses the subpicture info in SPS
// when subpicture sizes are different.
TEST_F(H266ParserTest,
       SPSParserShouldReturnCorrectSyntaxForNonEqualSizeSubPictures) {
  // Manually created SPS with subpicture info in it. Start code included.
  // Subpictures in this stream are of different sizes.
  constexpr uint8_t kStream[] = {
      0x00, 0x00, 0x01, 0x00, 0x79, 0x00, 0x8d, 0x02, 0x43, 0x80, 0x00, 0x00,
      0xc0, 0x07, 0x81, 0x00, 0x21, 0xca, 0x50, 0x96, 0x30, 0x75, 0x81, 0xa8,
      0xab, 0x03, 0x5a, 0xda, 0x08, 0x4d, 0x40, 0x0c, 0x5e, 0x88, 0xdd, 0x10,
      0x8d, 0x10, 0xa4, 0xc8, 0xdc, 0x26, 0xca, 0xc6, 0x08, 0x10, 0x4f, 0x00,
      0x54, 0x81, 0x08, 0x42, 0x20, 0xc4, 0x44, 0x59, 0x22, 0x2d, 0x44, 0x5e,
      0x8f, 0x56, 0xa4, 0xbc, 0x92, 0x6a, 0x4b, 0x24, 0x45, 0xa8, 0x8b, 0xc4,
      0x49, 0xa8, 0x89, 0x14, 0x91, 0x12, 0x64, 0x88, 0x97, 0x52, 0x44, 0x50,
      0x42, 0xc4, 0x42, 0x06, 0x48, 0x83, 0x52, 0x02, 0xac, 0x21, 0x08, 0x58,
      0x80, 0x42, 0xc8, 0x10, 0x22, 0x10, 0x20, 0x59, 0x08, 0x10, 0x24, 0x40,
      0x83, 0x41, 0x02, 0x48, 0x20, 0xe1, 0x06, 0x40, 0x8b, 0x42, 0x09, 0x21,
      0x0e, 0x21, 0xa1, 0x2e, 0x47, 0x2a, 0x08, 0x58, 0x80, 0x42, 0xc8, 0x10,
      0x22, 0x10, 0x20, 0xff, 0xff, 0xfa, 0xfc, 0x62, 0x04,
  };

  H266Parser parser;
  parser.SetStream(kStream, std::size(kStream));
  H266NALU target_nalu;
  ASSERT_EQ(H266Parser::kOk, parser.AdvanceToNextNALU(&target_nalu));
  int sps_id;
  EXPECT_EQ(H266Parser::kOk, parser.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  // Parsed syntax elements.
  EXPECT_TRUE(sps->sps_subpic_info_present_flag);
  EXPECT_EQ(sps->sps_num_subpics_minus1, 4);
  EXPECT_FALSE(sps->sps_independent_subpics_flag);
  EXPECT_FALSE(sps->sps_subpic_same_size_flag);
  EXPECT_EQ(sps->sps_subpic_width_minus1[0], 2);
  EXPECT_EQ(sps->sps_subpic_height_minus1[0], 5);
  EXPECT_TRUE(sps->sps_subpic_treated_as_pic_flag[0]);
  EXPECT_FALSE(sps->sps_loop_filter_across_subpic_enabled_flag[0]);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[1], 3);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[1], 0);
  EXPECT_EQ(sps->sps_subpic_width_minus1[1], 7);
  EXPECT_EQ(sps->sps_subpic_height_minus1[1], 5);
  EXPECT_TRUE(sps->sps_subpic_treated_as_pic_flag[1]);
  EXPECT_FALSE(sps->sps_loop_filter_across_subpic_enabled_flag[1]);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[2], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[2], 6);
  EXPECT_EQ(sps->sps_subpic_width_minus1[2], 10);
  EXPECT_EQ(sps->sps_subpic_height_minus1[2], 2);
  EXPECT_TRUE(sps->sps_subpic_treated_as_pic_flag[2]);
  EXPECT_FALSE(sps->sps_loop_filter_across_subpic_enabled_flag[2]);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[3], 11);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[3], 0);
  EXPECT_EQ(sps->sps_subpic_width_minus1[3], 3);
  EXPECT_EQ(sps->sps_subpic_height_minus1[3], 5);
  EXPECT_TRUE(sps->sps_subpic_treated_as_pic_flag[3]);
  EXPECT_FALSE(sps->sps_loop_filter_across_subpic_enabled_flag[3]);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[4], 11);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[4], 6);
  EXPECT_TRUE(sps->sps_subpic_treated_as_pic_flag[4]);
  EXPECT_FALSE(sps->sps_loop_filter_across_subpic_enabled_flag[4]);
  EXPECT_EQ(sps->sps_subpic_id_len_minus1, 15);
  EXPECT_TRUE(sps->sps_subpic_id_mapping_explicitly_signaled_flag);
  EXPECT_FALSE(sps->sps_subpic_id_mapping_present_flag);

  // Inferred values.
  // First subpicture should always at CTU (0, 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[0], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[0], 0);
  // Last subpciture's width_minus1 and height_minus1 is inferred
  // from its top_left_x/top_left_y.
  EXPECT_EQ(sps->sps_subpic_width_minus1[4], 3);
  EXPECT_EQ(sps->sps_subpic_height_minus1[4], 2);
}

// Verify the SPS parser correctly parses the subpicture info in SPS
// when subpicture sizes are equal.
TEST_F(H266ParserTest,
       SPSParserShouldReturnCorrectSyntaxForEqualSizeSubPictures) {
  // Manually created SPS with subpicture info in it. Start code included.
  // Subpictures in this stream are of same size.
  constexpr uint8_t kStream[] = {
      0x00, 0x00, 0x01, 0x00, 0x79, 0x00, 0xad, 0x02, 0x40, 0x80, 0x00, 0x00,
      0xc0, 0x1a, 0x10, 0x1e, 0x28, 0x84, 0x55, 0x55, 0x33, 0x50, 0x03, 0x9b,
      0xa2, 0x37, 0x44, 0x23, 0x44, 0x29, 0x32, 0x37, 0x09, 0xb2, 0xb1, 0x82,
      0x04, 0x13, 0xc0, 0x09, 0x88, 0x08, 0x21, 0x08, 0x42, 0xc2, 0x10, 0x85,
      0x88, 0x84, 0x2c, 0x90, 0x85, 0xa8, 0x42, 0xf4, 0x7a, 0xb5, 0x25, 0xe4,
      0x93, 0x52, 0x59, 0x22, 0x2d, 0x44, 0x5e, 0x22, 0x4d, 0x44, 0x48, 0xa4,
      0x88, 0x93, 0x24, 0x44, 0xba, 0x92, 0x22, 0xc4, 0x42, 0x16, 0x48, 0x42,
      0xd4, 0x21, 0x78, 0x42, 0x4d, 0x42, 0x12, 0x29, 0x21, 0x09, 0x32, 0x42,
      0x12, 0xea, 0x48, 0x42, 0x42, 0x44, 0x42, 0x12, 0x28, 0x88, 0x42, 0x4c,
      0x44, 0x21, 0x2e, 0xa2, 0x21, 0x09, 0x14, 0x64, 0x21, 0x26, 0x32, 0x10,
      0x97, 0x51, 0x90, 0x85, 0x02, 0x0b, 0x08, 0x41, 0x01, 0x88, 0x84, 0x0c,
      0x91, 0x06, 0xa4, 0x02, 0x66, 0x08, 0x21, 0x0b, 0x08, 0x01, 0x05, 0x88,
      0x04, 0x04, 0x20, 0x40, 0x20, 0x2a, 0x10, 0x20, 0x10, 0x1a, 0x42, 0x04,
      0x02, 0x02, 0xc4, 0x08, 0x04, 0x04, 0x41, 0x00, 0x80, 0xb2, 0x08, 0x04,
      0x04, 0x84, 0x02, 0x06, 0x40, 0x40, 0x44, 0x20, 0x20, 0x2c, 0x84, 0x04,
      0x04, 0x88, 0x08, 0x1a, 0x04, 0x04, 0x90, 0x20, 0x70, 0x40, 0xc4, 0x02,
      0x16, 0x40, 0x81, 0x10, 0x81, 0x02, 0xc8, 0x40, 0x81, 0x22, 0x04, 0x1a,
      0x08, 0x12, 0x41, 0x07, 0x08, 0x32, 0x04, 0x5a, 0x10, 0x49, 0x08, 0x71,
      0x0d, 0x09, 0x72, 0x39, 0x50, 0x20, 0xb0, 0x80, 0x10, 0x58, 0x80, 0x40,
      0x42, 0x04, 0x02, 0x02, 0xa1, 0x02, 0x01, 0x03, 0xff, 0xff, 0xeb, 0xf1,
      0x88, 0x10,
  };
  H266Parser parser;
  parser.SetStream(kStream, std::size(kStream));
  H266NALU target_nalu;
  ASSERT_EQ(H266Parser::kOk, parser.AdvanceToNextNALU(&target_nalu));
  int sps_id;
  EXPECT_EQ(H266Parser::kOk, parser.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  // Parsed syntax values.
  EXPECT_EQ(sps->sps_pic_width_max_in_luma_samples, 416);
  EXPECT_EQ(sps->sps_pic_height_max_in_luma_samples, 240);
  EXPECT_TRUE(sps->sps_subpic_info_present_flag);
  EXPECT_EQ(sps->sps_num_subpics_minus1, 7);
  EXPECT_FALSE(sps->sps_independent_subpics_flag);
  EXPECT_TRUE(sps->sps_subpic_same_size_flag);
  EXPECT_EQ(sps->sps_subpic_width_minus1[0], 0);
  EXPECT_EQ(sps->sps_subpic_height_minus1[0], 0);
  for (int i = 0; i <= sps->sps_num_subpics_minus1; i++) {
    EXPECT_FALSE(sps->sps_loop_filter_across_subpic_enabled_flag[i]);
    EXPECT_TRUE(sps->sps_subpic_treated_as_pic_flag[i]);
  }
  EXPECT_EQ(sps->sps_subpic_id_len_minus1, 2);
  EXPECT_FALSE(sps->sps_subpic_id_mapping_explicitly_signaled_flag);

  // Calculated values or inferred values.
  EXPECT_EQ(sps->ctb_size_y, 128);
  // The entire picture is splitted into 8 subpictures in 4 columns and
  // 2 rows, so each subpicture contains 1 CTU (except those at the last
  // column).
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[0], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[0], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[1], 1);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[1], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[2], 2);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[2], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[3], 3);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[3], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[4], 0);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[4], 1);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[5], 1);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[5], 1);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[6], 2);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[6], 1);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_x[7], 3);
  EXPECT_EQ(sps->sps_subpic_ctu_top_left_y[7], 1);
  for (int i = 1; i <= sps->sps_num_subpics_minus1; i++) {
    EXPECT_EQ(sps->sps_subpic_width_minus1[0], sps->sps_subpic_width_minus1[i]);
    EXPECT_EQ(sps->sps_subpic_height_minus1[0],
              sps->sps_subpic_height_minus1[i]);
  }
}

}  // namespace media
