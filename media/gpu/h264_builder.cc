// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/h264_builder.h"

#include "base/bits.h"
#include "base/notreached.h"
#include "media/filters/h26x_annex_b_bitstream_builder.h"

namespace media {

void BuildPackedH264SPS(H26xAnnexBBitstreamBuilder& bitstream_builder,
                        const H264SPS& sps) {
  bitstream_builder.BeginNALU(H264NALU::kSPS, 3);

  bitstream_builder.AppendBits(8, sps.profile_idc);
  bitstream_builder.AppendBool(sps.constraint_set0_flag);
  bitstream_builder.AppendBool(sps.constraint_set1_flag);
  bitstream_builder.AppendBool(sps.constraint_set2_flag);
  bitstream_builder.AppendBool(sps.constraint_set3_flag);
  bitstream_builder.AppendBool(sps.constraint_set4_flag);
  bitstream_builder.AppendBool(sps.constraint_set5_flag);
  bitstream_builder.AppendBits(2, 0);  // reserved_zero_2bits
  bitstream_builder.AppendBits(8, sps.level_idc);
  bitstream_builder.AppendUE(sps.seq_parameter_set_id);

  if (sps.profile_idc == H264SPS::kProfileIDCHigh) {
    bitstream_builder.AppendUE(sps.chroma_format_idc);
    if (sps.chroma_format_idc == 3) {
      bitstream_builder.AppendBool(sps.separate_colour_plane_flag);
    }
    bitstream_builder.AppendUE(sps.bit_depth_luma_minus8);
    bitstream_builder.AppendUE(sps.bit_depth_chroma_minus8);
    bitstream_builder.AppendBool(sps.qpprime_y_zero_transform_bypass_flag);
    bitstream_builder.AppendBool(sps.seq_scaling_matrix_present_flag);
    CHECK(!sps.seq_scaling_matrix_present_flag);
  }

  bitstream_builder.AppendUE(sps.log2_max_frame_num_minus4);
  bitstream_builder.AppendUE(sps.pic_order_cnt_type);
  if (sps.pic_order_cnt_type == 0) {
    bitstream_builder.AppendUE(sps.log2_max_pic_order_cnt_lsb_minus4);
  } else if (sps.pic_order_cnt_type == 1) {
    // Ignoring the content of this branch as we don't produce
    // sps.pic_order_cnt_type == 1
    NOTREACHED();
  }

  bitstream_builder.AppendUE(sps.max_num_ref_frames);
  bitstream_builder.AppendBool(sps.gaps_in_frame_num_value_allowed_flag);
  bitstream_builder.AppendUE(sps.pic_width_in_mbs_minus1);
  bitstream_builder.AppendUE(sps.pic_height_in_map_units_minus1);

  bitstream_builder.AppendBool(sps.frame_mbs_only_flag);
  if (!sps.frame_mbs_only_flag) {
    bitstream_builder.AppendBool(sps.mb_adaptive_frame_field_flag);
  }

  bitstream_builder.AppendBool(sps.direct_8x8_inference_flag);

  bitstream_builder.AppendBool(sps.frame_cropping_flag);
  if (sps.frame_cropping_flag) {
    bitstream_builder.AppendUE(sps.frame_crop_left_offset);
    bitstream_builder.AppendUE(sps.frame_crop_right_offset);
    bitstream_builder.AppendUE(sps.frame_crop_top_offset);
    bitstream_builder.AppendUE(sps.frame_crop_bottom_offset);
  }

  bitstream_builder.AppendBool(sps.vui_parameters_present_flag);
  if (sps.vui_parameters_present_flag) {
    bitstream_builder.AppendBool(false);  // aspect_ratio_info_present_flag
    bitstream_builder.AppendBool(false);  // overscan_info_present_flag
    bitstream_builder.AppendBool(false);  // video_signal_type_present_flag
    bitstream_builder.AppendBool(false);  // chroma_loc_info_present_flag

    bitstream_builder.AppendBool(sps.timing_info_present_flag);
    if (sps.timing_info_present_flag) {
      bitstream_builder.AppendBits(32, sps.num_units_in_tick);
      bitstream_builder.AppendBits(32, sps.time_scale);
      bitstream_builder.AppendBool(sps.fixed_frame_rate_flag);
    }

    bitstream_builder.AppendBool(sps.nal_hrd_parameters_present_flag);
    if (sps.nal_hrd_parameters_present_flag) {
      bitstream_builder.AppendUE(sps.cpb_cnt_minus1);
      bitstream_builder.AppendBits(4, sps.bit_rate_scale);
      bitstream_builder.AppendBits(4, sps.cpb_size_scale);
      CHECK_LT(base::checked_cast<size_t>(sps.cpb_cnt_minus1),
               std::size(sps.bit_rate_value_minus1));
      for (int i = 0; i <= sps.cpb_cnt_minus1; ++i) {
        bitstream_builder.AppendUE(sps.bit_rate_value_minus1[i]);
        bitstream_builder.AppendUE(sps.cpb_size_value_minus1[i]);
        bitstream_builder.AppendBool(sps.cbr_flag[i]);
      }
      bitstream_builder.AppendBits(
          5, sps.initial_cpb_removal_delay_length_minus_1);
      bitstream_builder.AppendBits(5, sps.cpb_removal_delay_length_minus1);
      bitstream_builder.AppendBits(5, sps.dpb_output_delay_length_minus1);
      bitstream_builder.AppendBits(5, sps.time_offset_length);
    }

    bitstream_builder.AppendBool(false);  // vcl_hrd_parameters_flag
    if (sps.nal_hrd_parameters_present_flag) {
      bitstream_builder.AppendBool(sps.low_delay_hrd_flag);
    }

    bitstream_builder.AppendBool(false);  // pic_struct_present_flag
    bitstream_builder.AppendBool(true);   // bitstream_restriction_flag

    bitstream_builder.AppendBool(
        false);                      // motion_vectors_over_pic_boundaries_flag
    bitstream_builder.AppendUE(2);   // max_bytes_per_pic_denom
    bitstream_builder.AppendUE(1);   // max_bits_per_mb_denom
    bitstream_builder.AppendUE(16);  // log2_max_mv_length_horizontal
    bitstream_builder.AppendUE(16);  // log2_max_mv_length_vertical

    // Explicitly set max_num_reorder_frames to 0 to allow the decoder to
    // output pictures early.
    bitstream_builder.AppendUE(0);  // max_num_reorder_frames

    // The value of max_dec_frame_buffering shall be greater than or equal to
    // max_num_ref_frames.
    const unsigned int max_dec_frame_buffering = sps.max_num_ref_frames;
    bitstream_builder.AppendUE(max_dec_frame_buffering);
  }

  bitstream_builder.FinishNALU();
}

void BuildPackedH264PPS(H26xAnnexBBitstreamBuilder& bitstream_builder,
                        const H264SPS& sps,
                        const H264PPS& pps) {
  bitstream_builder.BeginNALU(H264NALU::kPPS, 3);

  bitstream_builder.AppendUE(pps.pic_parameter_set_id);
  bitstream_builder.AppendUE(pps.seq_parameter_set_id);
  bitstream_builder.AppendBool(pps.entropy_coding_mode_flag);
  bitstream_builder.AppendBool(
      pps.bottom_field_pic_order_in_frame_present_flag);
  CHECK_EQ(pps.num_slice_groups_minus1, 0);
  bitstream_builder.AppendUE(pps.num_slice_groups_minus1);

  bitstream_builder.AppendUE(pps.num_ref_idx_l0_default_active_minus1);
  bitstream_builder.AppendUE(pps.num_ref_idx_l1_default_active_minus1);

  bitstream_builder.AppendBool(pps.weighted_pred_flag);
  bitstream_builder.AppendBits(2, pps.weighted_bipred_idc);

  bitstream_builder.AppendSE(pps.pic_init_qp_minus26);
  bitstream_builder.AppendSE(pps.pic_init_qs_minus26);
  bitstream_builder.AppendSE(pps.chroma_qp_index_offset);

  bitstream_builder.AppendBool(pps.deblocking_filter_control_present_flag);
  bitstream_builder.AppendBool(pps.constrained_intra_pred_flag);
  bitstream_builder.AppendBool(pps.redundant_pic_cnt_present_flag);

  if (sps.profile_idc >= H264SPS::kProfileIDCHigh) {
    bitstream_builder.AppendBool(pps.transform_8x8_mode_flag);
    bitstream_builder.AppendBool(pps.pic_scaling_matrix_present_flag);
    // Ignoring the scaling matrix branch as we don't produce it.
    CHECK(!pps.pic_scaling_matrix_present_flag);
    bitstream_builder.AppendSE(pps.second_chroma_qp_index_offset);
  }

  bitstream_builder.FinishNALU();
}

}  // namespace media
