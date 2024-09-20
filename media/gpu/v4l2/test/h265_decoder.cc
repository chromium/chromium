// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h265_decoder.h"

#include <linux/videodev2.h>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/gpu/macros.h"
#include "media/parsers/h265_parser.h"

namespace media {
namespace v4l2_test {

namespace {
constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_HEVC_SLICE;

// TODO(b/261127809): Find number of buffers in CAPTURE queue dynamically for
// H.265. |18| is the minimum number of buffers in the CAPTURE queue required to
// successfully decode all ITU-T H.264 baseline and main bitstreams.
constexpr uint32_t kNumberOfBuffersInCaptureQueue = 18;

struct POCAscCompare {
  bool operator()(const scoped_refptr<media::v4l2_test::H265Picture>& a,
                  const scoped_refptr<media::v4l2_test::H265Picture>& b) const {
    return a->pic_order_cnt_val_ < b->pic_order_cnt_val_;
  }
};

// Gets bit depth info from SPS
bool ParseBitDepth(const H265SPS& sps, uint8_t& bit_depth) {
  // Spec 7.4.3.2.1
  // See spec at http://www.itu.int/rec/T-REC-H.265
  if (sps.bit_depth_y != sps.bit_depth_c) {
    LOG(ERROR) << "Different bit depths among planes is not supported";
    return false;
  }
  bit_depth = base::checked_cast<uint8_t>(sps.bit_depth_y);
  return true;
}

// Checks bit depth is supported with the given HEVC profile
bool IsValidBitDepth(uint8_t bit_depth, VideoCodecProfile profile) {
  switch (profile) {
    // Spec A.3.2
    case HEVCPROFILE_MAIN:
      return bit_depth == 8u;
    // Spec A.3.3
    case HEVCPROFILE_MAIN10:
      return bit_depth == 8u || bit_depth == 10u;
    // Spec A.3.4
    case HEVCPROFILE_MAIN_STILL_PICTURE:
      return bit_depth == 8u;
    // Spec A.3.5
    case HEVCPROFILE_REXT:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 12u ||
             bit_depth == 14u || bit_depth == 16u;
    // Spec A.3.6
    case HEVCPROFILE_HIGH_THROUGHPUT:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 14u ||
             bit_depth == 16u;
    // Spec G.11.1.1
    case HEVCPROFILE_MULTIVIEW_MAIN:
      return bit_depth == 8u;
    // Spec H.11.1.1
    case HEVCPROFILE_SCALABLE_MAIN:
      return bit_depth == 8u || bit_depth == 10u;
    // Spec I.11.1.1
    case HEVCPROFILE_3D_MAIN:
      return bit_depth == 8u;
    // Spec A.3.7
    case HEVCPROFILE_SCREEN_EXTENDED:
      return bit_depth == 8u || bit_depth == 10u;
    // Spec H.11.1.2
    case HEVCPROFILE_SCALABLE_REXT:
      return bit_depth == 8u || bit_depth == 12u || bit_depth == 16u;
    // Spec A.3.8
    case HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED:
      return bit_depth == 8u || bit_depth == 10u || bit_depth == 14u;
    default:
      LOG(ERROR) << "Invalid profile specified for H265";
      return false;
  }
}

// Translates decoder SPS structure into |v4l2_ctrl_hevc_sps| structure.
v4l2_ctrl_hevc_sps SetupSPSCtrl(const H265SPS* sps) {
  struct v4l2_ctrl_hevc_sps v4l2_sps;
  memset(&v4l2_sps, 0, sizeof(v4l2_sps));

  int highest_tid = sps->sps_max_sub_layers_minus1;

  // Translates values using the |v4l2_ctrl_hevc_sps| struct order
  v4l2_sps.video_parameter_set_id = sps->sps_video_parameter_set_id;
  v4l2_sps.seq_parameter_set_id = sps->sps_seq_parameter_set_id;

#define SPS_TO_V4L2SPS(a) v4l2_sps.a = sps->a
  SPS_TO_V4L2SPS(pic_width_in_luma_samples);
  SPS_TO_V4L2SPS(pic_height_in_luma_samples);
  SPS_TO_V4L2SPS(bit_depth_luma_minus8);
  SPS_TO_V4L2SPS(bit_depth_chroma_minus8);
  SPS_TO_V4L2SPS(log2_max_pic_order_cnt_lsb_minus4);

#define SPS_TO_V4L2SPS_FROM_ARRAY(a) v4l2_sps.a = sps->a[highest_tid];
  SPS_TO_V4L2SPS_FROM_ARRAY(sps_max_dec_pic_buffering_minus1);
  SPS_TO_V4L2SPS_FROM_ARRAY(sps_max_num_reorder_pics);
  SPS_TO_V4L2SPS_FROM_ARRAY(sps_max_latency_increase_plus1);
#undef SPS_TO_V4L2SPS_FROM_ARRAY

  SPS_TO_V4L2SPS(log2_min_luma_coding_block_size_minus3);
  SPS_TO_V4L2SPS(log2_diff_max_min_luma_coding_block_size);
  SPS_TO_V4L2SPS(log2_min_luma_transform_block_size_minus2);
  SPS_TO_V4L2SPS(log2_diff_max_min_luma_transform_block_size);
  SPS_TO_V4L2SPS(max_transform_hierarchy_depth_inter);
  SPS_TO_V4L2SPS(max_transform_hierarchy_depth_intra);
  SPS_TO_V4L2SPS(pcm_sample_bit_depth_luma_minus1);
  SPS_TO_V4L2SPS(pcm_sample_bit_depth_chroma_minus1);
  SPS_TO_V4L2SPS(log2_min_pcm_luma_coding_block_size_minus3);
  SPS_TO_V4L2SPS(log2_diff_max_min_pcm_luma_coding_block_size);
  SPS_TO_V4L2SPS(num_short_term_ref_pic_sets);
  SPS_TO_V4L2SPS(num_long_term_ref_pics_sps);
  SPS_TO_V4L2SPS(chroma_format_idc);
  SPS_TO_V4L2SPS(sps_max_sub_layers_minus1);
#undef SPS_TO_V4L2SPS

#define SET_V4L2_SPS_FLAG_IF(cond, flag) \
  v4l2_sps.flags |= ((sps->cond) ? (flag) : 0)
  SET_V4L2_SPS_FLAG_IF(separate_colour_plane_flag,
                       V4L2_HEVC_SPS_FLAG_SEPARATE_COLOUR_PLANE);
  SET_V4L2_SPS_FLAG_IF(scaling_list_enabled_flag,
                       V4L2_HEVC_SPS_FLAG_SCALING_LIST_ENABLED);
  SET_V4L2_SPS_FLAG_IF(amp_enabled_flag, V4L2_HEVC_SPS_FLAG_AMP_ENABLED);
  SET_V4L2_SPS_FLAG_IF(sample_adaptive_offset_enabled_flag,
                       V4L2_HEVC_SPS_FLAG_SAMPLE_ADAPTIVE_OFFSET);
  SET_V4L2_SPS_FLAG_IF(pcm_enabled_flag, V4L2_HEVC_SPS_FLAG_PCM_ENABLED);
  SET_V4L2_SPS_FLAG_IF(pcm_loop_filter_disabled_flag,
                       V4L2_HEVC_SPS_FLAG_PCM_LOOP_FILTER_DISABLED);
  SET_V4L2_SPS_FLAG_IF(long_term_ref_pics_present_flag,
                       V4L2_HEVC_SPS_FLAG_LONG_TERM_REF_PICS_PRESENT);
  SET_V4L2_SPS_FLAG_IF(sps_temporal_mvp_enabled_flag,
                       V4L2_HEVC_SPS_FLAG_SPS_TEMPORAL_MVP_ENABLED);
  SET_V4L2_SPS_FLAG_IF(strong_intra_smoothing_enabled_flag,
                       V4L2_HEVC_SPS_FLAG_STRONG_INTRA_SMOOTHING_ENABLED);
#undef SET_V4L2_SPS_FLAG_IF

  return v4l2_sps;
}

// Translates decoder PPS structure into |v4l2_ctrl_hevc_pps| structure.
v4l2_ctrl_hevc_pps SetupPPSCtrl(const H265PPS* pps) {
  struct v4l2_ctrl_hevc_pps v4l2_pps;
  memset(&v4l2_pps, 0, sizeof(v4l2_pps));

  // Translates values using the |v4l2_ctrl_hevc_pps| struct order
#define PPS_TO_V4L2PPS(a) v4l2_pps.a = pps->a
  v4l2_pps.pic_parameter_set_id = pps->pps_pic_parameter_set_id;
  PPS_TO_V4L2PPS(num_extra_slice_header_bits);
  PPS_TO_V4L2PPS(num_ref_idx_l0_default_active_minus1);
  PPS_TO_V4L2PPS(num_ref_idx_l1_default_active_minus1);
  PPS_TO_V4L2PPS(init_qp_minus26);
  PPS_TO_V4L2PPS(diff_cu_qp_delta_depth);
  PPS_TO_V4L2PPS(pps_cb_qp_offset);
  PPS_TO_V4L2PPS(pps_cr_qp_offset);

  if (pps->tiles_enabled_flag) {
    PPS_TO_V4L2PPS(num_tile_columns_minus1);
    PPS_TO_V4L2PPS(num_tile_rows_minus1);

    if (!pps->uniform_spacing_flag) {
      static_assert(std::size(v4l2_pps.column_width_minus1) >=
                        std::extent<decltype(pps->column_width_minus1)>(),
                    "column_width_minus1 arrays must be same size");
      for (int i = 0; i <= pps->num_tile_columns_minus1; ++i) {
        v4l2_pps.column_width_minus1[i] = pps->column_width_minus1[i];
      }

      static_assert(std::size(v4l2_pps.row_height_minus1) >=
                        std::extent<decltype(pps->row_height_minus1)>(),
                    "row_height_minus1 arrays must be same size");
      for (int i = 0; i <= pps->num_tile_rows_minus1; ++i) {
        v4l2_pps.row_height_minus1[i] = pps->row_height_minus1[i];
      }
    }
  }

  PPS_TO_V4L2PPS(pps_beta_offset_div2);
  PPS_TO_V4L2PPS(pps_tc_offset_div2);
  PPS_TO_V4L2PPS(log2_parallel_merge_level_minus2);
#undef PPS_TO_V4L2PPS

#define SET_V4L2_PPS_FLAG_IF(cond, flag) \
  v4l2_pps.flags |= ((pps->cond) ? (flag) : 0)
  SET_V4L2_PPS_FLAG_IF(dependent_slice_segments_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_DEPENDENT_SLICE_SEGMENT_ENABLED);
  SET_V4L2_PPS_FLAG_IF(output_flag_present_flag,
                       V4L2_HEVC_PPS_FLAG_OUTPUT_FLAG_PRESENT);
  SET_V4L2_PPS_FLAG_IF(sign_data_hiding_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_SIGN_DATA_HIDING_ENABLED);
  SET_V4L2_PPS_FLAG_IF(cabac_init_present_flag,
                       V4L2_HEVC_PPS_FLAG_CABAC_INIT_PRESENT);
  SET_V4L2_PPS_FLAG_IF(constrained_intra_pred_flag,
                       V4L2_HEVC_PPS_FLAG_CONSTRAINED_INTRA_PRED);
  SET_V4L2_PPS_FLAG_IF(transform_skip_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_TRANSFORM_SKIP_ENABLED);
  SET_V4L2_PPS_FLAG_IF(cu_qp_delta_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_CU_QP_DELTA_ENABLED);
  SET_V4L2_PPS_FLAG_IF(pps_slice_chroma_qp_offsets_present_flag,
                       V4L2_HEVC_PPS_FLAG_PPS_SLICE_CHROMA_QP_OFFSETS_PRESENT);
  SET_V4L2_PPS_FLAG_IF(weighted_pred_flag, V4L2_HEVC_PPS_FLAG_WEIGHTED_PRED);
  SET_V4L2_PPS_FLAG_IF(weighted_bipred_flag,
                       V4L2_HEVC_PPS_FLAG_WEIGHTED_BIPRED);
  SET_V4L2_PPS_FLAG_IF(transquant_bypass_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_TRANSQUANT_BYPASS_ENABLED);
  SET_V4L2_PPS_FLAG_IF(tiles_enabled_flag, V4L2_HEVC_PPS_FLAG_TILES_ENABLED);
  SET_V4L2_PPS_FLAG_IF(entropy_coding_sync_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_ENTROPY_CODING_SYNC_ENABLED);
  SET_V4L2_PPS_FLAG_IF(loop_filter_across_tiles_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_LOOP_FILTER_ACROSS_TILES_ENABLED);
  SET_V4L2_PPS_FLAG_IF(
      pps_loop_filter_across_slices_enabled_flag,
      V4L2_HEVC_PPS_FLAG_PPS_LOOP_FILTER_ACROSS_SLICES_ENABLED);
  SET_V4L2_PPS_FLAG_IF(deblocking_filter_override_enabled_flag,
                       V4L2_HEVC_PPS_FLAG_DEBLOCKING_FILTER_OVERRIDE_ENABLED);
  SET_V4L2_PPS_FLAG_IF(pps_deblocking_filter_disabled_flag,
                       V4L2_HEVC_PPS_FLAG_PPS_DISABLE_DEBLOCKING_FILTER);
  SET_V4L2_PPS_FLAG_IF(lists_modification_present_flag,
                       V4L2_HEVC_PPS_FLAG_LISTS_MODIFICATION_PRESENT);
  SET_V4L2_PPS_FLAG_IF(
      slice_segment_header_extension_present_flag,
      V4L2_HEVC_PPS_FLAG_SLICE_SEGMENT_HEADER_EXTENSION_PRESENT);
  SET_V4L2_PPS_FLAG_IF(deblocking_filter_control_present_flag,
                       V4L2_HEVC_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT);
  SET_V4L2_PPS_FLAG_IF(uniform_spacing_flag,
                       V4L2_HEVC_PPS_FLAG_UNIFORM_SPACING);
#undef SET_V4L2_PPS_FLAG_IF

  return v4l2_pps;
}

// Builds the |v4l2_ctrl_hevc_scaling_matrix| structure and checks against SPS
// and PPS scaling matrix sizes.
v4l2_ctrl_hevc_scaling_matrix SetupScalingMatrix(const H265SPS* sps,
                                                 const H265PPS* pps) {
  struct v4l2_ctrl_hevc_scaling_matrix v4l2_scaling_matrix;
  memset(&v4l2_scaling_matrix, 0, sizeof(v4l2_scaling_matrix));
  struct H265ScalingListData checker;

  static_assert(
      std::size(checker.scaling_list_dc_coef_16x16) ==
              std::size(v4l2_scaling_matrix.scaling_list_dc_coef_16x16) &&
          std::size(checker.scaling_list_dc_coef_32x32) / 3 ==
              std::size(v4l2_scaling_matrix.scaling_list_dc_coef_32x32) &&
          std::size(checker.scaling_list_4x4) ==
              std::size(v4l2_scaling_matrix.scaling_list_4x4) &&
          std::size(checker.scaling_list_4x4[0]) ==
              std::size(v4l2_scaling_matrix.scaling_list_4x4[0]) &&
          std::size(checker.scaling_list_8x8) ==
              std::size(v4l2_scaling_matrix.scaling_list_8x8) &&
          std::size(checker.scaling_list_8x8[0]) ==
              std::size(v4l2_scaling_matrix.scaling_list_8x8[0]) &&
          std::size(checker.scaling_list_16x16) ==
              std::size(v4l2_scaling_matrix.scaling_list_16x16) &&
          std::size(checker.scaling_list_16x16[0]) ==
              std::size(v4l2_scaling_matrix.scaling_list_16x16[0]) &&
          std::size(checker.scaling_list_32x32) / 3 ==
              std::size(v4l2_scaling_matrix.scaling_list_32x32) &&
          std::size(checker.scaling_list_32x32[0]) ==
              std::size(v4l2_scaling_matrix.scaling_list_32x32[0]),
      "scaling_list_data must be of correct size");

  if (sps->scaling_list_enabled_flag) {
    // We already populated the scaling list data with default values in the
    // parser if they are not present in the stream, so just fill them all in.
    const auto& scaling_list = pps->pps_scaling_list_data_present_flag
                                   ? pps->scaling_list_data
                                   : sps->scaling_list_data;

    for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; ++i) {
      for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId0Count;
           ++j) {
        v4l2_scaling_matrix.scaling_list_4x4[i][j] =
            scaling_list.GetScalingList4x4EntryInRasterOrder(/*matrix_id=*/i,
                                                             /*raster_idx=*/j);
      }
    }

    for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; ++i) {
      for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId1To3Count;
           ++j) {
        v4l2_scaling_matrix.scaling_list_8x8[i][j] =
            scaling_list.GetScalingList8x8EntryInRasterOrder(/*matrix_id=*/i,
                                                             /*raster_idx=*/j);
      }
    }

    for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; ++i) {
      for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId1To3Count;
           ++j) {
        v4l2_scaling_matrix.scaling_list_16x16[i][j] =
            scaling_list.GetScalingList16x16EntryInRasterOrder(
                /*matrix_id=*/i,
                /*raster_idx=*/j);
      }
    }

    for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices;
         i += 3) {
      for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId1To3Count;
           ++j) {
        v4l2_scaling_matrix.scaling_list_32x32[i / 3][j] =
            scaling_list.GetScalingList32x32EntryInRasterOrder(
                /*matrix_id=*/i,
                /*raster_idx=*/j);
      }
    }

    memcpy(v4l2_scaling_matrix.scaling_list_dc_coef_16x16,
           scaling_list.scaling_list_dc_coef_16x16,
           sizeof(v4l2_scaling_matrix.scaling_list_dc_coef_16x16));
    v4l2_scaling_matrix.scaling_list_dc_coef_32x32[0] =
        scaling_list.scaling_list_dc_coef_32x32[0];
    v4l2_scaling_matrix.scaling_list_dc_coef_32x32[1] =
        scaling_list.scaling_list_dc_coef_32x32[3];
  }

  return v4l2_scaling_matrix;
}

struct v4l2_ctrl_hevc_decode_params SetupDecodeParams(
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> curr_pic) {
  struct v4l2_ctrl_hevc_decode_params v4l2_decode_params;
  memset(&v4l2_decode_params, 0, sizeof(v4l2_decode_params));

  v4l2_decode_params.pic_order_cnt_val = curr_pic->pic_order_cnt_val_,
  v4l2_decode_params.short_term_ref_pic_set_size =
      static_cast<__u16>(slice_hdr->st_rps_bits),
  v4l2_decode_params.long_term_ref_pic_set_size =
      static_cast<__u16>(slice_hdr->lt_rps_bits),
#if BUILDFLAG(IS_CHROMEOS)
  // .num_delta_pocs_of_ref_rps_idx is upstream but not yet pulled
  // into linux build sysroot.
  // TODO(b/261127809): Remove once linux-libc-dev package is updated to
  // at least v6.5 in the sysroots.
      v4l2_decode_params.num_delta_pocs_of_ref_rps_idx =
          static_cast<__u8>(slice_hdr->st_ref_pic_set.rps_idx_num_delta_pocs),
#endif
  v4l2_decode_params.flags = static_cast<__u64>(
      (curr_pic->irap_pic_ ? V4L2_HEVC_DECODE_PARAM_FLAG_IRAP_PIC : 0) |
      ((curr_pic->nal_unit_type_ >= H265NALU::IDR_W_RADL &&
        curr_pic->nal_unit_type_ <= H265NALU::IDR_N_LP)
           ? V4L2_HEVC_DECODE_PARAM_FLAG_IDR_PIC
           : 0) |
      (curr_pic->no_output_of_prior_pics_flag_
           ? V4L2_HEVC_DECODE_PARAM_FLAG_NO_OUTPUT_OF_PRIOR
           : 0)),

  memset(v4l2_decode_params.dpb, 0, sizeof(v4l2_decode_params.dpb));
  unsigned int i = 0;
  for (const auto& pic : ref_pic_list) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    // TODO(b/261127809): Handle |!pic->IsUnused()| case

    constexpr size_t kTimestampToNanoSecs = 1000;

    struct v4l2_hevc_dpb_entry& entry = v4l2_decode_params.dpb[i++];
    entry = {
        .timestamp = pic->ref_ts_nsec_ * kTimestampToNanoSecs,
        .flags = static_cast<__u8>(
            pic->IsLongTermRef() ? V4L2_HEVC_DPB_ENTRY_LONG_TERM_REFERENCE : 0),
        .field_pic = V4L2_HEVC_SEI_PIC_STRUCT_FRAME,  // No interlaced support
        .pic_order_cnt_val = pic->pic_order_cnt_val_,
    };
  }
  v4l2_decode_params.num_active_dpb_entries = i;

  // Set defaults
  std::fill_n(v4l2_decode_params.poc_st_curr_before,
              std::size(v4l2_decode_params.poc_st_curr_before), 0xff);
  std::fill_n(v4l2_decode_params.poc_st_curr_after,
              std::size(v4l2_decode_params.poc_st_curr_after), 0xff);
  std::fill_n(v4l2_decode_params.poc_lt_curr,
              std::size(v4l2_decode_params.poc_lt_curr), 0xff);

  i = 0;
  for (const auto& pic : ref_pic_set_st_curr_before) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < v4l2_decode_params.num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          v4l2_decode_params.dpb[j].pic_order_cnt_val) {
        v4l2_decode_params.poc_st_curr_before[i++] = j;
        break;
      }
    }
  }
  v4l2_decode_params.num_poc_st_curr_before = i;

  i = 0;
  for (const auto& pic : ref_pic_set_st_curr_after) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < v4l2_decode_params.num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          v4l2_decode_params.dpb[j].pic_order_cnt_val) {
        v4l2_decode_params.poc_st_curr_after[i++] = j;
        break;
      }
    }
  }
  v4l2_decode_params.num_poc_st_curr_after = i;

  i = 0;
  for (const auto& pic : ref_pic_set_lt_curr) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < v4l2_decode_params.num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          v4l2_decode_params.dpb[j].pic_order_cnt_val) {
        v4l2_decode_params.poc_lt_curr[i++] = j;
        break;
      }
    }
  }
  v4l2_decode_params.num_poc_lt_curr = i;

  return v4l2_decode_params;
}
}

H265Decoder::H265Decoder(std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                         gfx::Size display_resolution,
                         const base::MemoryMappedFile& data_stream)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl), display_resolution),
      data_stream_(data_stream) {}

H265Decoder::~H265Decoder() = default;

// static
std::unique_ptr<H265Decoder> H265Decoder::Create(
    const base::MemoryMappedFile& stream) {
  auto parser = std::make_unique<H265Parser>();
  parser->SetStream(stream.data(), stream.length());

  // Advance through NALUs until the first SPS.  The start of the decodable
  // data in an H.265 bistreams starts with an SPS.
  while (true) {
    H265NALU nalu;
    H265Parser::Result res = parser->AdvanceToNextNALU(&nalu);
    if (res != H265Parser::kOk) {
      LOG(ERROR) << "Unable to find SPS in stream";
      return nullptr;
    }

    if (nalu.nal_unit_type == H265NALU::SPS_NUT) {
      break;
    }
  }

  int sps_id;
  const H265Parser::Result parse_result = parser->ParseSPS(&sps_id);
  CHECK_EQ(parse_result, H265Parser::kOk);

  const H265SPS* sps = parser->GetSPS(sps_id);
  CHECK(sps);

  std::optional<gfx::Size> coded_size = sps->GetCodedSize();
  CHECK(coded_size);
  LOG(INFO) << "H.265 coded size : " << coded_size->ToString();

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>(kDriverCodecFourcc);

  return base::WrapUnique(
      new H265Decoder(std::move(v4l2_ioctl), coded_size.value(), stream));
}

bool H265Decoder::OutputAllRemainingPics() {
  // Output all pictures that are waiting to be outputted.
  H265Picture::Vector to_output;
  dpb_.AppendPendingOutputPics(&to_output);
  // Sort them by ascending POC to output in order.
  std::sort(to_output.begin(), to_output.end(), POCAscCompare());

  for (auto& pic : to_output) {
    if (!OutputPic(std::move(pic))) {
      return false;
    }
  }

  return true;
}

bool H265Decoder::Flush() {
  VLOGF(4) << "Decoder flush";

  if (!OutputAllRemainingPics()) {
    return false;
  }

  dpb_.Clear();
  prev_tid0_pic_ = nullptr;

  return true;
}

bool H265Decoder::ProcessPPS(int pps_id, bool* need_new_buffers) {
  VLOGF(4) << "Processing PPS id:" << pps_id;

  const H265PPS* pps = parser_->GetPPS(pps_id);
  DCHECK(pps);

  const H265SPS* sps = parser_->GetSPS(pps->pps_seq_parameter_set_id);
  DCHECK(sps);

  if (need_new_buffers) {
    *need_new_buffers = false;
  }

  gfx::Size new_pic_size = sps->GetCodedSize();
  gfx::Rect new_visible_rect = sps->GetVisibleRect();
  if (visible_rect_ != new_visible_rect) {
    VLOGF(4) << "New visible rect: " << new_visible_rect.ToString();
    visible_rect_ = new_visible_rect;
  }

  VideoChromaSampling new_chroma_sampling = sps->GetChromaSampling();
  if (new_chroma_sampling != VideoChromaSampling::k420) {
    LOG(ERROR) << "Only YUV 4:2:0 is supported";
    return false;
  }

  // Equation 7-8
  max_pic_order_cnt_lsb_ =
      std::pow(2, sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  VideoCodecProfile new_profile = H265Parser::ProfileIDCToVideoCodecProfile(
      sps->profile_tier_level.general_profile_idc);

  uint8_t new_bit_depth = 0;
  if (!ParseBitDepth(*sps, new_bit_depth)) {
    return false;
  }

  if (!IsValidBitDepth(new_bit_depth, new_profile)) {
    LOG(ERROR) << "Invalid bit depth=" << base::strict_cast<int>(new_bit_depth)
               << ", profile=" << GetProfileName(new_profile);
    return false;
  }

  if (pic_size_ != new_pic_size || dpb_.MaxNumPics() != sps->max_dpb_size ||
      profile_ != new_profile || bit_depth_ != new_bit_depth ||
      chroma_sampling_ != new_chroma_sampling) {
    CHECK(Flush()) << "Failed to flush the decoder.";

    LOG(INFO) << "Codec profile: " << GetProfileName(new_profile)
              << ", level(x30): " << sps->profile_tier_level.general_level_idc
              << ", DPB size: " << sps->max_dpb_size
              << ", Picture size: " << new_pic_size.ToString()
              << ", bit_depth: " << base::strict_cast<int>(new_bit_depth)
              << ", chroma_sampling_format: "
              << VideoChromaSamplingToString(new_chroma_sampling);
    profile_ = new_profile;
    bit_depth_ = new_bit_depth;
    pic_size_ = new_pic_size;
    chroma_sampling_ = new_chroma_sampling;
    dpb_.SetMaxNumPics(sps->max_dpb_size);
    if (need_new_buffers) {
      *need_new_buffers = true;
    }
  }

  return true;
}

bool H265Decoder::PreprocessCurrentSlice() {
  const H265SliceHeader* slice_hdr = curr_slice_hdr_.get();
  CHECK(slice_hdr);

  if (slice_hdr->first_slice_segment_in_pic_flag) {
    // New picture, so first finish the previous one before processing it.
    FinishPrevFrameIfPresent();
    CHECK(!curr_pic_);
  }

  return true;
}

bool H265Decoder::ProcessCurrentSlice() {
  CHECK(curr_pic_);

  const H265SliceHeader* slice_hdr = curr_slice_hdr_.get();
  CHECK(slice_hdr);

  const H265SPS* sps = parser_->GetSPS(curr_sps_id_);
  CHECK(sps);

  const H265PPS* pps = parser_->GetPPS(curr_pps_id_);
  CHECK(pps);

  // Adds a start code prefix, a unique sequence of 3 bytes equal to 0x000001
  // embedded in the byte stream as a prefix to each NAL unit. All hardwares
  // supported in ChromeOS require the start code prefix.
  std::vector<uint8_t> slice_data = {0x00, 0x00, 0x01};

  slice_data.insert(
      slice_data.end(), curr_slice_hdr_->nalu_data.get(),
      (curr_slice_hdr_->nalu_data + curr_slice_hdr_->nalu_size).get());

  scoped_refptr<MmappedBuffer> OUTPUT_buffer = OUTPUT_queue_->GetBuffer(0);
  OUTPUT_buffer->mmapped_planes()[0].CopyInSlice(
      &slice_data[0], slice_data.size(),
      curr_slice_hdr_->first_slice_segment_in_pic_flag);
  OUTPUT_buffer->set_frame_number(global_pic_count_);

  return true;
}

void H265Decoder::CalcPicOutputFlags(const H265SliceHeader* slice_hdr) {
  if (slice_hdr->irap_pic) {
    // 8.1.3
    curr_pic_->no_rasl_output_flag_ =
        (curr_nalu_->nal_unit_type >= H265NALU::BLA_W_LP &&
         curr_nalu_->nal_unit_type <= H265NALU::IDR_N_LP) ||
        curr_pic_->first_picture_;
  } else {
    curr_pic_->no_rasl_output_flag_ = false;
  }

  // C.5.2.2
  if (slice_hdr->irap_pic && curr_pic_->no_rasl_output_flag_ &&
      !curr_pic_->first_picture_) {
    curr_pic_->no_output_of_prior_pics_flag_ =
        (slice_hdr->nal_unit_type == H265NALU::CRA_NUT) ||
        slice_hdr->no_output_of_prior_pics_flag;
  } else {
    curr_pic_->no_output_of_prior_pics_flag_ = false;
  }

  if ((slice_hdr->nal_unit_type == H265NALU::RASL_N ||
       slice_hdr->nal_unit_type == H265NALU::RASL_R) &&
      curr_pic_->no_rasl_output_flag_) {
    curr_pic_->pic_output_flag_ = false;
  } else {
    curr_pic_->pic_output_flag_ = slice_hdr->pic_output_flag;
  }
}

void H265Decoder::CalcPictureOrderCount(const H265PPS* pps,
                                        const H265SliceHeader* slice_hdr) {
  // 8.3.1 Decoding process for picture order count.
  curr_pic_->valid_for_prev_tid0_pic_ =
      !slice_hdr->temporal_id &&
      (slice_hdr->nal_unit_type < H265NALU::RADL_N ||
       slice_hdr->nal_unit_type > H265NALU::RSV_VCL_N14);
  curr_pic_->slice_pic_order_cnt_lsb_ = slice_hdr->slice_pic_order_cnt_lsb;

  // Calculate POC for current picture.
  if ((!slice_hdr->irap_pic || !curr_pic_->no_rasl_output_flag_) &&
      prev_tid0_pic_) {
    const int prev_pic_order_cnt_lsb = prev_tid0_pic_->slice_pic_order_cnt_lsb_;
    const int prev_pic_order_cnt_msb = prev_tid0_pic_->pic_order_cnt_msb_;
    if ((slice_hdr->slice_pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
        ((prev_pic_order_cnt_lsb - slice_hdr->slice_pic_order_cnt_lsb) >=
         (max_pic_order_cnt_lsb_ / 2))) {
      curr_pic_->pic_order_cnt_msb_ =
          prev_pic_order_cnt_msb + max_pic_order_cnt_lsb_;
    } else if ((slice_hdr->slice_pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
               ((slice_hdr->slice_pic_order_cnt_lsb - prev_pic_order_cnt_lsb) >
                (max_pic_order_cnt_lsb_ / 2))) {
      curr_pic_->pic_order_cnt_msb_ =
          prev_pic_order_cnt_msb - max_pic_order_cnt_lsb_;
    } else {
      curr_pic_->pic_order_cnt_msb_ = prev_pic_order_cnt_msb;
    }
  } else {
    curr_pic_->pic_order_cnt_msb_ = 0;
  }
  curr_pic_->pic_order_cnt_val_ =
      curr_pic_->pic_order_cnt_msb_ + slice_hdr->slice_pic_order_cnt_lsb;
}

bool H265Decoder::CalcRefPicPocs(const H265SPS* sps,
                                 const H265PPS* pps,
                                 const H265SliceHeader* slice_hdr) {
  if (slice_hdr->nal_unit_type == H265NALU::IDR_W_RADL ||
      slice_hdr->nal_unit_type == H265NALU::IDR_N_LP) {
    num_poc_st_curr_before_ = num_poc_st_curr_after_ = num_poc_st_foll_ =
        num_poc_lt_curr_ = num_poc_lt_foll_ = 0;
    return true;
  }

  // 8.3.2 - NOTE 2
  const H265StRefPicSet& curr_st_ref_pic_set = slice_hdr->GetStRefPicSet(sps);

  // Equation 8-5.
  int i, j, k;
  for (i = 0, j = 0, k = 0; i < curr_st_ref_pic_set.num_negative_pics; ++i) {
    base::CheckedNumeric<int> poc = curr_pic_->pic_order_cnt_val_;
    poc += curr_st_ref_pic_set.delta_poc_s0[i];
    if (!poc.IsValid()) {
      LOG(ERROR) << "Invalid POC";
      return false;
    }
    if (curr_st_ref_pic_set.used_by_curr_pic_s0[i]) {
      poc_st_curr_before_[j++] = poc.ValueOrDefault(0);
    } else {
      poc_st_foll_[k++] = poc.ValueOrDefault(0);
    }
  }
  num_poc_st_curr_before_ = j;
  for (i = 0, j = 0; i < curr_st_ref_pic_set.num_positive_pics; ++i) {
    base::CheckedNumeric<int> poc = curr_pic_->pic_order_cnt_val_;
    poc += curr_st_ref_pic_set.delta_poc_s1[i];
    if (!poc.IsValid()) {
      LOG(ERROR) << "Invalid POC";
      return false;
    }
    if (curr_st_ref_pic_set.used_by_curr_pic_s1[i]) {
      poc_st_curr_after_[j++] = poc.ValueOrDefault(0);
    } else {
      poc_st_foll_[k++] = poc.ValueOrDefault(0);
    }
  }
  num_poc_st_curr_after_ = j;
  num_poc_st_foll_ = k;
  for (i = 0, j = 0, k = 0;
       i < slice_hdr->num_long_term_sps + slice_hdr->num_long_term_pics; ++i) {
    base::CheckedNumeric<int> poc_lt = slice_hdr->poc_lsb_lt[i];
    if (slice_hdr->delta_poc_msb_present_flag[i]) {
      poc_lt += curr_pic_->pic_order_cnt_val_;
      base::CheckedNumeric<int> poc_delta =
          slice_hdr->delta_poc_msb_cycle_lt[i];
      poc_delta *= max_pic_order_cnt_lsb_;
      if (!poc_delta.IsValid()) {
        LOG(ERROR) << "Invalid POC";
        return false;
      }
      poc_lt -= poc_delta.ValueOrDefault(0);
      poc_lt -= curr_pic_->pic_order_cnt_val_ & (max_pic_order_cnt_lsb_ - 1);
    }
    if (!poc_lt.IsValid()) {
      LOG(ERROR) << "Invalid POC";
      return false;
    }
    if (slice_hdr->used_by_curr_pic_lt[i]) {
      poc_lt_curr_[j] = poc_lt.ValueOrDefault(0);
      curr_delta_poc_msb_present_flag_[j++] =
          slice_hdr->delta_poc_msb_present_flag[i];
    } else {
      poc_lt_foll_[k] = poc_lt.ValueOrDefault(0);
      foll_delta_poc_msb_present_flag_[k++] =
          slice_hdr->delta_poc_msb_present_flag[i];
    }
  }
  num_poc_lt_curr_ = j;
  num_poc_lt_foll_ = k;

  // Check conformance for |num_pic_total_curr|.
  if (slice_hdr->nal_unit_type == H265NALU::CRA_NUT ||
      (slice_hdr->nal_unit_type >= H265NALU::BLA_W_LP &&
       slice_hdr->nal_unit_type <= H265NALU::BLA_N_LP)) {
    if (slice_hdr->num_pic_total_curr) {
      LOG(ERROR) << "Invalid value for num_pic_total_curr";
      return false;
    }
  } else if ((slice_hdr->IsBSlice() || slice_hdr->IsPSlice()) &&
             !slice_hdr->num_pic_total_curr) {
    LOG(ERROR) << "Invalid value for num_pic_total_curr";
    return false;
  }

  return true;
}

bool H265Decoder::BuildRefPicLists(const H265SPS* sps,
                                   const H265PPS* pps,
                                   const H265SliceHeader* slice_hdr) {
  ref_pic_set_lt_curr_.clear();
  ref_pic_set_lt_curr_.resize(kMaxDpbSize);
  ref_pic_set_st_curr_after_.clear();
  ref_pic_set_st_curr_after_.resize(kMaxDpbSize);
  ref_pic_set_st_curr_before_.clear();
  ref_pic_set_st_curr_before_.resize(kMaxDpbSize);
  scoped_refptr<H265Picture> ref_pic_set_lt_foll[kMaxDpbSize];
  scoped_refptr<H265Picture> ref_pic_set_st_foll[kMaxDpbSize];

  // Mark everything in the DPB as unused for reference now. When we determine
  // the pics in the ref list, then we will mark them appropriately.
  dpb_.MarkAllUnusedForReference();

  // Equation 8-6.
  // We may be missing reference pictures, if so then we just don't specify
  // them and let the accelerator deal with the missing reference pictures
  // which is covered in the spec.
  int total_ref_pics = 0;
  for (int i = 0; i < num_poc_lt_curr_; ++i) {
    if (!curr_delta_poc_msb_present_flag_[i]) {
      ref_pic_set_lt_curr_[i] = dpb_.GetPicByPocMaskedAndMark(
          poc_lt_curr_[i], sps->max_pic_order_cnt_lsb - 1,
          H265Picture::kLongTermCurr);
    } else {
      ref_pic_set_lt_curr_[i] =
          dpb_.GetPicByPocAndMark(poc_lt_curr_[i], H265Picture::kLongTermCurr);
    }

    if (ref_pic_set_lt_curr_[i]) {
      total_ref_pics++;
    }
  }
  for (int i = 0; i < num_poc_lt_foll_; ++i) {
    if (!foll_delta_poc_msb_present_flag_[i]) {
      ref_pic_set_lt_foll[i] = dpb_.GetPicByPocMaskedAndMark(
          poc_lt_foll_[i], sps->max_pic_order_cnt_lsb - 1,
          H265Picture::kLongTermFoll);
    } else {
      ref_pic_set_lt_foll[i] =
          dpb_.GetPicByPocAndMark(poc_lt_foll_[i], H265Picture::kLongTermFoll);
    }

    if (ref_pic_set_lt_foll[i]) {
      total_ref_pics++;
    }
  }

  // Equation 8-7.
  for (int i = 0; i < num_poc_st_curr_before_; ++i) {
    ref_pic_set_st_curr_before_[i] = dpb_.GetPicByPocAndMark(
        poc_st_curr_before_[i], H265Picture::kShortTermCurrBefore);

    if (ref_pic_set_st_curr_before_[i]) {
      total_ref_pics++;
    }
  }
  for (int i = 0; i < num_poc_st_curr_after_; ++i) {
    ref_pic_set_st_curr_after_[i] = dpb_.GetPicByPocAndMark(
        poc_st_curr_after_[i], H265Picture::kShortTermCurrAfter);
    if (ref_pic_set_st_curr_after_[i]) {
      total_ref_pics++;
    }
  }
  for (int i = 0; i < num_poc_st_foll_; ++i) {
    ref_pic_set_st_foll[i] =
        dpb_.GetPicByPocAndMark(poc_st_foll_[i], H265Picture::kShortTermFoll);
    if (ref_pic_set_st_foll[i]) {
      total_ref_pics++;
    }
  }

  // Verify that the total number of reference pictures in the DPB matches the
  // total count of reference pics. This ensures that a picture is not in more
  // than one list, per the spec.
  if (dpb_.GetReferencePicCount() != total_ref_pics) {
    LOG(ERROR) << "Conformance problem, reference pic is in more than one list";
    return false;
  }

  ref_pic_list_.clear();
  dpb_.AppendReferencePics(&ref_pic_list_);
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  // 8.3.3 Generation of unavailable reference pictures is something we do not
  // need to handle here. It's handled by the accelerator itself when we do not
  // specify a reference picture that it needs.

  if (slice_hdr->IsPSlice() || slice_hdr->IsBSlice()) {
    // 8.3.4 Decoding process for reference picture lists construction
    int num_rps_curr_temp_list0 =
        std::max(slice_hdr->num_ref_idx_l0_active_minus1 + 1,
                 slice_hdr->num_pic_total_curr);
    scoped_refptr<H265Picture> ref_pic_list_temp0[kMaxDpbSize];

    // Equation 8-8.
    int r_idx = 0;
    while (r_idx < num_rps_curr_temp_list0) {
      for (int i = 0;
           i < num_poc_st_curr_before_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_st_curr_before_[i];
      }
      for (int i = 0;
           i < num_poc_st_curr_after_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_st_curr_after_[i];
      }
      for (int i = 0; i < num_poc_lt_curr_ && r_idx < num_rps_curr_temp_list0;
           ++i, ++r_idx) {
        ref_pic_list_temp0[r_idx] = ref_pic_set_lt_curr_[i];
      }
    }

    // Equation 8-9.
    for (r_idx = 0; r_idx <= slice_hdr->num_ref_idx_l0_active_minus1; ++r_idx) {
      ref_pic_list0_.push_back(
          slice_hdr->ref_pic_lists_modification
                  .ref_pic_list_modification_flag_l0
              ? ref_pic_list_temp0[slice_hdr->ref_pic_lists_modification
                                       .list_entry_l0[r_idx]]
              : ref_pic_list_temp0[r_idx]);
    }

    if (slice_hdr->IsBSlice()) {
      int num_rps_curr_temp_list1 =
          std::max(slice_hdr->num_ref_idx_l1_active_minus1 + 1,
                   slice_hdr->num_pic_total_curr);
      scoped_refptr<H265Picture> ref_pic_list_temp1[kMaxDpbSize];

      // Equation 8-10.
      r_idx = 0;
      while (r_idx < num_rps_curr_temp_list1) {
        for (int i = 0;
             i < num_poc_st_curr_after_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_st_curr_after_[i];
        }
        for (int i = 0;
             i < num_poc_st_curr_before_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_st_curr_before_[i];
        }
        for (int i = 0; i < num_poc_lt_curr_ && r_idx < num_rps_curr_temp_list1;
             ++i, r_idx++) {
          ref_pic_list_temp1[r_idx] = ref_pic_set_lt_curr_[i];
        }
      }

      // Equation 8-11.
      for (r_idx = 0; r_idx <= slice_hdr->num_ref_idx_l1_active_minus1;
           ++r_idx) {
        ref_pic_list1_.push_back(
            slice_hdr->ref_pic_lists_modification
                    .ref_pic_list_modification_flag_l1
                ? ref_pic_list_temp1[slice_hdr->ref_pic_lists_modification
                                         .list_entry_l1[r_idx]]
                : ref_pic_list_temp1[r_idx]);
      }
    }
  }

  return true;
}

bool H265Decoder::OutputPic(scoped_refptr<H265Picture> pic) {
  CHECK(!pic->outputted_);
  pic->outputted_ = true;
  VLOGF(4) << "Posting output task for POC: " << pic->pic_order_cnt_val_;

  frames_ready_to_be_outputted_.push(std::move(pic));

  return true;
}

bool H265Decoder::PerformDpbOperations(const H265SPS* sps) {
  // C.5.2.2
  if (curr_pic_->irap_pic_ && curr_pic_->no_rasl_output_flag_ &&
      !curr_pic_->first_picture_) {
    if (!curr_pic_->no_output_of_prior_pics_flag_) {
      OutputAllRemainingPics();
    }
    dpb_.Clear();
  } else {
    int num_to_output;
    do {
      dpb_.DeleteUnused();
      // Get all pictures that haven't been outputted yet.
      H265Picture::Vector not_outputted;
      dpb_.AppendPendingOutputPics(&not_outputted);
      // Sort in output order.
      std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

      // Calculate how many pictures we need to output.
      num_to_output = 0;
      int highest_tid = sps->sps_max_sub_layers_minus1;
      num_to_output = std::max(num_to_output,
                               static_cast<int>(not_outputted.size()) -
                                   sps->sps_max_num_reorder_pics[highest_tid]);
      num_to_output =
          std::max(num_to_output,
                   static_cast<int>(dpb_.Size()) -
                       sps->sps_max_dec_pic_buffering_minus1[highest_tid]);

      num_to_output =
          std::min(num_to_output, static_cast<int>(not_outputted.size()));
      if (!num_to_output && dpb_.IsFull()) {
        // This is wrong, we should try to output pictures until we can clear
        // one from the DPB. This is better than failing, but we then may end up
        // with something out of order.
        LOG(ERROR) << "Forcibly outputting pictures to make room in DPB.";
        for (const auto& pic : not_outputted) {
          num_to_output++;
          if (pic->reference_type_ == H265Picture::kUnused) {
            break;
          }
        }
      }

      not_outputted.resize(num_to_output);
      for (auto& pic : not_outputted) {
        OutputPic(pic);
      }

      dpb_.DeleteUnused();
    } while (dpb_.IsFull() && num_to_output);
  }

  if (dpb_.IsFull()) {
    LOG(ERROR) << "Could not free up space in DPB for current picture";
    return false;
  }

  // Put the current pic in the DPB.
  curr_pic_->reference_type_ = H265Picture::kShortTermFoll;
  dpb_.StorePicture(curr_pic_);
  return true;
}

bool H265Decoder::StartNewFrame(const H265SliceHeader* slice_hdr) {
  CHECK(curr_pic_.get());
  DCHECK(slice_hdr);

  curr_pps_id_ = slice_hdr->slice_pic_parameter_set_id;
  const H265PPS* pps = parser_->GetPPS(curr_pps_id_);
  DCHECK(pps);

  curr_sps_id_ = pps->pps_seq_parameter_set_id;
  const H265SPS* sps = parser_->GetSPS(curr_sps_id_);
  DCHECK(sps);

  // If this is from a retry for submitting frame meta data,
  // we should not redo all of these calculations.
  if (!curr_pic_->processed_) {
    // Copy slice/pps variables we need to the picture.
    curr_pic_->nal_unit_type_ = curr_nalu_->nal_unit_type;
    curr_pic_->irap_pic_ = slice_hdr->irap_pic;

    curr_pic_->ref_ts_nsec_ = global_pic_count_;

    // TODO(b/261127809): Set the color space for the picture.

    CalcPicOutputFlags(slice_hdr);
    CalcPictureOrderCount(pps, slice_hdr);
    {
      const bool success = CalcRefPicPocs(sps, pps, slice_hdr);
      CHECK(success) << "CalcRefPicPocs function failed.";
    }
    {
      const bool success = BuildRefPicLists(sps, pps, slice_hdr);
      CHECK(success) << "BuildRefPicLists function failed.";
    }
    {
      const bool success = PerformDpbOperations(sps);
      CHECK(success) << "PerformDpbOperations function failed.";
    }
    curr_pic_->processed_ = true;
  }

  struct v4l2_ctrl_hevc_sps v4l2_sps = SetupSPSCtrl(sps);
  struct v4l2_ctrl_hevc_pps v4l2_pps = SetupPPSCtrl(pps);
  struct v4l2_ctrl_hevc_scaling_matrix v4l2_matrix =
      SetupScalingMatrix(sps, pps);
  struct v4l2_ctrl_hevc_decode_params v4l2_decode_params = SetupDecodeParams(
      slice_hdr, ref_pic_list_, ref_pic_set_lt_curr_,
      ref_pic_set_st_curr_after_, ref_pic_set_st_curr_before_, curr_pic_);

  struct v4l2_ext_control ctrls[] = {
      {.id = V4L2_CID_STATELESS_HEVC_SPS,
       .size = sizeof(v4l2_sps),
       .ptr = &v4l2_sps},
      {.id = V4L2_CID_STATELESS_HEVC_PPS,
       .size = sizeof(v4l2_pps),
       .ptr = &v4l2_pps},
      {.id = V4L2_CID_STATELESS_HEVC_SCALING_MATRIX,
       .size = sizeof(v4l2_matrix),
       .ptr = &v4l2_matrix},
      {.id = V4L2_CID_STATELESS_HEVC_DECODE_PARAMS,
       .size = sizeof(v4l2_decode_params),
       .ptr = &v4l2_decode_params}};

  struct v4l2_ext_controls ext_ctrls = {
      .count = (sizeof(ctrls) / sizeof(ctrls[0])), .controls = ctrls};

  v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls, is_OUTPUT_queue_new_);

  return true;
}

std::set<uint32_t> H265Decoder::GetReusableReferenceSlots(
    const MmappedBuffer& buffer,
    const std::set<uint32_t>& queued_buffer_ids) {
  std::set<uint32_t> reusable_buffer_slots = {};
  const std::set<uint32_t> buffer_ids_in_use = dpb_.GetBufferIdsInUse();

  for (uint32_t i = 0; i < CAPTURE_queue_->num_buffers(); i++) {
    // Checks that buffer ID is not currently queued in the CAPTURE queue
    // and that it is not the same buffer ID previously written to.
    const bool is_element_in_that_list = (queued_buffer_ids.count(i) != 0);
    if (is_element_in_that_list) {
      continue;
    }

    const bool is_buffer_previously_written_to = (i == buffer.buffer_id());
    if (is_buffer_previously_written_to) {
      continue;
    }

    const bool is_buffer_in_use = base::Contains(buffer_ids_in_use, i);
    if (is_buffer_in_use) {
      continue;
    }

    reusable_buffer_slots.insert(i);
  }

  return reusable_buffer_slots;
}

bool H265Decoder::DecodePicture() {
  CHECK(curr_pic_.get());

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0)) {
    VLOG(4) << "VIDIOC_QBUF failed for OUTPUT queue.";
    return VideoDecoder::kError;
  }

  v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_);

  if (!CAPTURE_queue_) {
    CreateCAPTUREQueue(kNumberOfBuffersInCaptureQueue);
  }

  v4l2_ioctl_->WaitForRequestCompletion(OUTPUT_queue_);

  uint32_t CAPTURE_id;
  v4l2_ioctl_->DQBuf(CAPTURE_queue_, &CAPTURE_id);

  CAPTURE_queue_->DequeueBufferId(CAPTURE_id);
  curr_pic_->capture_queue_buffer_id_ = CAPTURE_id;

  const std::set<uint32_t> reusable_buffer_slots =
      GetReusableReferenceSlots(*CAPTURE_queue_->GetBuffer(CAPTURE_id).get(),
                                CAPTURE_queue_->queued_buffer_ids());

  for (const auto reusable_buffer_slot : reusable_buffer_slots) {
    if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, reusable_buffer_slot)) {
      VLOGF(4) << "VIDIOC_QBUF failed for CAPTURE queue.";
      continue;
    }
    // Keeps track of which indices are currently queued in the
    // CAPTURE queue. This will be used to determine which indices
    // can/cannot be refreshed.
    CAPTURE_queue_->QueueBufferId(reusable_buffer_slot);
  }

  uint32_t OUTPUT_queue_buffer_id;
  v4l2_ioctl_->DQBuf(OUTPUT_queue_, &OUTPUT_queue_buffer_id);
  v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_);

  global_pic_count_++;

  return true;
}

void H265Decoder::FinishPicture(scoped_refptr<H265Picture> pic) {
  // 8.3.1
  if (pic->valid_for_prev_tid0_pic_) {
    prev_tid0_pic_ = pic;
  }

  ref_pic_list_.clear();
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();
  ref_pic_set_lt_curr_.clear();
  ref_pic_set_st_curr_after_.clear();
  ref_pic_set_st_curr_before_.clear();

  last_slice_hdr_.reset();
}

void H265Decoder::FinishPrevFrameIfPresent() {
  // If we already have a frame waiting to be decoded, decode it and finish.
  if (curr_pic_) {
    const bool success = DecodePicture();
    CHECK(success) << "Failed to decode the current picture.";

    FinishPicture(std::move(curr_pic_));
  }
}

H265Decoder::DecodeResult H265Decoder::Decode() {
  DCHECK(state_ != kError) << "Decoder in error state";

  while (frames_ready_to_be_outputted_.empty()) {
    if (!curr_nalu_) {
      curr_nalu_ = std::make_unique<H265NALU>();

      const H265Parser::Result parse_result =
          parser_->AdvanceToNextNALU(curr_nalu_.get());
      if (parse_result == H265Parser::kEOStream) {
        curr_nalu_.reset();

        FinishPrevFrameIfPresent();

        is_stream_over_ = true;
        return kRanOutOfStreamData;
      }

      CHECK_EQ(parse_result, H265Parser::kOk);
      VLOGF(4) << "New NALU: " << static_cast<int>(curr_nalu_->nal_unit_type);
    }

    // 8.1.2 We only want nuh_layer_id of zero.
    if (curr_nalu_->nuh_layer_id) {
      VLOGF(4) << "Skipping NALU with nuh_layer_id="
               << curr_nalu_->nuh_layer_id;
      curr_nalu_.reset();
      continue;
    }

    switch (curr_nalu_->nal_unit_type) {
      case H265NALU::BLA_W_LP:  // fallthrough
      case H265NALU::BLA_W_RADL:
      case H265NALU::BLA_N_LP:
      case H265NALU::IDR_W_RADL:
      case H265NALU::IDR_N_LP:
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
      case H265NALU::CRA_NUT: {
        if (!curr_slice_hdr_) {
          curr_slice_hdr_ = std::make_unique<H265SliceHeader>();

          const H265Parser::Result parse_result = parser_->ParseSliceHeader(
              *curr_nalu_, curr_slice_hdr_.get(), last_slice_hdr_.get());
          if (parse_result == H265Parser::kMissingParameterSet) {
            // We may still be able to recover if we skip until we find the
            // SPS/PPS.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }

          CHECK_EQ(parse_result, H265Parser::kOk);
          if (!curr_slice_hdr_->irap_pic && state_ == kAfterReset) {
            // We can't resume from a non-IRAP picture.
            curr_slice_hdr_.reset();
            last_slice_hdr_.reset();
            break;
          }

          state_ = kTryPreprocessCurrentSlice;
          if (curr_slice_hdr_->irap_pic) {
            bool need_new_buffers = false;

            const bool success = ProcessPPS(
                curr_slice_hdr_->slice_pic_parameter_set_id, &need_new_buffers);
            CHECK(success) << "Failed to process PPS.";

            if (need_new_buffers) {
              curr_pic_ = nullptr;
              return kConfigChange;
            }
          }
        }

        if (state_ == kTryPreprocessCurrentSlice) {
          const bool success = PreprocessCurrentSlice();
          CHECK(success) << "Failed to pre-process current slice.";

          state_ = kEnsurePicture;
        }

        if (state_ == kEnsurePicture) {
          if (curr_pic_) {
            // |curr_pic_| already exists, so skip to ProcessCurrentSlice().
            state_ = kTryCurrentSlice;
          } else {
            curr_pic_ = new H265Picture();
            CHECK(curr_pic_) << "Ran out of surfaces.";

            curr_pic_->first_picture_ = first_picture_;
            first_picture_ = false;
            state_ = kTryNewFrame;
          }
        }

        if (state_ == kTryNewFrame) {
          const bool success = StartNewFrame(curr_slice_hdr_.get());
          CHECK(success) << "Failed to start processing a new frame.";

          state_ = kTryCurrentSlice;
        }

        DCHECK_EQ(state_, kTryCurrentSlice);
        const bool success = ProcessCurrentSlice();
        CHECK(success) << "Failed to process current slice.";

        state_ = kDecoding;
        last_slice_hdr_ = std::move(curr_slice_hdr_);
        curr_slice_hdr_.reset();
        break;
      }
      case H265NALU::SPS_NUT: {
        FinishPrevFrameIfPresent();

        int sps_id;

        const H265Parser::Result parse_result = parser_->ParseSPS(&sps_id);
        CHECK_EQ(parse_result, H265Parser::kOk)
            << "Parser Failed to parse SPS.";

        break;
      }
      case H265NALU::PPS_NUT: {
        FinishPrevFrameIfPresent();

        int pps_id;

        const H265Parser::Result parse_result =
            parser_->ParsePPS(*curr_nalu_, &pps_id);
        CHECK_EQ(parse_result, H265Parser::kOk)
            << "Parser Failed to parse PPS.";

        if (curr_pps_id_ == -1) {
          bool need_new_buffers = false;

          const bool success_process_pps =
              ProcessPPS(pps_id, &need_new_buffers);
          CHECK(success_process_pps) << "Failed to process PPS.";

          if (need_new_buffers) {
            curr_nalu_.reset();
            return kConfigChange;
          }
        }

        break;
      }
      case H265NALU::EOS_NUT:
        first_picture_ = true;
        [[fallthrough]];
      case H265NALU::EOB_NUT:  // fallthrough
      case H265NALU::AUD_NUT:
      case H265NALU::RSV_NVCL41:
      case H265NALU::RSV_NVCL42:
      case H265NALU::RSV_NVCL43:
      case H265NALU::RSV_NVCL44:
      case H265NALU::UNSPEC48:
      case H265NALU::UNSPEC49:
      case H265NALU::UNSPEC50:
      case H265NALU::UNSPEC51:
      case H265NALU::UNSPEC52:
      case H265NALU::UNSPEC53:
      case H265NALU::UNSPEC54:
      case H265NALU::UNSPEC55: {
        FinishPrevFrameIfPresent();
        break;
      }
      default:
        VLOGF(4) << "Skipping NALU type: " << curr_nalu_->nal_unit_type;
        break;
    }

    VLOGF(4) << "Finished with current NALU type: "
             << static_cast<int>(curr_nalu_->nal_unit_type);
    curr_nalu_.reset();
  }

  return kOk;
}

VideoDecoder::Result H265Decoder::DecodeNextFrame(const int frame_number,
                                                  std::vector<uint8_t>& y_plane,
                                                  std::vector<uint8_t>& u_plane,
                                                  std::vector<uint8_t>& v_plane,
                                                  gfx::Size& size,
                                                  BitDepth& bit_depth) {
  if (!parser_) {
    parser_ = std::make_unique<H265Parser>();
    parser_->SetStream(data_stream_->data(), data_stream_->length());
  }

  is_OUTPUT_queue_new_ = !OUTPUT_queue_;
  if (!OUTPUT_queue_) {
    CreateOUTPUTQueue(kDriverCodecFourcc);
  }

  while (!is_stream_over_ && frames_ready_to_be_outputted_.empty()) {
    Decode();
  }

  if (is_stream_over_) {
    OutputAllRemainingPics();
  }

  if (is_stream_over_ && frames_ready_to_be_outputted_.empty()) {
    return VideoDecoder::kEOStream;
  }

  if (frames_ready_to_be_outputted_.empty()) {
    NOTREACHED_IN_MIGRATION()
        << "Stream ended with |frames_ready_to_be_outputted_| empty";
  }

  scoped_refptr<H265Picture> picture = frames_ready_to_be_outputted_.front();
  last_decoded_frame_visible_ = picture->outputted_;
  scoped_refptr<MmappedBuffer> buffer =
      CAPTURE_queue_->GetBuffer(picture->capture_queue_buffer_id_);

  bit_depth =
      ConvertToYUV(y_plane, u_plane, v_plane, OUTPUT_queue_->resolution(),
                   buffer->mmapped_planes(), CAPTURE_queue_->resolution(),
                   CAPTURE_queue_->fourcc());

  frames_ready_to_be_outputted_.pop();

  return VideoDecoder::kOk;
}
}  // namespace v4l2_test
}  // namespace media
