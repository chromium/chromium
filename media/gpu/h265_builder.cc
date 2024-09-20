// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/h265_builder.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"

namespace media {

void BuildPackedH265ProfileTierLevel(
    H26xAnnexBBitstreamBuilder& builder,
    const H265ProfileTierLevel& profile_tier_level,
    bool profile_present_flag,
    uint8_t max_num_sub_layers_minus1) {
  // 7.3.3 Profile, tier and level syntax
  if (profile_present_flag) {
    builder.AppendBits(2, 0);  // general_profile_space
    builder.AppendBits(1, 0);  // general_tier_flag
    builder.AppendBits(5, profile_tier_level.general_profile_idc);
    builder.AppendBits(32,
                       profile_tier_level.general_profile_compatibility_flags);
    builder.AppendBits(1, profile_tier_level.general_progressive_source_flag);
    builder.AppendBits(1, profile_tier_level.general_interlaced_source_flag);
    builder.AppendBits(1,
                       profile_tier_level.general_non_packed_constraint_flag);
    builder.AppendBits(1,
                       profile_tier_level.general_frame_only_constraint_flag);
    CHECK_LT(profile_tier_level.general_profile_idc, 4);
    // Check general_profile_compatibility_flag[ 2 ] == 0
    CHECK(!(profile_tier_level.general_profile_compatibility_flags & 1 << 29));
    builder.AppendBits(43, 0);  // general_reserved_zero_43bits
    builder.AppendBits(1, 0);   // general_inbld_flag
  }
  builder.AppendBits(8, profile_tier_level.general_level_idc);
  CHECK_EQ(max_num_sub_layers_minus1, 0);
}

void BuildPackedH265VPS(H26xAnnexBBitstreamBuilder& builder,
                        const H265VPS& vps) {
  // 7.3.2.1 Video parameter set RBSP syntax
  builder.BeginNALU(H265NALU::VPS_NUT);

  builder.AppendBits(4, vps.vps_video_parameter_set_id);
  builder.AppendBits(1, vps.vps_base_layer_internal_flag);
  builder.AppendBits(1, vps.vps_base_layer_available_flag);
  builder.AppendBits(6, vps.vps_max_layers_minus1);
  builder.AppendBits(3, vps.vps_max_sub_layers_minus1);
  builder.AppendBits(1, vps.vps_temporal_id_nesting_flag);
  builder.AppendBits(16, 0xffff);  // vps_reserved_0xffff_16bits

  BuildPackedH265ProfileTierLevel(builder, vps.profile_tier_level, true,
                                  vps.vps_max_sub_layers_minus1);

  builder.AppendBits(1, 0);  // vps_sub_layer_ordering_info_present_flag
  {
    int i = vps.vps_max_sub_layers_minus1;
    builder.AppendUE(vps.vps_max_dec_pic_buffering_minus1[i]);
    builder.AppendUE(vps.vps_max_num_reorder_pics[i]);
    builder.AppendUE(vps.vps_max_latency_increase_plus1[i]);
  }
  builder.AppendBits(6, vps.vps_max_layer_id);
  CHECK_EQ(vps.vps_num_layer_sets_minus1, 0);
  builder.AppendUE(vps.vps_num_layer_sets_minus1);

  builder.AppendBits(1, 0);  // vps_timing_info_present_flag
  builder.AppendBits(1, 0);  // vps_extension_flag

  builder.FinishNALU();
}

void BuildPackedH265SPS(H26xAnnexBBitstreamBuilder& builder,
                        const H265SPS& sps) {
  // 7.3.2.2.1 General sequence parameter set RBSP syntax
  builder.BeginNALU(H265NALU::SPS_NUT);

  builder.AppendBits(4, sps.sps_video_parameter_set_id);
  builder.AppendBits(3, sps.sps_max_sub_layers_minus1);
  builder.AppendBits(1, sps.sps_temporal_id_nesting_flag);
  BuildPackedH265ProfileTierLevel(builder, sps.profile_tier_level, true,
                                  sps.sps_max_sub_layers_minus1);

  builder.AppendUE(sps.sps_seq_parameter_set_id);
  builder.AppendUE(sps.chroma_format_idc);
  if (sps.chroma_format_idc == 3) {
    builder.AppendBits(1, sps.separate_colour_plane_flag);
  }
  builder.AppendUE(sps.pic_width_in_luma_samples);
  builder.AppendUE(sps.pic_height_in_luma_samples);

  bool conformance_window_flag =
      sps.conf_win_left_offset > 0 || sps.conf_win_right_offset > 0 ||
      sps.conf_win_top_offset > 0 || sps.conf_win_bottom_offset > 0;
  builder.AppendBits(1, conformance_window_flag);
  if (conformance_window_flag) {
    builder.AppendUE(sps.conf_win_left_offset);
    builder.AppendUE(sps.conf_win_right_offset);
    builder.AppendUE(sps.conf_win_top_offset);
    builder.AppendUE(sps.conf_win_bottom_offset);
  }

  builder.AppendUE(sps.bit_depth_luma_minus8);
  builder.AppendUE(sps.bit_depth_chroma_minus8);
  builder.AppendUE(sps.log2_max_pic_order_cnt_lsb_minus4);

  builder.AppendBits(1, 0);  // sps_sub_layer_ordering_info_present_flag
  {
    int i = sps.sps_max_sub_layers_minus1;
    builder.AppendUE(sps.sps_max_dec_pic_buffering_minus1[i]);
    builder.AppendUE(sps.sps_max_num_reorder_pics[i]);
    builder.AppendUE(sps.sps_max_latency_increase_plus1[i]);
  }

  builder.AppendUE(sps.log2_min_luma_coding_block_size_minus3);
  builder.AppendUE(sps.log2_diff_max_min_luma_coding_block_size);
  builder.AppendUE(sps.log2_min_luma_transform_block_size_minus2);
  builder.AppendUE(sps.log2_diff_max_min_luma_transform_block_size);
  builder.AppendUE(sps.max_transform_hierarchy_depth_inter);
  builder.AppendUE(sps.max_transform_hierarchy_depth_intra);

  builder.AppendBits(1, sps.scaling_list_enabled_flag);
  CHECK(!sps.scaling_list_enabled_flag);
  builder.AppendBits(1, sps.amp_enabled_flag);
  builder.AppendBits(1, sps.sample_adaptive_offset_enabled_flag);

  builder.AppendBits(1, sps.pcm_enabled_flag);
  if (sps.pcm_enabled_flag) {
    builder.AppendBits(4, sps.pcm_sample_bit_depth_luma_minus1);
    builder.AppendBits(4, sps.pcm_sample_bit_depth_chroma_minus1);
    builder.AppendUE(sps.log2_min_pcm_luma_coding_block_size_minus3);
    builder.AppendUE(sps.log2_diff_max_min_pcm_luma_coding_block_size);
    builder.AppendBits(1, sps.pcm_loop_filter_disabled_flag);
  }

  builder.AppendUE(sps.num_short_term_ref_pic_sets);
  CHECK_EQ(sps.num_short_term_ref_pic_sets, 0);
  builder.AppendBits(1, sps.long_term_ref_pics_present_flag);
  if (sps.long_term_ref_pics_present_flag) {
    builder.AppendUE(sps.num_long_term_ref_pics_sps);
    for (int i = 0; i < sps.num_long_term_ref_pics_sps; i++) {
      builder.AppendBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4,
                         sps.lt_ref_pic_poc_lsb_sps[i]);
      builder.AppendBits(1, sps.used_by_curr_pic_lt_sps_flag[i]);
    }
  }

  builder.AppendBits(1, sps.sps_temporal_mvp_enabled_flag);
  builder.AppendBits(1, sps.strong_intra_smoothing_enabled_flag);
  builder.AppendBits(1, 0);  // vui_parameters_present_flag
  builder.AppendBits(1, 0);  // sps_extension_present_flag

  builder.FinishNALU();
}

void BuildPackedH265PPS(H26xAnnexBBitstreamBuilder& builder,
                        const H265PPS& pps) {
  // 7.3.2.3.1 General picture parameter set RBSP syntax
  builder.BeginNALU(H265NALU::PPS_NUT);

  builder.AppendUE(pps.pps_pic_parameter_set_id);
  builder.AppendUE(pps.pps_seq_parameter_set_id);
  builder.AppendBits(1, pps.dependent_slice_segments_enabled_flag);
  builder.AppendBits(1, pps.output_flag_present_flag);
  builder.AppendBits(3, pps.num_extra_slice_header_bits);
  builder.AppendBits(1, pps.sign_data_hiding_enabled_flag);
  builder.AppendBits(1, pps.cabac_init_present_flag);

  builder.AppendUE(pps.num_ref_idx_l0_default_active_minus1);
  builder.AppendUE(pps.num_ref_idx_l1_default_active_minus1);
  builder.AppendSE(pps.init_qp_minus26);

  builder.AppendBits(1, pps.constrained_intra_pred_flag);
  builder.AppendBits(1, pps.transform_skip_enabled_flag);

  builder.AppendBits(1, pps.cu_qp_delta_enabled_flag);
  if (pps.cu_qp_delta_enabled_flag) {
    builder.AppendUE(pps.diff_cu_qp_delta_depth);
  }

  builder.AppendSE(pps.pps_cb_qp_offset);
  builder.AppendSE(pps.pps_cr_qp_offset);

  builder.AppendBits(1, pps.pps_slice_chroma_qp_offsets_present_flag);
  builder.AppendBits(1, pps.weighted_pred_flag);
  builder.AppendBits(1, pps.weighted_bipred_flag);
  builder.AppendBits(1, pps.transquant_bypass_enabled_flag);

  builder.AppendBits(1, pps.tiles_enabled_flag);
  builder.AppendBits(1, pps.entropy_coding_sync_enabled_flag);
  if (pps.tiles_enabled_flag) {
    builder.AppendUE(pps.num_tile_columns_minus1);
    builder.AppendUE(pps.num_tile_rows_minus1);
    builder.AppendBits(1, pps.uniform_spacing_flag);
    if (!pps.uniform_spacing_flag) {
      for (int i = 0; i < pps.num_tile_columns_minus1; i++) {
        builder.AppendUE(pps.column_width_minus1[i]);
      }
      for (int i = 0; i < pps.num_tile_rows_minus1; i++) {
        builder.AppendUE(pps.row_height_minus1[i]);
      }
    }
    builder.AppendBits(1, pps.loop_filter_across_tiles_enabled_flag);
  }

  builder.AppendBits(1, pps.pps_loop_filter_across_slices_enabled_flag);
  builder.AppendBits(1, pps.deblocking_filter_control_present_flag);
  if (pps.deblocking_filter_control_present_flag) {
    builder.AppendBits(1, pps.deblocking_filter_override_enabled_flag);
    builder.AppendBits(1, pps.pps_deblocking_filter_disabled_flag);
    if (!pps.pps_deblocking_filter_disabled_flag) {
      builder.AppendSE(pps.pps_beta_offset_div2);
      builder.AppendSE(pps.pps_tc_offset_div2);
    }
  }

  builder.AppendBits(1, pps.pps_scaling_list_data_present_flag);
  CHECK(!pps.pps_scaling_list_data_present_flag);

  builder.AppendBits(1, pps.lists_modification_present_flag);
  builder.AppendUE(pps.log2_parallel_merge_level_minus2);
  builder.AppendBits(1, pps.slice_segment_header_extension_present_flag);

  builder.AppendBits(1, pps.pps_extension_present_flag);
  CHECK(!pps.pps_extension_present_flag);

  builder.FinishNALU();
}

}  // namespace media
