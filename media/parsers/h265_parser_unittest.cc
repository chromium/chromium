// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "media/base/test_data_util.h"
#include "media/parsers/h265_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
struct HevcTestData {
  std::string file_name;
  // Number of NALUs in the test stream to be parsed.
  int num_nalus;
};

}  // namespace

class H265ParserTest : public ::testing::Test {
 protected:
  void LoadParserFile(std::string file_name) {
    parser_.Reset();
    base::FilePath file_path = GetTestDataFilePath(file_name);

    stream_ = std::make_unique<base::MemoryMappedFile>();
    ASSERT_TRUE(stream_->Initialize(file_path))
        << "Couldn't open stream file: " << file_path.MaybeAsASCII();

    parser_.SetStream(stream_->data(), stream_->length());
  }

  bool ParseNalusUntilNut(H265NALU* target_nalu, H265NALU::Type nalu_type) {
    while (true) {
      H265Parser::Result res = parser_.AdvanceToNextNALU(target_nalu);
      if (res == H265Parser::kEOStream) {
        return false;
      }
      EXPECT_EQ(res, H265Parser::kOk);
      if (target_nalu->nal_unit_type == nalu_type)
        return true;
    }
  }

  H265Parser parser_;
  std::unique_ptr<base::MemoryMappedFile> stream_;
};

TEST_F(H265ParserTest, RawHevcStreamFileParsing) {
  HevcTestData test_data[] = {
      {"bear.hevc", 35},
      {"bbb.hevc", 64},
  };

  for (const auto& data : test_data) {
    LoadParserFile(data.file_name);
    // Parse until the end of stream/unsupported stream/error in stream is
    // found.
    int num_parsed_nalus = 0;
    while (true) {
      H265NALU nalu;
      H265SEI sei;
      H265Parser::Result res = parser_.AdvanceToNextNALU(&nalu);
      if (res == H265Parser::kEOStream) {
        DVLOG(1) << "Number of successfully parsed NALUs before EOS: "
                 << num_parsed_nalus;
        EXPECT_EQ(data.num_nalus, num_parsed_nalus);
        break;
      }
      EXPECT_EQ(res, H265Parser::kOk);

      ++num_parsed_nalus;
      DVLOG(4) << "Found NALU " << nalu.nal_unit_type;

      H265SliceHeader shdr;
      switch (nalu.nal_unit_type) {
        case H265NALU::VPS_NUT:
          int vps_id;
          res = parser_.ParseVPS(&vps_id);
          EXPECT_TRUE(!!parser_.GetVPS(vps_id));
          break;
        case H265NALU::SPS_NUT:
          int sps_id;
          res = parser_.ParseSPS(&sps_id);
          EXPECT_TRUE(!!parser_.GetSPS(sps_id));
          break;
        case H265NALU::PPS_NUT:
          int pps_id;
          res = parser_.ParsePPS(nalu, &pps_id);
          EXPECT_TRUE(!!parser_.GetPPS(pps_id));
          break;
        case H265NALU::PREFIX_SEI_NUT:
          res = parser_.ParseSEI(&sei);
          EXPECT_EQ(res, H265Parser::kOk);
          break;
        case H265NALU::TRAIL_N:
        case H265NALU::TRAIL_R:
        case H265NALU::TSA_N:
        case H265NALU::TSA_R:
        case H265NALU::STSA_N:
        case H265NALU::STSA_R:
        case H265NALU::RADL_N:
        case H265NALU::RADL_R:
        case H265NALU::RASL_N:
        case H265NALU::RASL_R:
        case H265NALU::BLA_W_LP:
        case H265NALU::BLA_W_RADL:
        case H265NALU::BLA_N_LP:
        case H265NALU::IDR_W_RADL:
        case H265NALU::IDR_N_LP:
        case H265NALU::CRA_NUT:  // fallthrough
          res = parser_.ParseSliceHeader(nalu, &shdr, nullptr);
          break;
        default:
          break;
      }
      EXPECT_EQ(res, H265Parser::kOk);
    }
  }
}

TEST_F(H265ParserTest, VpsParsing) {
  LoadParserFile("bear.hevc");
  H265NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::VPS_NUT));
  int vps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseVPS(&vps_id));
  const H265VPS* vps = parser_.GetVPS(vps_id);
  EXPECT_TRUE(!!vps);
  EXPECT_TRUE(vps->vps_base_layer_internal_flag);
  EXPECT_TRUE(vps->vps_base_layer_available_flag);
  EXPECT_EQ(vps->vps_max_layers_minus1, 0);
  EXPECT_EQ(vps->vps_max_sub_layers_minus1, 0);
  EXPECT_TRUE(vps->vps_temporal_id_nesting_flag);
  EXPECT_EQ(vps->profile_tier_level.general_profile_idc, 1);
  EXPECT_EQ(vps->profile_tier_level.general_level_idc, 60);
  EXPECT_EQ(vps->vps_max_dec_pic_buffering_minus1[0], 4);
  EXPECT_EQ(vps->vps_max_num_reorder_pics[0], 2);
  EXPECT_EQ(vps->vps_max_latency_increase_plus1[0], 0);
  for (int i = 1; i < kMaxSubLayers; ++i) {
    EXPECT_EQ(vps->vps_max_dec_pic_buffering_minus1[i], 0);
    EXPECT_EQ(vps->vps_max_num_reorder_pics[i], 0);
    EXPECT_EQ(vps->vps_max_latency_increase_plus1[i], 0);
  }
  EXPECT_EQ(vps->vps_max_layer_id, 0);
  EXPECT_EQ(vps->vps_num_layer_sets_minus1, 0);
}

TEST_F(H265ParserTest, VpsAlphaLayerId) {
  constexpr uint8_t kStream[] = {
      // Start code.
      0x00,
      0x00,
      0x00,
      0x01,
      // NALU type = 32 (VPS).
      0x40,
      0x01,
      0x0c,
      0x11,
      0xff,
      0xff,
      0x01,
      0x60,
      0x00,
      0x00,
      0x03,
      0x00,
      0xb0,
      0x00,
      0x00,
      0x03,
      0x00,
      0x00,
      0x03,
      0x00,
      0x3e,
      0x19,
      0x40,
      0xbf,
      0x3e,
      0x08,
      0x00,
      0x08,
      0x30,
      0x20,
      0xa4,
      0x00,
      0x00,
      0x03,
      0x00,
      0x00,
      0x03,
      0x00,
      0xc5,
      0x20,
  };

  H265Parser parser;
  parser.SetStream(kStream, std::size(kStream));

  H265NALU target_nalu;
  ASSERT_EQ(H265Parser::kOk, parser.AdvanceToNextNALU(&target_nalu));
  ASSERT_EQ(target_nalu.nal_unit_type, H265NALU::VPS_NUT);

  int vps_id;
  ASSERT_EQ(H265Parser::kOk, parser.ParseVPS(&vps_id));

  const H265VPS* vps = parser.GetVPS(vps_id);
  EXPECT_EQ(vps->aux_alpha_layer_id, 1);
}

TEST_F(H265ParserTest, SpsParsing) {
  LoadParserFile("bear.hevc");
  H265NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::SPS_NUT));
  int sps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseSPS(&sps_id));
  const H265SPS* sps = parser_.GetSPS(sps_id);
  EXPECT_TRUE(!!sps);
  EXPECT_EQ(sps->sps_max_sub_layers_minus1, 0);
  EXPECT_EQ(sps->profile_tier_level.general_profile_idc, 1);
  EXPECT_EQ(sps->profile_tier_level.general_level_idc, 60);
  EXPECT_EQ(sps->sps_seq_parameter_set_id, 0);
  EXPECT_EQ(sps->chroma_format_idc, 1);
  EXPECT_FALSE(sps->separate_colour_plane_flag);
  EXPECT_EQ(sps->pic_width_in_luma_samples, 320);
  EXPECT_EQ(sps->pic_height_in_luma_samples, 184);
  EXPECT_EQ(sps->conf_win_left_offset, 0);
  EXPECT_EQ(sps->conf_win_right_offset, 0);
  EXPECT_EQ(sps->conf_win_top_offset, 0);
  EXPECT_EQ(sps->conf_win_bottom_offset, 2);
  EXPECT_EQ(sps->bit_depth_luma_minus8, 0);
  EXPECT_EQ(sps->bit_depth_chroma_minus8, 0);
  EXPECT_EQ(sps->log2_max_pic_order_cnt_lsb_minus4, 4);
  EXPECT_EQ(sps->sps_max_dec_pic_buffering_minus1[0], 4);
  EXPECT_EQ(sps->sps_max_num_reorder_pics[0], 2);
  EXPECT_EQ(sps->sps_max_latency_increase_plus1[0], 0u);
  for (int i = 1; i < kMaxSubLayers; ++i) {
    EXPECT_EQ(sps->sps_max_dec_pic_buffering_minus1[i], 0);
    EXPECT_EQ(sps->sps_max_num_reorder_pics[i], 0);
    EXPECT_EQ(sps->sps_max_latency_increase_plus1[i], 0u);
  }
  EXPECT_EQ(sps->log2_min_luma_coding_block_size_minus3, 0);
  EXPECT_EQ(sps->log2_diff_max_min_luma_coding_block_size, 3);
  EXPECT_EQ(sps->log2_min_luma_transform_block_size_minus2, 0);
  EXPECT_EQ(sps->log2_diff_max_min_luma_transform_block_size, 3);
  EXPECT_EQ(sps->max_transform_hierarchy_depth_inter, 0);
  EXPECT_EQ(sps->max_transform_hierarchy_depth_intra, 0);
  EXPECT_FALSE(sps->scaling_list_enabled_flag);
  EXPECT_FALSE(sps->sps_scaling_list_data_present_flag);
  EXPECT_FALSE(sps->amp_enabled_flag);
  EXPECT_TRUE(sps->sample_adaptive_offset_enabled_flag);
  EXPECT_FALSE(sps->pcm_enabled_flag);
  EXPECT_EQ(sps->pcm_sample_bit_depth_luma_minus1, 0);
  EXPECT_EQ(sps->pcm_sample_bit_depth_chroma_minus1, 0);
  EXPECT_EQ(sps->log2_min_pcm_luma_coding_block_size_minus3, 0);
  EXPECT_EQ(sps->log2_diff_max_min_pcm_luma_coding_block_size, 0);
  EXPECT_FALSE(sps->pcm_loop_filter_disabled_flag);
  EXPECT_EQ(sps->num_short_term_ref_pic_sets, 0);
  EXPECT_FALSE(sps->long_term_ref_pics_present_flag);
  EXPECT_EQ(sps->num_long_term_ref_pics_sps, 0);
  EXPECT_TRUE(sps->sps_temporal_mvp_enabled_flag);
  EXPECT_TRUE(sps->strong_intra_smoothing_enabled_flag);
  EXPECT_EQ(sps->vui_parameters.sar_width, 0);
  EXPECT_EQ(sps->vui_parameters.sar_height, 0);
  EXPECT_EQ(sps->vui_parameters.video_full_range_flag, 0);
  EXPECT_EQ(sps->vui_parameters.colour_description_present_flag, 0);
  EXPECT_EQ(sps->vui_parameters.colour_primaries, 0);
  EXPECT_EQ(sps->vui_parameters.transfer_characteristics, 0);
  EXPECT_EQ(sps->vui_parameters.matrix_coeffs, 0);
  EXPECT_EQ(sps->vui_parameters.def_disp_win_left_offset, 0);
  EXPECT_EQ(sps->vui_parameters.def_disp_win_right_offset, 0);
  EXPECT_EQ(sps->vui_parameters.def_disp_win_top_offset, 0);
  EXPECT_EQ(sps->vui_parameters.def_disp_win_bottom_offset, 0);
}

TEST_F(H265ParserTest, PpsParsing) {
  LoadParserFile("bear.hevc");
  H265NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::SPS_NUT));
  int sps_id;
  // We need to parse the SPS so the PPS can find it.
  EXPECT_EQ(H265Parser::kOk, parser_.ParseSPS(&sps_id));
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::PPS_NUT));
  int pps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));
  const H265PPS* pps = parser_.GetPPS(pps_id);
  EXPECT_TRUE(!!pps);
  EXPECT_EQ(pps->pps_pic_parameter_set_id, 0);
  EXPECT_EQ(pps->pps_seq_parameter_set_id, 0);
  EXPECT_FALSE(pps->dependent_slice_segments_enabled_flag);
  EXPECT_FALSE(pps->output_flag_present_flag);
  EXPECT_EQ(pps->num_extra_slice_header_bits, 0);
  EXPECT_TRUE(pps->sign_data_hiding_enabled_flag);
  EXPECT_FALSE(pps->cabac_init_present_flag);
  EXPECT_EQ(pps->num_ref_idx_l0_default_active_minus1, 0);
  EXPECT_EQ(pps->num_ref_idx_l1_default_active_minus1, 0);
  EXPECT_EQ(pps->init_qp_minus26, 0);
  EXPECT_FALSE(pps->constrained_intra_pred_flag);
  EXPECT_FALSE(pps->transform_skip_enabled_flag);
  EXPECT_TRUE(pps->cu_qp_delta_enabled_flag);
  EXPECT_EQ(pps->diff_cu_qp_delta_depth, 0);
  EXPECT_EQ(pps->pps_cb_qp_offset, 0);
  EXPECT_EQ(pps->pps_cr_qp_offset, 0);
  EXPECT_FALSE(pps->pps_slice_chroma_qp_offsets_present_flag);
  EXPECT_TRUE(pps->weighted_pred_flag);
  EXPECT_FALSE(pps->weighted_bipred_flag);
  EXPECT_FALSE(pps->transquant_bypass_enabled_flag);
  EXPECT_FALSE(pps->tiles_enabled_flag);
  EXPECT_TRUE(pps->entropy_coding_sync_enabled_flag);
  EXPECT_TRUE(pps->loop_filter_across_tiles_enabled_flag);
  EXPECT_FALSE(pps->pps_scaling_list_data_present_flag);
  EXPECT_FALSE(pps->lists_modification_present_flag);
  EXPECT_EQ(pps->log2_parallel_merge_level_minus2, 0);
  EXPECT_FALSE(pps->slice_segment_header_extension_present_flag);
}

TEST_F(H265ParserTest, SliceHeaderParsing) {
  LoadParserFile("bear.hevc");
  H265NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::VPS_NUT));
  int vps_id;
  // We need to parse the VPS/SPS/PPS so the slice header can find them.
  EXPECT_EQ(H265Parser::kOk, parser_.ParseVPS(&vps_id));
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::SPS_NUT));
  int sps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseSPS(&sps_id));
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::PPS_NUT));
  int pps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));

  // Do an IDR slice header first.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::IDR_W_RADL));
  H265SliceHeader shdr;
  EXPECT_EQ(H265Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, &shdr, nullptr));
  EXPECT_TRUE(shdr.first_slice_segment_in_pic_flag);
  EXPECT_FALSE(shdr.no_output_of_prior_pics_flag);
  EXPECT_EQ(shdr.slice_pic_parameter_set_id, 0);
  EXPECT_FALSE(shdr.dependent_slice_segment_flag);
  EXPECT_EQ(shdr.slice_type, H265SliceHeader::kSliceTypeI);
  EXPECT_TRUE(shdr.slice_sao_luma_flag);
  EXPECT_TRUE(shdr.slice_sao_chroma_flag);
  EXPECT_EQ(shdr.slice_qp_delta, 8);
  EXPECT_TRUE(shdr.slice_loop_filter_across_slices_enabled_flag);

  // Then do a non-IDR slice header.
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::TRAIL_R));
  EXPECT_EQ(H265Parser::kOk,
            parser_.ParseSliceHeader(target_nalu, &shdr, nullptr));
  EXPECT_TRUE(shdr.first_slice_segment_in_pic_flag);
  EXPECT_EQ(shdr.slice_pic_parameter_set_id, 0);
  EXPECT_FALSE(shdr.dependent_slice_segment_flag);
  EXPECT_EQ(shdr.slice_type, H265SliceHeader::kSliceTypeP);
  EXPECT_EQ(shdr.slice_pic_order_cnt_lsb, 4);
  EXPECT_FALSE(shdr.short_term_ref_pic_set_sps_flag);
  EXPECT_EQ(shdr.st_ref_pic_set.num_negative_pics, 1);
  EXPECT_EQ(shdr.st_ref_pic_set.num_positive_pics, 0);
  EXPECT_EQ(shdr.st_ref_pic_set.delta_poc_s0[0], -4);
  EXPECT_EQ(shdr.st_ref_pic_set.used_by_curr_pic_s0[0], 1);
  EXPECT_TRUE(shdr.slice_temporal_mvp_enabled_flag);
  EXPECT_TRUE(shdr.slice_sao_luma_flag);
  EXPECT_TRUE(shdr.slice_sao_chroma_flag);
  EXPECT_FALSE(shdr.num_ref_idx_active_override_flag);
  EXPECT_EQ(shdr.pred_weight_table.luma_log2_weight_denom, 0);
  EXPECT_EQ(shdr.pred_weight_table.delta_chroma_log2_weight_denom, 7);
  EXPECT_EQ(shdr.pred_weight_table.delta_luma_weight_l0[0], 0);
  EXPECT_EQ(shdr.pred_weight_table.luma_offset_l0[0], -2);
  EXPECT_EQ(shdr.pred_weight_table.delta_chroma_weight_l0[0][0], -9);
  EXPECT_EQ(shdr.pred_weight_table.delta_chroma_weight_l0[0][1], -9);
  EXPECT_EQ(shdr.pred_weight_table.delta_chroma_offset_l0[0][0], 0);
  EXPECT_EQ(shdr.pred_weight_table.delta_chroma_offset_l0[0][1], 0);
  EXPECT_EQ(shdr.five_minus_max_num_merge_cand, 3);
  EXPECT_EQ(shdr.slice_qp_delta, 8);
  EXPECT_TRUE(shdr.slice_loop_filter_across_slices_enabled_flag);
}

TEST_F(H265ParserTest, SliceHeaderParsingNoValidationOnFirstSliceInFrame) {
  LoadParserFile("bear.hevc");
  H265SliceHeader curr_slice_header;
  H265SliceHeader last_slice_header;

  while (true) {
    H265NALU nalu;
    H265Parser::Result result = parser_.AdvanceToNextNALU(&nalu);
    ASSERT_TRUE(result == H265Parser::kOk || result == H265Parser::kEOStream);
    if (result == H265Parser::kEOStream)
      break;

    switch (nalu.nal_unit_type) {
      case H265NALU::TRAIL_R:
        [[fallthrough]];
      case H265NALU::IDR_W_RADL:
        result = parser_.ParseSliceHeader(nalu, &curr_slice_header,
                                          &last_slice_header);
        EXPECT_EQ(result, H265Parser::kOk);
        last_slice_header = curr_slice_header;
        break;
      case H265NALU::SPS_NUT:
        int sps_id;
        EXPECT_EQ(parser_.ParseSPS(&sps_id), H265Parser::kOk);
        EXPECT_NE(parser_.GetSPS(sps_id), nullptr);
        break;
      case H265NALU::PPS_NUT:
        int pps_id;
        EXPECT_EQ(parser_.ParsePPS(nalu, &pps_id), H265Parser::kOk);
        EXPECT_NE(parser_.GetPPS(pps_id), nullptr);
        break;
      default:
        break;
    }
  }
}

TEST_F(H265ParserTest, HDRMetadataSEIParsing) {
  LoadParserFile("bear-1280x720-hevc-10bit-hdr10.hevc");
  H265NALU target_nalu;
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::VPS_NUT));
  int vps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseVPS(&vps_id));
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::SPS_NUT));
  int sps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseSPS(&sps_id));
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::PPS_NUT));
  int pps_id;
  EXPECT_EQ(H265Parser::kOk, parser_.ParsePPS(target_nalu, &pps_id));

  // Parse the next content light level info SEI
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::PREFIX_SEI_NUT));
  H265SEI clli_sei;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseSEI(&clli_sei));
  EXPECT_EQ(clli_sei.msgs.size(), 1u);
  for (auto& sei_msg : clli_sei.msgs) {
    EXPECT_EQ(sei_msg.type, H265SEIMessage::kSEIContentLightLevelInfo);
    EXPECT_EQ(sei_msg.content_light_level_info.max_content_light_level, 1000u);
    EXPECT_EQ(sei_msg.content_light_level_info.max_picture_average_light_level,
              400u);
  }

  // Parse the next mastering display colour volume info SEI
  EXPECT_TRUE(ParseNalusUntilNut(&target_nalu, H265NALU::PREFIX_SEI_NUT));
  H265SEI mdcv_sei;
  EXPECT_EQ(H265Parser::kOk, parser_.ParseSEI(&mdcv_sei));
  EXPECT_EQ(mdcv_sei.msgs.size(), 1u);
  for (auto& sei_msg : mdcv_sei.msgs) {
    EXPECT_EQ(sei_msg.type, H265SEIMessage::kSEIMasteringDisplayInfo);
    EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[0][0], 13250u);
    EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[0][1], 34500u);
    EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[1][0], 7500u);
    EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[1][1], 3000u);
    EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[2][0], 34000u);
    EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[2][1], 16000u);
    EXPECT_EQ(sei_msg.mastering_display_info.white_points[0], 15635u);
    EXPECT_EQ(sei_msg.mastering_display_info.white_points[1], 16450u);
    EXPECT_EQ(sei_msg.mastering_display_info.max_luminance, 10000000u);
    EXPECT_EQ(sei_msg.mastering_display_info.min_luminance, 500u);
  }
}

TEST_F(H265ParserTest, AlphaChannelInfoSEIParsing) {
  constexpr uint8_t kStream[] = {
      // Start code.
      0x00,
      0x00,
      0x01,
      // NALU type = 39 (PREFIX_SEI).
      0x4e,
      0x01,
      // SEI payload type = 137 (alpha_channel_info).
      0xa5,
      // SEI payload size = 4.
      0x04,
      // SEI payload.
      0x00,
      0x00,
      0x7f,
      0x90,
  };

  H265Parser parser;
  parser.SetStream(kStream, std::size(kStream));

  H265NALU target_nalu;
  ASSERT_EQ(H265Parser::kOk, parser.AdvanceToNextNALU(&target_nalu));
  EXPECT_EQ(target_nalu.nal_unit_type, H265NALU::PREFIX_SEI_NUT);

  // Recursively parse SEI.
  H265SEI alpha_sei;
  EXPECT_EQ(H265Parser::kOk, parser.ParseSEI(&alpha_sei));
  EXPECT_EQ(alpha_sei.msgs.size(), 1u);

  // Alpha channel info present.
  for (auto& sei_msg : alpha_sei.msgs) {
    EXPECT_EQ(sei_msg.type, H265SEIMessage::kSEIAlphaChannelInfo);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_channel_cancel_flag, 0);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_channel_use_idc, 0);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_channel_bit_depth_minus8, 0);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_transparent_value, 0);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_opaque_value, 255);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_channel_incr_flag, 0);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_channel_clip_flag, 0);
    EXPECT_EQ(sei_msg.alpha_channel_info.alpha_channel_clip_type_flag, 0);
  }
}

TEST_F(H265ParserTest, RecursiveSEIParsing) {
  constexpr uint8_t kStream[] = {
      // Start code.
      0x00,
      0x00,
      0x01,
      // NALU type = 39 (PREFIX_SEI).
      0x4e,
      0x01,
      // SEI payload type = 137 (mastering_display_colour_volume).
      0x89,
      // SEI payload size = 24.
      0x18,
      // SEI payload.
      0x33,
      0xc1,
      0x86,
      0xc3,
      0x1d,
      0x4c,
      0x0b,
      0xb7,
      0x84,
      0xd0,
      0x3e,
      0x7f,
      0x3d,
      0x13,
      0x40,
      0x41,
      0x00,
      0x98,
      0x96,
      0x80,
      0x00,
      0x00,
      // Skipped `0x03`.
      0x03,
      0x00,
      0x32,
      // SEI payload type = 144 (content_light_level_info).
      0x90,
      // SEI payload size = 4.
      0x04,
      // SEI payload.
      0x03,
      0xe8,
      0x00,
      0xc8,
  };

  H265Parser parser;
  parser.SetStream(kStream, std::size(kStream));

  H265NALU target_nalu;
  ASSERT_EQ(H265Parser::kOk, parser.AdvanceToNextNALU(&target_nalu));
  EXPECT_EQ(target_nalu.nal_unit_type, H265NALU::PREFIX_SEI_NUT);

  // Recursively parse SEI.
  H265SEI clli_mdcv_sei;
  EXPECT_EQ(H265Parser::kOk, parser.ParseSEI(&clli_mdcv_sei));
  EXPECT_EQ(clli_mdcv_sei.msgs.size(), 2u);

  for (auto& sei_msg : clli_mdcv_sei.msgs) {
    EXPECT_TRUE(sei_msg.type == H265SEIMessage::kSEIContentLightLevelInfo ||
                sei_msg.type == H265SEIMessage::kSEIMasteringDisplayInfo);
    switch (sei_msg.type) {
      case H265SEIMessage::kSEIContentLightLevelInfo:
        // Content light level info present.
        EXPECT_EQ(sei_msg.content_light_level_info.max_content_light_level,
                  1000u);
        EXPECT_EQ(
            sei_msg.content_light_level_info.max_picture_average_light_level,
            200u);
        break;
      case H265SEIMessage::kSEIMasteringDisplayInfo:
        EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[0][0],
                  13249u);
        EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[0][1],
                  34499u);
        EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[1][0],
                  7500u);
        EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[1][1],
                  2999u);
        EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[2][0],
                  34000u);
        EXPECT_EQ(sei_msg.mastering_display_info.display_primaries[2][1],
                  15999u);
        EXPECT_EQ(sei_msg.mastering_display_info.white_points[0], 15635u);
        EXPECT_EQ(sei_msg.mastering_display_info.white_points[1], 16449u);
        EXPECT_EQ(sei_msg.mastering_display_info.max_luminance, 10000000u);
        EXPECT_EQ(sei_msg.mastering_display_info.min_luminance, 50u);
        break;
      default:
        break;
    }
  }
}

}  // namespace media
