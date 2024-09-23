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
#include "media/parsers/h266_parser.h"
#include "media/parsers/h266_poc.h"
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
  H266POC poc_;
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
      H266PictureHeader ph;
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
        case H266NALU::kPPS:
          int pps_id;
          res = parser_.ParsePPS(nalu, &pps_id);
          EXPECT_TRUE(!!parser_.GetPPS(pps_id));
          break;
        case H266NALU::kPrefixAPS:
        case H266NALU::kSuffixAPS:
          H266APS::ParamType aps_type;
          int aps_id;
          res = parser_.ParseAPS(nalu, &aps_id, &aps_type);
          EXPECT_TRUE(!!parser_.GetAPS(aps_type, aps_id));
          break;
        case H266NALU::kPH:
          res = parser_.ParsePHNut(nalu, &ph);
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
  // The entire picture is split into 8 subpictures in 4 columns and 2 rows, so
  // each subpicture contains 1 CTU (except those at the last column).
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

TEST_F(H266ParserTest, ParsePPSShouldGetCorrectSyntaxValues) {
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
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  // Verify syntax elements.
  EXPECT_EQ(pps->pps_pic_parameter_set_id, 0);
  EXPECT_EQ(pps->pps_seq_parameter_set_id, 0);
  EXPECT_FALSE(pps->pps_mixed_nalu_types_in_pic_flag);
  EXPECT_EQ(pps->pps_pic_width_in_luma_samples, 320);
  EXPECT_EQ(pps->pps_pic_height_in_luma_samples, 184);
  EXPECT_FALSE(pps->pps_conformance_window_flag);
  EXPECT_EQ(pps->pps_conf_win_left_offset, 0);
  EXPECT_EQ(pps->pps_conf_win_right_offset, 0);
  EXPECT_EQ(pps->pps_conf_win_top_offset, 0);
  // When conformance window flag is false, the value is derived
  // from SPS.
  EXPECT_EQ(pps->pps_conf_win_bottom_offset, 2);
  EXPECT_FALSE(pps->pps_scaling_window_explicit_signaling_flag);
  EXPECT_EQ(pps->pps_scaling_win_left_offset, 0);
  EXPECT_EQ(pps->pps_scaling_win_right_offset, 0);
  EXPECT_EQ(pps->pps_scaling_win_top_offset, 0);
  EXPECT_EQ(pps->pps_scaling_win_bottom_offset, 2);
  EXPECT_FALSE(pps->pps_output_flag_present_flag);
  EXPECT_TRUE(pps->pps_no_pic_partition_flag);
  EXPECT_FALSE(pps->pps_subpic_id_mapping_present_flag);
  EXPECT_EQ(pps->pps_num_subpics_minus1, 0);
  EXPECT_EQ(pps->pps_log2_ctu_size_minus5, 0);
  EXPECT_EQ(pps->pps_num_exp_tile_columns_minus1, 0);
  EXPECT_EQ(pps->pps_num_exp_tile_rows_minus1, 0);
  // This stream is without subpicture/multi-slice, decoder
  // may not overlook this flag. We check this as it may
  // impact existence of other syntax elements.
  EXPECT_TRUE(pps->pps_rect_slice_flag);
  EXPECT_FALSE(pps->pps_cabac_init_present_flag);
  EXPECT_EQ(pps->pps_num_ref_idx_default_active_minus1[0], 1);
  EXPECT_EQ(pps->pps_num_ref_idx_default_active_minus1[1], 1);
  EXPECT_FALSE(pps->pps_rpl1_idx_present_flag);
  EXPECT_FALSE(pps->pps_weighted_pred_flag);
  EXPECT_FALSE(pps->pps_weighted_bipred_flag);
  EXPECT_FALSE(pps->pps_ref_wraparound_enabled_flag);
  EXPECT_EQ(pps->pps_init_qp_minus26, -9);
  EXPECT_TRUE(pps->pps_cu_qp_delta_enabled_flag);
  EXPECT_TRUE(pps->pps_chroma_tool_offsets_present_flag);
  EXPECT_EQ(pps->pps_cb_qp_offset, 0);
  EXPECT_EQ(pps->pps_cr_qp_offset, 0);
  EXPECT_TRUE(pps->pps_joint_cbcr_qp_offset_present_flag);
  EXPECT_EQ(pps->pps_joint_cbcr_qp_offset_value, -1);
  EXPECT_TRUE(pps->pps_slice_chroma_qp_offsets_present_flag);
  EXPECT_FALSE(pps->pps_cu_chroma_qp_offset_list_enabled_flag);
  EXPECT_FALSE(pps->pps_deblocking_filter_control_present_flag);
  EXPECT_FALSE(pps->pps_picture_header_extension_present_flag);
  EXPECT_FALSE(pps->pps_slice_header_extension_present_flag);
  EXPECT_FALSE(pps->pps_extension_flag);
}

// Verify tile layout parsing for stream with multiple tiles and single slice.
TEST_F(H266ParserTest,
       ParsePPSWithMultipleTilesAndSingleSliceShouldGetCorrectSyntaxValues) {
  LoadParserFile("bbb_9tiles.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  EXPECT_EQ(pps->pps_tile_column_width_minus1[0], 4);
  EXPECT_EQ(pps->pps_tile_row_height_minus1[0], 2);
  EXPECT_FALSE(pps->pps_rect_slice_flag);
  EXPECT_EQ(pps->num_tile_columns, 3);
  EXPECT_EQ(pps->num_tile_rows, 3);
}

// Verify tile/slice layout parsing for stream with multiple tiles and
// multiple rect slices.
TEST_F(H266ParserTest,
       ParsePPSWithTileAndSliceSizeEqualShouldGetCorrectSynatxValues) {
  LoadParserFile("bbb_15tiles_15slices.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  for (int i = 0; i < pps->pps_num_slices_in_pic_minus1; i++) {
    EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[i], 0);
    EXPECT_EQ(pps->pps_slice_height_in_tiles_minus1[i], 0);
    EXPECT_EQ(pps->pps_num_exp_slices_in_tile[i], 0);
  }
  EXPECT_TRUE(pps->pps_loop_filter_across_tiles_enabled_flag);
  EXPECT_TRUE(pps->pps_loop_filter_across_slices_enabled_flag);
  EXPECT_TRUE(pps->pps_rect_slice_flag);
  EXPECT_EQ(pps->num_tile_columns, 5);
  EXPECT_EQ(pps->num_tile_rows, 3);
  EXPECT_EQ(pps->pps_num_slices_in_pic_minus1, 14);
}

// Verify tile/slice layout parsing for stream with non-equal tile/slice size
TEST_F(H266ParserTest,
       ParsePPSWithNonEqualTileAndSliceShouldGetCorrectSyntaxValues) {
  LoadParserFile("bbb_9tiles_18slices.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  EXPECT_EQ(pps->pps_tile_column_width_minus1[0], 4);
  EXPECT_EQ(pps->pps_tile_row_height_minus1[0], 2);
  EXPECT_TRUE(pps->pps_loop_filter_across_tiles_enabled_flag);
  EXPECT_TRUE(pps->pps_loop_filter_across_slices_enabled_flag);
  EXPECT_TRUE(pps->pps_rect_slice_flag);
  EXPECT_EQ(pps->num_tile_columns, 3);
  EXPECT_EQ(pps->num_tile_rows, 3);
  EXPECT_EQ(pps->pps_num_slices_in_pic_minus1, 17);
  EXPECT_TRUE(pps->pps_tile_idx_delta_present_flag);
  EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[0], 0);
  EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[0], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[0], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[0][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[1], 3);
  EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[2], 0);
  EXPECT_EQ(pps->pps_slice_height_in_tiles_minus1[2], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[2], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[2][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[3], 3);
  EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[4], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[4], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[4][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[5], -5);
  EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[6], 0);
  EXPECT_EQ(pps->pps_slice_height_in_tiles_minus1[6], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[6], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[6][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[7], 3);
  EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[8], 0);
  EXPECT_EQ(pps->pps_slice_height_in_tiles_minus1[8], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[8], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[8][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[9], 3);
  EXPECT_EQ(pps->pps_slice_width_in_tiles_minus1[10], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[10], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[10][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[11], -5);
  EXPECT_EQ(pps->pps_slice_height_in_tiles_minus1[12], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[12], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[12][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[13], 3);
  EXPECT_EQ(pps->pps_slice_height_in_tiles_minus1[14], 0);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[14], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[14][0], 1);
  EXPECT_EQ(pps->pps_tile_idx_delta_val[15], 3);
  EXPECT_EQ(pps->pps_num_exp_slices_in_tile[16], 1);
  EXPECT_EQ(pps->pps_exp_slice_height_in_ctus_minus1[16][0], 1);
}

// Verify the Cb/Cr QP offset list in PPS.
TEST_F(H266ParserTest, ParsePPSShouldReturnCorrectChromaQPOffsetLists) {
  LoadParserFile("bbb_chroma_qp_offset_lists.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  EXPECT_EQ(pps->pps_init_qp_minus26, 8);
  EXPECT_FALSE(pps->pps_cu_qp_delta_enabled_flag);
  EXPECT_TRUE(pps->pps_chroma_tool_offsets_present_flag);
  EXPECT_EQ(pps->pps_cb_qp_offset, 1);
  EXPECT_EQ(pps->pps_cr_qp_offset, 1);
  EXPECT_TRUE(pps->pps_joint_cbcr_qp_offset_present_flag);
  EXPECT_EQ(pps->pps_joint_cbcr_qp_offset_value, -1);
  EXPECT_FALSE(pps->pps_slice_chroma_qp_offsets_present_flag);
  EXPECT_TRUE(pps->pps_cu_chroma_qp_offset_list_enabled_flag);
  EXPECT_EQ(pps->pps_chroma_qp_offset_list_len_minus1, 3);
  EXPECT_EQ(pps->pps_cb_qp_offset_list[0], 3);
  EXPECT_EQ(pps->pps_cr_qp_offset_list[0], 2);
  EXPECT_EQ(pps->pps_joint_cbcr_qp_offset_list[0], 0);
  EXPECT_EQ(pps->pps_cb_qp_offset_list[1], 3);
  EXPECT_EQ(pps->pps_cr_qp_offset_list[1], 4);
  EXPECT_EQ(pps->pps_joint_cbcr_qp_offset_list[1], 0);
  EXPECT_EQ(pps->pps_cb_qp_offset_list[2], 8);
  EXPECT_EQ(pps->pps_cr_qp_offset_list[2], 1);
  EXPECT_EQ(pps->pps_joint_cbcr_qp_offset_list[2], 0);
  EXPECT_EQ(pps->pps_cb_qp_offset_list[3], 9);
  EXPECT_EQ(pps->pps_cr_qp_offset_list[3], 7);
  EXPECT_EQ(pps->pps_joint_cbcr_qp_offset_list[3], 0);
}

// Verify scaling list parsing in APS.
TEST_F(H266ParserTest, ParseAPSShouldConstructCorrectScalingLists) {
  LoadParserFile("bbb_scaling_lists.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);
  int aps_id;
  H266APS::ParamType aps_type;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* aps = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!aps);

  EXPECT_EQ(aps->aps_params_type, 2);
  EXPECT_EQ(aps->aps_adaptation_parameter_set_id, 0);
  EXPECT_TRUE(aps->aps_chroma_present_flag);

  const H266ScalingListData* scaling_list_data =
      &(std::get<H266ScalingListData>(aps->data));
  EXPECT_TRUE(!!scaling_list_data);

  // Verify the reconstructed quantization matrices
  // INTER2x2_CHRAMAU/INTER2x2_CHROMAV
  int inter2x2_scaling_list_expected[2][2] = {{11, 30}, {30, 50}};

  for (int i = 0; i < 2; i++) {
    for (int m = 0; m < 2; m++) {
      for (int n = 0; n < 2; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_2x2[i][m][n],
                  inter2x2_scaling_list_expected[m][n]);
      }
    }
  }

  // INTRA4x4_LUMA/INTRA4x4_CHROMAU/INTRA4x4_CHOMRAV
  int intra4x4_scaling_list_expected[4][4] = {
      {7, 12, 19, 26}, {12, 16, 24, 40}, {19, 24, 41, 50}, {26, 40, 50, 56}};

  for (int i = 0; i <= 2; i++) {
    for (int m = 0; m < 4; m++) {
      for (int n = 0; n < 4; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_4x4[i][m][n],
                  intra4x4_scaling_list_expected[m][n]);
      }
    }
  }

  // INTER4x4_LUMA/INTER4x4_CHROMAU/INTER4x4_CHROMAV
  int inter4x4_scaling_list_expected[4][4] = {
      {11, 18, 30, 43}, {18, 22, 40, 50}, {30, 40, 50, 52}, {43, 50, 52, 55}};

  for (int i = 3; i < 6; i++) {
    for (int m = 0; m < 4; m++) {
      for (int n = 0; n < 4; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_4x4[i][m][n],
                  inter4x4_scaling_list_expected[m][n]);
      }
    }
  }

  // INTRA8x8_LUMA/INTRA8x8_CHROMAU/INTRA8x8_CHROMAV
  int intra8x8_scaling_list_expected[8][8] = {
      {6, 9, 13, 18, 25, 35, 36, 37},   {9, 10, 15, 21, 32, 35, 37, 41},
      {13, 15, 18, 23, 35, 55, 58, 59}, {18, 21, 23, 26, 65, 58, 64, 66},
      {25, 32, 35, 65, 66, 66, 67, 70}, {35, 35, 55, 58, 66, 68, 70, 73},
      {36, 37, 58, 64, 67, 70, 76, 80}, {37, 41, 59, 66, 70, 73, 80, 85}};

  for (int i = 0; i <= 2; i++) {
    for (int m = 0; m < 8; m++) {
      for (int n = 0; n < 8; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[i][m][n],
                  intra8x8_scaling_list_expected[m][n]);
      }
    }
  }

  // INTER8x8_LUMA/INTER8x8_CHROMAU/INTER8x8_CHROMAV
  int inter8x8_scaling_list_expected[8][8] = {
      {9, 15, 20, 29, 36, 38, 42, 43},  {15, 17, 22, 29, 39, 43, 45, 46},
      {20, 22, 32, 34, 47, 48, 49, 50}, {29, 29, 34, 44, 50, 51, 52, 53},
      {36, 39, 47, 50, 51, 52, 55, 55}, {38, 43, 48, 51, 52, 53, 56, 58},
      {42, 45, 49, 52, 55, 56, 55, 60}, {43, 46, 50, 53, 55, 58, 60, 63}};

  for (int i = 3; i < 6; i++) {
    for (int m = 0; m < 8; m++) {
      for (int n = 0; n < 8; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[i][m][n],
                  inter8x8_scaling_list_expected[m][n]);
      }
    }
  }

  // INTRA16x16_LUMA
  for (int m = 0; m < 8; m++) {
    for (int n = 0; n < 8; n++) {
      EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[6][m][n],
                intra8x8_scaling_list_expected[m][n]);
    }
  }

  // INTRA16x16_CHROMAU/INTRA16x16_CHROMAV
  int intra16x16_scaling_list_expected[8][8] = {
      {7, 9, 13, 18, 25, 35, 36, 37},   {9, 10, 15, 21, 32, 35, 37, 41},
      {13, 15, 18, 23, 35, 55, 58, 59}, {18, 21, 23, 26, 65, 58, 64, 66},
      {25, 32, 35, 65, 66, 66, 67, 70}, {35, 35, 55, 58, 66, 68, 70, 73},
      {36, 37, 58, 64, 67, 70, 76, 80}, {37, 41, 59, 66, 70, 73, 80, 85}};

  for (int i = 7; i < 9; i++) {
    for (int m = 0; m < 8; m++) {
      for (int n = 0; n < 8; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[i][m][n],
                  intra16x16_scaling_list_expected[m][n]);
      }
    }
  }

  // INTER16x16_LUMA/INTER16x16_CHROMAU/INTER16x16_CHROMAV
  int inter16x16_scaling_list_expected[8][8] = {
      {11, 15, 20, 29, 36, 38, 42, 43}, {15, 17, 22, 29, 39, 43, 45, 46},
      {20, 22, 32, 34, 47, 48, 49, 50}, {29, 29, 34, 44, 50, 51, 52, 53},
      {36, 39, 47, 50, 51, 52, 55, 55}, {38, 43, 48, 51, 52, 53, 56, 58},
      {42, 45, 49, 52, 55, 56, 55, 60}, {43, 46, 50, 53, 55, 58, 60, 63}};

  for (int i = 9; i < 12; i++) {
    for (int m = 0; m < 8; m++) {
      for (int n = 0; n < 8; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[i][m][n],
                  inter16x16_scaling_list_expected[m][n]);
      }
    }
  }

  // INTRA32x32_LUMA/INTRA32x32_CHROMAU/INTRA32x32_CHROMAV
  // The test clip reuses 8x8 list.
  for (int i = 12; i < 15; i++) {
    for (int m = 0; m < 8; m++) {
      for (int n = 0; n < 8; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[i][m][n],
                  intra16x16_scaling_list_expected[m][n]);
      }
    }
  }

  // INTER32x32_LUMA/INTER32x32_CHROMAU/INTER32x32_CHROMAV
  // The test clip reuses 8x8 list.
  for (int i = 15; i < 18; i++) {
    for (int m = 0; m < 8; m++) {
      for (int n = 0; n < 8; n++) {
        EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[i][m][n],
                  inter16x16_scaling_list_expected[m][n]);
      }
    }
  }

  // INTRA64x64_LUMA
  int intra64x64_scaling_list_expected[8][8] = {
      {8, 9, 13, 18, 25, 35, 36, 37},   {9, 12, 15, 20, 32, 35, 37, 41},
      {13, 15, 18, 23, 35, 55, 58, 59}, {18, 21, 23, 26, 65, 58, 64, 66},
      {25, 32, 35, 65, 66, 66, 67, 70}, {35, 35, 55, 58, 66, 68, 70, 73},
      {36, 37, 58, 64, 67, 70, 76, 80}, {37, 41, 59, 66, 70, 73, 80, 85}};

  for (int m = 0; m < 8; m++) {
    for (int n = 0; n < 8; n++) {
      EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[18][m][n],
                intra64x64_scaling_list_expected[m][n]);
    }
  }

  // INTER64x64_LUMA
  int inter64x64_scaling_list_expected[8][8] = {
      {11, 15, 20, 29, 36, 38, 42, 43}, {14, 17, 23, 29, 38, 43, 45, 46},
      {20, 22, 32, 34, 47, 48, 49, 50}, {29, 29, 34, 44, 50, 51, 52, 53},
      {36, 39, 47, 50, 51, 52, 55, 55}, {38, 43, 48, 51, 52, 53, 56, 58},
      {42, 45, 49, 52, 55, 56, 55, 60}, {43, 46, 50, 53, 55, 58, 60, 63}};

  for (int m = 0; m < 8; m++) {
    for (int n = 0; n < 8; n++) {
      EXPECT_EQ(scaling_list_data->scaling_matrix_rec_8x8[19][m][n],
                inter64x64_scaling_list_expected[m][n]);
    }
  }

  // Verify the reconstructed DC array
  int quantization_matrix_dc_expected[14] = {6, 6, 6, 9, 9, 9, 6,
                                             6, 6, 9, 9, 9, 6, 9};

  for (int i = 0; i < 14; i++) {
    EXPECT_EQ(scaling_list_data->scaling_matrix_dc_rec[i],
              quantization_matrix_dc_expected[i]);
  }
}

// Verify adaptive loop filter syntax parsing in APS.
TEST_F(H266ParserTest, ParseAPSShouldConstructCorrectAlfData) {
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
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);
  int aps_id;
  H266APS::ParamType aps_type;

  // Parse the first ALF APS with id = 7.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* aps = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!aps);

  const H266AlfData* alf = &(std::get<H266AlfData>(aps->data));
  EXPECT_TRUE(!!alf);

  EXPECT_EQ(aps->aps_params_type, 0);
  EXPECT_EQ(aps->aps_adaptation_parameter_set_id, 7);
  EXPECT_TRUE(aps->aps_chroma_present_flag);
  EXPECT_TRUE(alf->alf_luma_filter_signal_flag);
  EXPECT_FALSE(alf->alf_cc_cb_filter_signal_flag);
  EXPECT_FALSE(alf->alf_cc_cr_filter_signal_flag);
  EXPECT_FALSE(alf->alf_luma_clip_flag);
  EXPECT_EQ(alf->alf_luma_num_filters_signalled_minus1, 9);

  // Verify the luma coeff delta index.
  int luma_coeff_delta_idx_expected[25] = {0, 1, 2, 3, 4, 0, 0, 2, 5,
                                           6, 0, 0, 0, 0, 0, 0, 1, 7,
                                           4, 5, 0, 6, 8, 2, 9};

  for (int i = 0; i < 25; i++) {
    EXPECT_EQ(alf->alf_luma_coeff_delta_idx[i],
              luma_coeff_delta_idx_expected[i]);
  }

  // Verify the luma coeff absolute values. Verify only the first group.
  int luma_coeff_abs_expected[12] = {1, 5, 1, 1, 4, 2, 18, 7, 8, 3, 7, 21};

  for (int i = 0; i < 12; i++) {
    EXPECT_EQ(alf->alf_luma_coeff_abs[0][i], luma_coeff_abs_expected[i]);
  }

  EXPECT_FALSE(alf->alf_chroma_clip_flag);
  EXPECT_EQ(alf->alf_chroma_num_alt_filters_minus1, 3);

  // Verify the chroma coeff absolute values. Verify only the last group.
  int chroma_coeff_abs_expected[6] = {0, 3, 4, 3, 7, 11};
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(alf->alf_chroma_coeff_abs[3][i], chroma_coeff_abs_expected[i]);
  }

  // Parse the second ALF APS with same id = 7. This should override previously
  // parsed APS with same id.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* aps_updated = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!aps_updated);

  const H266AlfData* alf_updated = &(std::get<H266AlfData>(aps_updated->data));
  EXPECT_TRUE(!!alf_updated);

  EXPECT_EQ(aps_updated->aps_params_type, 0);
  EXPECT_EQ(aps_updated->aps_adaptation_parameter_set_id, 7);
  EXPECT_TRUE(aps_updated->aps_chroma_present_flag);
  EXPECT_TRUE(alf_updated->alf_luma_filter_signal_flag);
  EXPECT_FALSE(alf_updated->alf_cc_cb_filter_signal_flag);
  EXPECT_FALSE(alf_updated->alf_cc_cr_filter_signal_flag);
  EXPECT_FALSE(alf_updated->alf_luma_clip_flag);
  EXPECT_EQ(alf_updated->alf_luma_num_filters_signalled_minus1, 9);

  // Verify the luma coeff delta index.
  int luma_coeff_delta_idx_expected_updated[25] = {0, 1, 2, 3, 3, 4, 1, 5, 3,
                                                   6, 0, 0, 0, 0, 0, 0, 4, 5,
                                                   7, 7, 1, 1, 8, 9, 3};

  for (int i = 0; i < 25; i++) {
    EXPECT_EQ(alf_updated->alf_luma_coeff_delta_idx[i],
              luma_coeff_delta_idx_expected_updated[i]);
  }

  // Verify the luma coeff absolute values. Verify only the first group.
  int luma_coeff_abs_expected_updated[12] = {1,  5, 7, 8, 8, 8,
                                             21, 7, 6, 2, 7, 25};

  for (int i = 0; i < 12; i++) {
    EXPECT_EQ(alf_updated->alf_luma_coeff_abs[0][i],
              luma_coeff_abs_expected_updated[i]);
  }

  EXPECT_FALSE(alf_updated->alf_chroma_clip_flag);
  EXPECT_EQ(alf_updated->alf_chroma_num_alt_filters_minus1, 1);

  // Verify the chroma coeff absolute values. Verify only the last group.
  int chroma_coeff_abs_expected_updated[6] = {10, 5, 27, 0, 8, 33};
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(alf_updated->alf_chroma_coeff_abs[1][i],
              chroma_coeff_abs_expected_updated[i]);
  }

  // Parse the next ALF APS with id = 6.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* aps3 = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!aps3);

  const H266AlfData* alf3 = &(std::get<H266AlfData>(aps3->data));
  EXPECT_TRUE(!!alf3);

  EXPECT_EQ(aps3->aps_params_type, 0);
  EXPECT_EQ(aps3->aps_adaptation_parameter_set_id, 6);
  EXPECT_TRUE(aps3->aps_chroma_present_flag);
  EXPECT_TRUE(alf3->alf_luma_filter_signal_flag);
  EXPECT_FALSE(alf3->alf_chroma_filter_signal_flag);
  EXPECT_FALSE(alf3->alf_cc_cb_filter_signal_flag);
  EXPECT_FALSE(alf3->alf_cc_cr_filter_signal_flag);
  EXPECT_FALSE(alf3->alf_luma_clip_flag);
  EXPECT_EQ(alf3->alf_luma_num_filters_signalled_minus1, 3);

  // Current ALF APS contains only luma coeff delta index and absolute values.
  int luma_coeff_delta_idx_expected3[25] = {0, 0, 1, 2, 2, 3, 0, 3, 2,
                                            2, 0, 0, 0, 0, 0, 0, 0, 3,
                                            2, 2, 2, 3, 3, 3, 3};
  for (int i = 0; i < 25; i++) {
    EXPECT_EQ(alf3->alf_luma_coeff_delta_idx[i],
              luma_coeff_delta_idx_expected3[i]);
  }

  // Parse the entire bitstream till no APS NUT is found, and check number
  // of APSes stored by parser.
  while (ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS)) {
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  }
  int stored_ids_of_apses[5] = {3, 4, 5, 6, 7};
  for (int i = 0; i < 5; i++) {
    const H266APS* current_aps =
        parser_.GetAPS(aps_type, stored_ids_of_apses[i]);
    EXPECT_TRUE(!!current_aps);
  }
}

// Verify luma mapping & chroma scaling data syntax parsing in APS.
TEST_F(H266ParserTest, ParseAPSShouldConstructCorrectLmcsData) {
  LoadParserFile("basketball_2_layers.vvc");
  H266NALU target_nalu;
  int vps_id;

  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kVPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseVPS(&vps_id));
  const H266VPS* vps = parser_.GetVPS(vps_id);
  EXPECT_TRUE(!!vps);

  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  int aps_id;
  H266APS::ParamType aps_type;

  // Parse the first LMCS APS with id = 0.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* aps1 = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!aps1);

  const H266LmcsData* lmcs1 = &(std::get<H266LmcsData>(aps1->data));
  EXPECT_TRUE(!!lmcs1);

  EXPECT_EQ(aps1->aps_params_type, 1);
  EXPECT_EQ(aps1->aps_adaptation_parameter_set_id, 0);
  EXPECT_TRUE(aps1->aps_chroma_present_flag);
  EXPECT_EQ(lmcs1->lmcs_min_bin_idx, 0);
  EXPECT_EQ(lmcs1->lmcs_delta_max_bin_idx, 1);
  EXPECT_EQ(lmcs1->lmcs_delta_cw_prec_minus1, 1);

  // LmcsMaxBinIdx of the test stream is 14.
  int lmcs_delta_abs_cw_expected1[15] = {2, 0, 0, 0, 0, 0, 1, 2,
                                         1, 0, 0, 0, 0, 0, 2};
  for (int i = 0; i < 15; i++) {
    EXPECT_EQ(lmcs1->lmcs_delta_abs_cw[i], lmcs_delta_abs_cw_expected1[i]);
  }
  EXPECT_EQ(lmcs1->lmcs_delta_abs_crs, 1);
  EXPECT_FALSE(lmcs1->lmcs_delta_sign_crs_flag);

  // Parse till the end of the stream and check all stored LMCS APSes
  while (ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS)) {
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  }

  int stored_ids_of_apses[4] = {0, 1, 7, 6};
  for (int i = 0; i < 2; i++) {
    aps_type = H266APS::ParamType::kLmcs;
    const H266APS* current_aps =
        parser_.GetAPS(aps_type, stored_ids_of_apses[i]);
    EXPECT_TRUE(!!current_aps);
  }

  for (int i = 2; i < 4; i++) {
    aps_type = H266APS::ParamType::kAlf;
    const H266APS* current_aps =
        parser_.GetAPS(aps_type, stored_ids_of_apses[i]);
    EXPECT_TRUE(!!current_aps);
  }

  aps_type = H266APS::ParamType::kLmcs;
  const H266APS* nonexisting_aps = parser_.GetAPS(aps_type, 2);
  EXPECT_TRUE(!nonexisting_aps);
}

// Verify parsing of simple PH_NUT.
TEST_F(H266ParserTest, ParseSimplePHNutShouldSucceed) {
  LoadParserFile("bbb_9tiles_18slices.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPH));
  H266PictureHeader ph;
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePHNut(target_nalu, &ph));
  EXPECT_TRUE(ph.ph_gdr_or_irap_pic_flag);
  EXPECT_FALSE(ph.ph_non_ref_pic_flag);
  EXPECT_FALSE(ph.ph_gdr_pic_flag);
  EXPECT_FALSE(ph.ph_inter_slice_allowed_flag);
  EXPECT_EQ(ph.ph_pic_parameter_set_id, 0);
  EXPECT_EQ(ph.ph_pic_order_cnt_lsb, 0);
  EXPECT_FALSE(ph.ph_lmcs_enabled_flag);
  EXPECT_FALSE(ph.ph_partition_constraints_override_flag);
  EXPECT_FALSE(ph.ph_joint_cbcr_sign_flag);
}

// Verify parsing of complex PH_NUT in which the RPL, deblocking filter,
// SAO, ALF and etc are coded, instead of placing them in slice header(not
// even in the picture header structure of slice header, which will be covered
// by slice header parsing tests.).
TEST_F(H266ParserTest, ParseComplexPHNutShouldSucceed) {
  LoadParserFile("bbb_rpl_in_ph_nut.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  // Parse the first PH_NUT which is for slices of the IDR_N_LP frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPH));
  H266PictureHeader ph;
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePHNut(target_nalu, &ph));
  EXPECT_TRUE(ph.ph_gdr_or_irap_pic_flag);
  EXPECT_FALSE(ph.ph_non_ref_pic_flag);
  EXPECT_FALSE(ph.ph_gdr_pic_flag);
  EXPECT_FALSE(ph.ph_inter_slice_allowed_flag);
  EXPECT_EQ(ph.ph_pic_parameter_set_id, 0);
  EXPECT_EQ(ph.ph_pic_order_cnt_lsb, 0);
  EXPECT_FALSE(ph.ph_alf_enabled_flag);
  EXPECT_FALSE(ph.ph_lmcs_enabled_flag);
  // For this test clip, RPL 0 in ref_pic_lists() is based on one of
  // ref_pic_struct(0, rplsIdx), where rplsIdx is indicated by rpl_idx[0].
  EXPECT_EQ(sps->sps_num_ref_pic_lists[0], 20);
  EXPECT_FALSE(sps->sps_rpl1_same_as_rpl0_flag);
  EXPECT_FALSE(pps->pps_rpl1_idx_present_flag);
  EXPECT_TRUE(ph.ref_pic_lists.rpl_sps_flag[0]);
  EXPECT_EQ(ph.ref_pic_lists.rpl_idx[0], 0);
  // rpl_sps_flag[1] & rpl_idx[1] is not present in PH, but they are inferred to
  // be equal to rpl_sps_flag[0] since sps_num_ref_pic_lists[i] is non-zero.
  EXPECT_EQ(sps->sps_num_ref_pic_lists[1], 20);
  EXPECT_EQ(ph.ref_pic_lists.rpl_sps_flag[1], ph.ref_pic_lists.rpl_sps_flag[0]);
  EXPECT_EQ(ph.ref_pic_lists.rpl_idx[1], ph.ref_pic_lists.rpl_idx[0]);

  EXPECT_FALSE(ph.ph_partition_constraints_override_flag);
  EXPECT_EQ(ph.ph_qp_delta, -5);
  EXPECT_FALSE(ph.ph_joint_cbcr_sign_flag);
  EXPECT_TRUE(ph.ph_sao_luma_enabled_flag);
  EXPECT_TRUE(ph.ph_sao_chroma_enabled_flag);

  // Parse the second PH_NUT which is for slices of first STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPH));
  H266PictureHeader ph2;
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePHNut(target_nalu, &ph2));
  EXPECT_FALSE(ph2.ph_gdr_or_irap_pic_flag);
  EXPECT_FALSE(ph2.ph_non_ref_pic_flag);
  EXPECT_TRUE(ph2.ph_inter_slice_allowed_flag);
  EXPECT_FALSE(ph2.ph_intra_slice_allowed_flag);
  EXPECT_EQ(ph2.ph_pic_parameter_set_id, 0);
  EXPECT_EQ(ph2.ph_pic_order_cnt_lsb, 4);
  // RPL 0/1 is signalled in PH, since rpl_sps_flag[1] is inferred to be
  // equal to rpl_sps_flag[0], which is 0 here.
  EXPECT_EQ(ph2.ref_pic_lists.rpl_sps_flag[0], 0);
  EXPECT_EQ(ph2.ref_pic_lists.rpl_sps_flag[1],
            ph2.ref_pic_lists.rpl_sps_flag[0]);
  EXPECT_EQ(ph2.ref_pic_lists.rpl_ref_lists[0].num_ref_entries, 1);
  EXPECT_EQ(ph2.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[0], 3);
  EXPECT_EQ(ph2.ref_pic_lists.rpl_ref_lists[1].num_ref_entries, 1);
  EXPECT_EQ(ph2.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[0], 3);

  EXPECT_TRUE(ph2.ph_temporal_mvp_enabled_flag);
  EXPECT_FALSE(ph2.ph_collocated_from_l0_flag);

  // Verify the weighted prediction table.
  EXPECT_EQ(ph2.pred_weight_table.luma_log2_weight_denom, 0);
  EXPECT_EQ(ph2.pred_weight_table.delta_chroma_log2_weight_denom, 0);
  EXPECT_EQ(ph2.pred_weight_table.num_l0_weights, 1);
  EXPECT_FALSE(ph2.pred_weight_table.luma_weight_l0_flag[0]);
  EXPECT_FALSE(ph2.pred_weight_table.chroma_weight_l0_flag[0]);
  EXPECT_EQ(ph2.pred_weight_table.num_l1_weights, 1);
  EXPECT_FALSE(ph2.pred_weight_table.luma_weight_l1_flag[0]);
  EXPECT_FALSE(ph2.pred_weight_table.chroma_weight_l1_flag[0]);

  // Verify other misc syntax elements.
  EXPECT_EQ(ph2.ph_qp_delta, 4);
  EXPECT_FALSE(ph2.ph_joint_cbcr_sign_flag);
  EXPECT_TRUE(ph2.ph_sao_chroma_enabled_flag);
  EXPECT_TRUE(ph2.ph_sao_chroma_enabled_flag);

  // Parse till the end of the stream for the last PH_NUT.
  H266PictureHeader last_ph;
  while (ParseNalusUntilNut(&target_nalu, H266NALU::kPH)) {
    EXPECT_EQ(H266Parser::kOk, parser_.ParsePHNut(target_nalu, &last_ph));
  }
  EXPECT_EQ(last_ph.ph_pic_order_cnt_lsb, 3);
  EXPECT_EQ(last_ph.ref_pic_lists.rpl_ref_lists[0].num_ref_entries, 2);
  EXPECT_EQ(last_ph.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[0], 0);
  EXPECT_EQ(last_ph.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[1], 2);
  EXPECT_EQ(last_ph.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[0], 0);
  EXPECT_EQ(last_ph.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[1], 2);
  EXPECT_EQ(last_ph.pred_weight_table.luma_log2_weight_denom, 0);
  EXPECT_EQ(last_ph.pred_weight_table.delta_chroma_log2_weight_denom, 0);
  EXPECT_EQ(last_ph.pred_weight_table.num_l0_weights, 2);
  EXPECT_FALSE(last_ph.pred_weight_table.luma_weight_l0_flag[0]);
  EXPECT_FALSE(last_ph.pred_weight_table.luma_weight_l0_flag[1]);
  EXPECT_FALSE(last_ph.pred_weight_table.chroma_weight_l0_flag[0]);
  EXPECT_FALSE(last_ph.pred_weight_table.chroma_weight_l0_flag[1]);
  EXPECT_EQ(last_ph.pred_weight_table.num_l1_weights, 2);
  EXPECT_FALSE(last_ph.pred_weight_table.luma_weight_l1_flag[0]);
  EXPECT_FALSE(last_ph.pred_weight_table.chroma_weight_l1_flag[0]);
  EXPECT_EQ(last_ph.ph_qp_delta, 7);
}

TEST_F(H266ParserTest, ParsePHNutWithoutParsingPPSShouldFail) {
  LoadParserFile("bbb_9tiles_18slices.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  // Parsing of the SPS should generate fake VPS with vps_id = 0;
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPH));
  H266PictureHeader ph;
  EXPECT_EQ(H266Parser::kInvalidStream, parser_.ParsePHNut(target_nalu, &ph));
}

// Verify parsing of slices that contains RPL etc directly in slice header.
TEST_F(H266ParserTest, ParseSliceWithRplInSliceShouldSucceed) {
  LoadParserFile("bbb_rpl_in_slice.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  int aps_id;
  H266APS::ParamType aps_type;
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* lmcs_aps = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!lmcs_aps);

  // Parse the first frame, the IDR_N_LP slice.
  H266SliceHeader shdr;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  // Verify the picture header in slice.
  EXPECT_TRUE(shdr.sh_picture_header_in_slice_header_flag);
  EXPECT_TRUE(shdr.picture_header.ph_gdr_or_irap_pic_flag);
  EXPECT_FALSE(shdr.picture_header.ph_non_ref_pic_flag);
  EXPECT_FALSE(shdr.picture_header.ph_gdr_pic_flag);
  EXPECT_FALSE(shdr.picture_header.ph_inter_slice_allowed_flag);
  EXPECT_EQ(shdr.picture_header.ph_pic_order_cnt_lsb, 0);
  EXPECT_EQ(shdr.picture_header.ph_lmcs_aps_id, 0);
  EXPECT_TRUE(shdr.picture_header.ph_lmcs_enabled_flag);
  EXPECT_TRUE(shdr.picture_header.ph_chroma_residual_scale_flag);
  EXPECT_FALSE(shdr.picture_header.ph_partition_constraints_override_flag);
  EXPECT_EQ(shdr.picture_header.ph_cu_chroma_qp_offset_subdiv_intra_slice, 0);
  EXPECT_FALSE(shdr.picture_header.ph_joint_cbcr_sign_flag);

  EXPECT_FALSE(shdr.sh_no_output_of_prior_pics_flag);
  EXPECT_FALSE(shdr.sh_alf_enabled_flag);
  EXPECT_EQ(shdr.sh_qp_delta, -5);
  EXPECT_TRUE(shdr.sh_cu_chroma_qp_offset_enabled_flag);
  EXPECT_TRUE(shdr.sh_sao_luma_used_flag);
  EXPECT_TRUE(shdr.sh_sao_chroma_used_flag);
  EXPECT_TRUE(shdr.sh_dep_quant_used_flag);
  EXPECT_TRUE(shdr.IsISlice());

  // Parser the second frame, the STSA slice.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  // Verify the picture header in slice.
  EXPECT_TRUE(shdr.sh_picture_header_in_slice_header_flag);
  EXPECT_FALSE(shdr.picture_header.ph_gdr_or_irap_pic_flag);
  EXPECT_FALSE(shdr.picture_header.ph_non_ref_pic_flag);
  EXPECT_FALSE(shdr.picture_header.ph_gdr_pic_flag);
  EXPECT_TRUE(shdr.picture_header.ph_inter_slice_allowed_flag);
  EXPECT_FALSE(shdr.picture_header.ph_intra_slice_allowed_flag);
  EXPECT_EQ(shdr.picture_header.ph_pic_order_cnt_lsb, 4);
  EXPECT_EQ(shdr.picture_header.ph_lmcs_aps_id, 0);
  EXPECT_TRUE(shdr.picture_header.ph_lmcs_enabled_flag);
  EXPECT_TRUE(shdr.picture_header.ph_chroma_residual_scale_flag);
  EXPECT_FALSE(shdr.picture_header.ph_partition_constraints_override_flag);
  EXPECT_EQ(shdr.picture_header.ph_cu_chroma_qp_offset_subdiv_intra_slice, 0);
  EXPECT_FALSE(shdr.picture_header.ph_joint_cbcr_sign_flag);
  EXPECT_TRUE(shdr.picture_header.ph_temporal_mvp_enabled_flag);
  EXPECT_FALSE(shdr.picture_header.ph_mmvd_fullpel_only_flag);
  EXPECT_TRUE(shdr.picture_header.ph_mvd_l1_zero_flag);
  EXPECT_FALSE(shdr.picture_header.ph_mmvd_fullpel_only_flag);
  EXPECT_FALSE(shdr.picture_header.ph_bdof_disabled_flag);
  EXPECT_FALSE(shdr.picture_header.ph_dmvr_disabled_flag);
  EXPECT_TRUE(shdr.IsBSlice());
  // Verify the ref list of current frame.
  // Current B-frame does not use ref picture list structure template
  // in SPS, but instead explicitly signal it in slice header.
  EXPECT_FALSE(shdr.ref_pic_lists.rpl_sps_flag[0]);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].num_ref_entries, 1);
  // According to equation 150, AbsDeltaPocSt[0][0][0] should be given 4
  // current PoC = 4 and the I-frame is with PoC = 0.
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[0], 3);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[0].strp_entry_sign_flag[0]);

  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].num_ref_entries, 1);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[0], 3);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[1].strp_entry_sign_flag[0]);

  EXPECT_FALSE(shdr.sh_alf_enabled_flag);
  EXPECT_FALSE(shdr.sh_cabac_init_flag);
  EXPECT_EQ(shdr.sh_pred_weight_table.delta_chroma_log2_weight_denom, 0);
  EXPECT_FALSE(shdr.sh_pred_weight_table.chroma_weight_l1_flag[0]);
  EXPECT_EQ(shdr.sh_qp_delta, 4);
  EXPECT_TRUE(shdr.sh_cu_chroma_qp_offset_enabled_flag);
  EXPECT_TRUE(shdr.sh_sao_luma_used_flag);
  EXPECT_TRUE(shdr.sh_sao_chroma_used_flag);
  EXPECT_TRUE(shdr.sh_dep_quant_used_flag);

  // Parse the 2nd STAS frame in decoding order, with PoC = 2 and referencing
  // frame with PoC = 0 & PoC = 4.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  EXPECT_EQ(shdr.picture_header.ph_pic_order_cnt_lsb, 2);
  EXPECT_FALSE(shdr.ref_pic_lists.rpl_sps_flag[0]);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].num_ref_entries, 2);
  // Refers to PoC = 0.
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[0], 1);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[0].strp_entry_sign_flag[0]);
  // Refers to PoC = 4.
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[1], 4);
  EXPECT_FALSE(shdr.ref_pic_lists.rpl_ref_lists[0].strp_entry_sign_flag[1]);

  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].num_ref_entries, 2);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[0], 1);
  EXPECT_FALSE(shdr.ref_pic_lists.rpl_ref_lists[1].strp_entry_sign_flag[0]);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[1], 4);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[1].strp_entry_sign_flag[1]);

  // Parse till before next APS.
  for (int i = 0; i < 4; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  }

  // Parse next APS which is an ALF APS, referenced by the last STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));

  // Parse the last STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));

  EXPECT_EQ(shdr.picture_header.ph_pic_order_cnt_lsb, 7);
  EXPECT_FALSE(shdr.ref_pic_lists.rpl_sps_flag[0]);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].num_ref_entries, 3);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[0], 0);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[0].strp_entry_sign_flag[0]);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[1], 2);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[0].strp_entry_sign_flag[1]);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[2], 4);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[0].strp_entry_sign_flag[2]);

  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].num_ref_entries, 2);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[0], 0);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[1].strp_entry_sign_flag[0]);
  EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[1], 2);
  EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[1].strp_entry_sign_flag[0]);

  // Verify the ALF info and weighted prediction table.
  EXPECT_TRUE(shdr.sh_alf_enabled_flag);
  EXPECT_EQ(shdr.sh_num_alf_aps_ids_luma, 1);
  EXPECT_TRUE(shdr.sh_alf_aps_id_luma[0]);
  EXPECT_TRUE(shdr.sh_alf_cb_enabled_flag);
  EXPECT_TRUE(shdr.sh_alf_cr_enabled_flag);
  EXPECT_EQ(shdr.sh_alf_aps_id_chroma, 7);
  EXPECT_FALSE(shdr.sh_alf_cc_cb_enabled_flag);
  EXPECT_FALSE(shdr.sh_alf_cc_cr_enabled_flag);
  EXPECT_EQ(shdr.sh_pred_weight_table.luma_log2_weight_denom, 6);
  EXPECT_TRUE(shdr.sh_pred_weight_table.luma_weight_l0_flag[0]);
  EXPECT_TRUE(shdr.sh_pred_weight_table.luma_weight_l0_flag[1]);
  EXPECT_TRUE(shdr.sh_pred_weight_table.chroma_weight_l0_flag[0]);
  EXPECT_FALSE(shdr.sh_pred_weight_table.chroma_weight_l0_flag[1]);
  EXPECT_EQ(shdr.sh_pred_weight_table.delta_luma_weight_l0[0], 47);
  EXPECT_EQ(shdr.sh_pred_weight_table.luma_offset_l0[0], -15);
  EXPECT_EQ(shdr.sh_pred_weight_table.delta_chroma_weight_l0[0][0], 45);
  EXPECT_EQ(shdr.sh_pred_weight_table.delta_chroma_weight_l0[0][1], 46);
  EXPECT_EQ(shdr.sh_pred_weight_table.delta_luma_weight_l1[1], -8);
  EXPECT_EQ(shdr.sh_pred_weight_table.luma_offset_l1[1], 122);
}

// Verify parsing of slice that has entry points syntax in it.
TEST_F(H266ParserTest, ParseSliceWithDblkParamsAndEntryPointsShouldSucceed) {
  LoadParserFile("bbb_slice_with_entrypoints.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  int aps_id;
  H266APS::ParamType aps_type;
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* lmcs_aps = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!lmcs_aps);

  // Parse the first frame, the IDR_N_LP frame.
  H266SliceHeader shdr;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_TRUE(shdr.picture_header.ph_deblocking_params_present_flag);
  EXPECT_FALSE(shdr.picture_header.ph_deblocking_filter_disabled_flag);
  EXPECT_EQ(shdr.picture_header.ph_luma_beta_offset_div2, -2);
  EXPECT_EQ(shdr.picture_header.ph_luma_tc_offset_div2, 0);
  EXPECT_EQ(shdr.picture_header.ph_cb_beta_offset_div2, -2);
  EXPECT_EQ(shdr.picture_header.ph_cb_tc_offset_div2, 0);
  EXPECT_EQ(shdr.picture_header.ph_cr_beta_offset_div2, -2);
  EXPECT_EQ(shdr.picture_header.ph_cr_tc_offset_div2, 0);
  EXPECT_EQ(shdr.sh_slice_address, 0);
  EXPECT_EQ(shdr.sh_num_tiles_in_slice_minus1, 8);
  EXPECT_FALSE(shdr.sh_no_output_of_prior_pics_flag);
  EXPECT_TRUE(shdr.sh_cu_chroma_qp_offset_enabled_flag);
  EXPECT_EQ(shdr.sh_entry_offset_len_minus1, 4);
  EXPECT_EQ(shdr.sh_entry_point_offset_minus1.size(), 26u);
  int expected_entry_point_offsets[26] = {17, 12, 12, 17, 12, 12, 17, 12, 12,
                                          17, 12, 12, 17, 12, 12, 17, 12, 12,
                                          17, 12, 26, 17, 12, 26, 17, 12};
  int idx = 0;
  for (const auto& val : shdr.sh_entry_point_offset_minus1) {
    if (idx < 26) {
      EXPECT_EQ(val, expected_entry_point_offsets[idx]);
    }
    idx++;
  }

  // Parse the second frame, the STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  // For this frame, the ref_pic_list is in picture header, instead of slice
  // header directly.
  EXPECT_FALSE(shdr.picture_header.ref_pic_lists.rpl_sps_flag[0]);
  EXPECT_EQ(shdr.picture_header.ref_pic_lists.rpl_ref_lists[0].num_ref_entries,
            1);
  EXPECT_EQ(
      shdr.picture_header.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[0],
      0);
  EXPECT_EQ(shdr.picture_header.ref_pic_lists.rpl_ref_lists[1].num_ref_entries,
            1);
  EXPECT_EQ(
      shdr.picture_header.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[1],
      0);
  EXPECT_EQ(shdr.sh_entry_offset_len_minus1, 1);
  EXPECT_EQ(shdr.sh_entry_point_offset_minus1.size(), 26u);
  int expected_entry_point_offsets2[26] = {1, 1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 1, 1, 1, 1, 1, 1, 1,
                                           1, 1, 2, 1, 1, 2, 1, 1};
  idx = 0;
  for (const auto& val : shdr.sh_entry_point_offset_minus1) {
    if (idx < 26) {
      EXPECT_EQ(val, expected_entry_point_offsets2[idx]);
    }
    idx++;
  }
}

// Verify parsing of frames that are multi-slice encoded.
TEST_F(H266ParserTest, ParsePUWithMultipleSlicesShouldSucceed) {
  LoadParserFile("bbb_9tiles_18slices.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  for (int frame_id = 0; frame_id < 8; frame_id++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPH));
    H266PictureHeader ph;
    EXPECT_EQ(H266Parser::kOk, parser_.ParsePHNut(target_nalu, &ph));

    H266SliceHeader shdr;
    for (int slice_id = 0; slice_id < 18; slice_id++) {
      if (frame_id == 0) {
        EXPECT_TRUE(
            ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
        EXPECT_EQ(H266Parser::kOk,
                  parser_.ParseSliceHeader(target_nalu, true, &ph, &shdr));
      } else {
        EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
        EXPECT_EQ(H266Parser::kOk,
                  parser_.ParseSliceHeader(target_nalu, false, &ph, &shdr));
      }
      EXPECT_EQ(shdr.sh_slice_address, slice_id);
    }
  }
}

// Verify parsing of frames that are multi-slice and multi-subpicture encoded.
TEST_F(H266ParserTest, ParsePUWithMultipleSubpicturesAndSlicesShouldSucceed) {
  LoadParserFile("bbb_2_subpictures_8_slices.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  int aps_id;
  H266APS::ParamType aps_type;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* lmcs_aps = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!lmcs_aps);

  // First check number of slices in each subpicture.
  EXPECT_EQ(sps->sps_num_subpics_minus1, 1);
  EXPECT_EQ(pps->num_slices_in_subpic[0], 6);
  EXPECT_EQ(pps->num_slices_in_subpic[1], 2);

  // Then check the slice sizes.
  EXPECT_EQ(pps->pps_num_slices_in_pic_minus1, 7);
  int expected_slice_height[8] = {2, 1, 2, 1, 6, 3, 6, 3};
  for (int i = 0; i < pps->pps_num_slices_in_pic_minus1 + 1; i++) {
    EXPECT_EQ(pps->slice_height_in_ctus[i], expected_slice_height[i]);
  }

  // For each frame, last two slices belong to the second subpicture
  for (int frame_id = 0; frame_id < 2; frame_id++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPH));
    H266PictureHeader ph;
    EXPECT_EQ(H266Parser::kOk, parser_.ParsePHNut(target_nalu, &ph));

    H266SliceHeader shdr;
    for (int slice_id = 0; slice_id < 8; slice_id++) {
      if (frame_id == 0) {
        EXPECT_TRUE(
            ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
        EXPECT_EQ(H266Parser::kOk,
                  parser_.ParseSliceHeader(target_nalu, true, &ph, &shdr));
      } else {
        EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
        EXPECT_EQ(H266Parser::kOk,
                  parser_.ParseSliceHeader(target_nalu, false, &ph, &shdr));
      }
      if (slice_id <= 5) {
        EXPECT_EQ(shdr.sh_subpic_id, 0);
        EXPECT_EQ(shdr.sh_slice_address, slice_id);
      } else {
        EXPECT_EQ(shdr.sh_subpic_id, 1);
        EXPECT_EQ(shdr.sh_slice_address, slice_id - 6);
      }
    }
    // Verify the syntax elements in the last frame's last slice.
    if (frame_id == 1) {
      EXPECT_FALSE(shdr.ref_pic_lists.rpl_sps_flag[0]);
      EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].num_ref_entries, 1);
      EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[0].abs_delta_poc_st[0], 0);
      EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[0].strp_entry_sign_flag[0]);
      EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].num_ref_entries, 1);
      EXPECT_EQ(shdr.ref_pic_lists.rpl_ref_lists[1].abs_delta_poc_st[0], 0);
      EXPECT_TRUE(shdr.ref_pic_lists.rpl_ref_lists[1].strp_entry_sign_flag[0]);
      EXPECT_FALSE(shdr.sh_cabac_init_flag);
      EXPECT_FALSE(shdr.sh_collocated_from_l0_flag);
      EXPECT_EQ(shdr.sh_qp_delta, 7);
      EXPECT_TRUE(shdr.sh_cu_chroma_qp_offset_enabled_flag);
      EXPECT_TRUE(shdr.sh_sao_chroma_used_flag);
      EXPECT_TRUE(shdr.sh_sao_luma_used_flag);
      EXPECT_TRUE(shdr.sh_dep_quant_used_flag);
    }
  }
}

// Verify POC calculation for stream without ph_poc_msb_cycle_value set,
// for stream without multi-slice encoded.
TEST_F(H266ParserTest, FramesWithoutMsbCycleShouldReturnCorrectPOC) {
  LoadParserFile("bear_180p.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);

  int aps_id;
  H266APS::ParamType aps_type;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* alf_aps = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!alf_aps);

  H266SliceHeader shdr;
  int expected_poc_list[29] = {16, 8,  4,  2,  1,  3,  6,  5,  7,  12,
                               10, 9,  11, 14, 13, 15, 24, 20, 18, 17,
                               19, 22, 21, 23, 28, 26, 25, 27, 29};

  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            0);

  for (int i = 0; i < 29; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
    EXPECT_EQ(
        poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
        expected_poc_list[i]);
  }
}

// Verify POC calculation for stream that has IDR frames in the middle of
// it.
TEST_F(H266ParserTest, FramesWithMultipleIDRsShouldReturnCorrectPOC) {
  LoadParserFile("bbb_poc_gop8.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);

  H266SliceHeader shdr;
  // Verify POC of first IDR_N_LP frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            0);

  // Verify POC of TRAIL_NUT frames #1 - #7
  for (int i = 1; i < 8; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
    EXPECT_EQ(
        poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr), i);
  }

  // Verify POC of second IDR_N_LP frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            8);

  // Verify POC of remaining TRAIL frames.
  for (int i = 9; i < 12; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
    EXPECT_EQ(
        poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr), i);
  }
}

// Verify POC calculation for stream that is encoded with POC LSB limited
// to 16.
TEST_F(H266ParserTest, FramesWithMaximumPOCLsbSetShouldReturnCorrectPOC) {
  LoadParserFile("bbb_poc_msb.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);

  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);

  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);

  H266SliceHeader shdr;
  // Verify POC of first IDR_N_LP frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            0);

  // Verify POC of TRAIL_NUT frames #1 - #7
  for (int i = 1; i < 8; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
    EXPECT_EQ(
        poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr), i);
  }

  // Skip the second group of SPS/PPS, verify POC of second IDR_N_LP frame,
  // since repeated SPS/PPS is enabled.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            8);

  // Verify POC of remaining TRAIL frames till before next SPS/PPS.
  for (int i = 9; i < 16; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
    EXPECT_EQ(
        poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr), i);
  }

  // Refresh current active SPS/PPS, though not necessary.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps2 = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps2);

  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps2 = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps2);

  const H266VPS* vps2 = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps2);

  // For frame #16 and #17, the POC is reset due to LSB is configured to not
  // exceed 16.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(
      poc_.ComputePicOrderCnt(sps2, pps2, vps2, &shdr.picture_header, shdr), 0);

  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(
      poc_.ComputePicOrderCnt(sps2, pps2, vps2, &shdr.picture_header, shdr), 1);
}

// Verify reference picture list construction for frames with only STRP
TEST_F(H266ParserTest, RefPictListsForFrameWithSTRPShouldSucceed) {
  LoadParserFile("bbb_rpl_in_slice.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H266PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);

  int aps_id;
  H266APS::ParamType aps_type;
  EXPECT_EQ(H266Parser::kOk, parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  const H266APS* lmcs_aps = parser_.GetAPS(aps_type, aps_id);
  EXPECT_TRUE(!!lmcs_aps);

  H266SliceHeader shdr;
  // Verify POC/ref picture lists of first IDR_N_LP frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            0);

  std::vector<H266RefEntry> ref_list0;
  std::vector<H266RefEntry> ref_list1;
  poc_.ComputeRefPicPocList(sps, pps, vps, &shdr.picture_header, shdr, 0,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 0u);
  EXPECT_EQ(ref_list1.size(), 0u);

  // Verify next STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            4);
  poc_.ComputeRefPicPocList(sps, pps, vps, &shdr.picture_header, shdr, 4,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 1u);
  EXPECT_EQ(ref_list1.size(), 1u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 0);

  // Verify next STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            2);
  poc_.ComputeRefPicPocList(sps, pps, vps, &shdr.picture_header, shdr, 2,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 2u);
  EXPECT_EQ(ref_list1.size(), 2u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list0[1].entry_type, 0);
  EXPECT_EQ(ref_list0[1].pic_order_cnt, 4);
  EXPECT_EQ(ref_list0[1].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 4);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[1].entry_type, 0);
  EXPECT_EQ(ref_list1[1].pic_order_cnt, 0);
  EXPECT_EQ(ref_list1[1].nuh_layer_id, 0);

  // Verify the 3rd STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            1);
  poc_.ComputeRefPicPocList(sps, pps, vps, &shdr.picture_header, shdr, 1,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 2u);
  EXPECT_EQ(ref_list1.size(), 2u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list0[1].entry_type, 0);
  EXPECT_EQ(ref_list0[1].pic_order_cnt, 2);
  EXPECT_EQ(ref_list0[1].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 2);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[1].entry_type, 0);
  EXPECT_EQ(ref_list1[1].pic_order_cnt, 4);
  EXPECT_EQ(ref_list1[1].nuh_layer_id, 0);

  // Verify the 4th STSA frame.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSTSA));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            3);
  poc_.ComputeRefPicPocList(sps, pps, vps, &shdr.picture_header, shdr, 3,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 2u);
  EXPECT_EQ(ref_list1.size(), 2u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 2);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list0[1].entry_type, 0);
  EXPECT_EQ(ref_list0[1].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[1].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 4);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[1].entry_type, 0);
  EXPECT_EQ(ref_list1[1].pic_order_cnt, 2);
  EXPECT_EQ(ref_list1[1].nuh_layer_id, 0);
}

// Verify reference picture list construction for frames with LTRP
TEST_F(H266ParserTest, RefPictListsForFrameWithLTRPShouldSucceed) {
  LoadParserFile("vvc_frames_with_ltr.vvc");
  H266NALU target_nalu;
  int sps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id));
  const H266SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  int pps_id;
  // There are 64 PPSes after SPS in this stream.
  for (int i = 0; i < 64; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
    EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  }

  for (int i = 0; i < 2; i++) {
    int aps_id;
    H266APS::ParamType aps_type;
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPrefixAPS));
    EXPECT_EQ(H266Parser::kOk,
              parser_.ParseAPS(target_nalu, &aps_id, &aps_type));
  }

  // Since all PUs will have a PH in this stream as it is multi-slice encoded,
  // we skip directly to the 7th PU.
  H266PictureHeader ph;
  for (int i = 0; i < 8; i++) {
    EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPH));
  }
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePHNut(target_nalu, &ph));

  const H266VPS* vps = parser_.GetVPS(0);
  EXPECT_TRUE(!!vps);

  const H266PPS* pps = parser_.GetPPS(33);
  EXPECT_TRUE(!!pps);

  H266SliceHeader shdr;
  // Verify POC/ref picture lists of the 0-based 7th PU's first slice.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, &ph, &shdr));
  EXPECT_EQ(poc_.ComputePicOrderCnt(sps, pps, vps, &shdr.picture_header, shdr),
            6);

  std::vector<H266RefEntry> ref_list0;
  std::vector<H266RefEntry> ref_list1;
  poc_.ComputeRefPicPocList(sps, pps, vps, &shdr.picture_header, shdr, 6,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 4u);
  EXPECT_EQ(ref_list1.size(), 6u);

  int expected_list0_pocs[4] = {0, 2, 8, 3},
      expected_list1_pocs[6] = {8, 2, 0, 0, 0, 0};
  for (int i = 0; i < 4; i++) {
    EXPECT_EQ(ref_list0[i].entry_type, i == 3 ? 0 : 1);
    EXPECT_EQ(ref_list0[i].pic_order_cnt, expected_list0_pocs[i]);
    EXPECT_EQ(ref_list0[i].nuh_layer_id, 0);
  }
  for (int i = 0; i < 6; i++) {
    EXPECT_EQ(ref_list1[i].entry_type, 1);
    EXPECT_EQ(ref_list1[i].pic_order_cnt, expected_list1_pocs[i]);
    EXPECT_EQ(ref_list1[i].nuh_layer_id, 0);
  }
}

// Verify reference picture list construction for frames with ILRP
TEST_F(H266ParserTest, RefPictListsForFrameWithILRPShouldSucceed) {
  LoadParserFile("basketball_2_layers.vvc");
  H266NALU target_nalu;
  int vps_id;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kVPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseVPS(&vps_id));
  const H266VPS* vps = parser_.GetVPS(vps_id);

  int sps_id_l0;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id_l0));
  const H266SPS* sps_l0 = parser_.GetSPS(sps_id_l0);
  EXPECT_TRUE(!!sps_l0);

  int pps_id_l0;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id_l0));
  const H266PPS* pps_l0 = parser_.GetPPS(pps_id_l0);
  EXPECT_TRUE(!!pps_l0);

  // Parse the first IDR_N_LP frame for ref_pic_lists. Since this stream
  // is a multi-layer stream, the H266POC instance will not handle it at
  // present. For the test we directly provide POC for higher layer PUs.
  H266SliceHeader shdr;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));

  std::vector<H266RefEntry> ref_list0;
  std::vector<H266RefEntry> ref_list1;
  poc_.ComputeRefPicPocList(sps_l0, pps_l0, vps, &shdr.picture_header, shdr, 0,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 0u);
  EXPECT_EQ(ref_list1.size(), 0u);

  int sps_id_l1;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kSPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParseSPS(target_nalu, &sps_id_l1));
  const H266SPS* sps_l1 = parser_.GetSPS(sps_id_l1);
  EXPECT_TRUE(!!sps_l1);

  int pps_id_l1;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kPPS));
  EXPECT_EQ(H266Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id_l1));
  const H266PPS* pps_l1 = parser_.GetPPS(pps_id_l1);
  EXPECT_TRUE(!!pps_l1);

  // Next IDR_N_LP frame in the same AU but at layer 1.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kIDRNoLeadingPicture));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, true, nullptr, &shdr));

  poc_.ComputeRefPicPocList(sps_l1, pps_l1, vps, &shdr.picture_header, shdr, 0,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 1u);
  EXPECT_EQ(ref_list1.size(), 1u);
  EXPECT_EQ(ref_list0[0].entry_type, 2);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 2);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 0);

  // Next TRAIL frame in next AU at layer 0.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  poc_.ComputeRefPicPocList(sps_l0, pps_l0, vps, &shdr.picture_header, shdr, 1,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 1u);
  EXPECT_EQ(ref_list1.size(), 1u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 0);

  // Next TRAIL frame in current AU at layer 1.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  poc_.ComputeRefPicPocList(sps_l1, pps_l1, vps, &shdr.picture_header, shdr, 1,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 2u);
  EXPECT_EQ(ref_list1.size(), 2u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 1);
  EXPECT_EQ(ref_list0[1].entry_type, 2);
  EXPECT_EQ(ref_list0[1].pic_order_cnt, 1);
  EXPECT_EQ(ref_list0[1].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 0);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 1);
  EXPECT_EQ(ref_list1[1].entry_type, 2);
  EXPECT_EQ(ref_list1[1].pic_order_cnt, 1);
  EXPECT_EQ(ref_list1[1].nuh_layer_id, 0);

  // Next TRAIL frame in next AU at layer 0.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  poc_.ComputeRefPicPocList(sps_l0, pps_l0, vps, &shdr.picture_header, shdr, 2,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 2u);
  EXPECT_EQ(ref_list1.size(), 2u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 1);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list0[1].entry_type, 0);
  EXPECT_EQ(ref_list0[1].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[1].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 1);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[1].entry_type, 0);
  EXPECT_EQ(ref_list1[1].pic_order_cnt, 0);
  EXPECT_EQ(ref_list1[1].nuh_layer_id, 0);

  // Next TRAIL frame in current AU at layer 1.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H266NALU::kTrail));
  EXPECT_EQ(H266Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, false, nullptr, &shdr));
  poc_.ComputeRefPicPocList(sps_l1, pps_l1, vps, &shdr.picture_header, shdr, 2,
                            ref_list0, ref_list1);
  EXPECT_EQ(ref_list0.size(), 3u);
  EXPECT_EQ(ref_list1.size(), 3u);
  EXPECT_EQ(ref_list0[0].entry_type, 0);
  EXPECT_EQ(ref_list0[0].pic_order_cnt, 1);
  EXPECT_EQ(ref_list0[0].nuh_layer_id, 1);
  EXPECT_EQ(ref_list0[1].entry_type, 0);
  EXPECT_EQ(ref_list0[1].pic_order_cnt, 0);
  EXPECT_EQ(ref_list0[1].nuh_layer_id, 1);
  EXPECT_EQ(ref_list0[2].entry_type, 2);
  EXPECT_EQ(ref_list0[2].pic_order_cnt, 2);
  EXPECT_EQ(ref_list0[2].nuh_layer_id, 0);
  EXPECT_EQ(ref_list1[0].entry_type, 0);
  EXPECT_EQ(ref_list1[0].pic_order_cnt, 1);
  EXPECT_EQ(ref_list1[0].nuh_layer_id, 1);
  EXPECT_EQ(ref_list1[1].entry_type, 0);
  EXPECT_EQ(ref_list1[1].pic_order_cnt, 0);
  EXPECT_EQ(ref_list1[1].nuh_layer_id, 1);
  EXPECT_EQ(ref_list1[2].entry_type, 2);
  EXPECT_EQ(ref_list1[2].pic_order_cnt, 2);
  EXPECT_EQ(ref_list1[2].nuh_layer_id, 0);
}

}  // namespace media
