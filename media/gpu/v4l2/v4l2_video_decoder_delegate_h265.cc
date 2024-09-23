// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_delegate_h265.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include <algorithm>
#include <type_traits>

#include "base/logging.h"
#include "build/build_config.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

class V4L2H265Picture : public H265Picture {
 public:
  explicit V4L2H265Picture(scoped_refptr<V4L2DecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  V4L2H265Picture(const V4L2H265Picture&) = delete;
  V4L2H265Picture& operator=(const V4L2H265Picture&) = delete;

  V4L2H265Picture* AsV4L2H265Picture() override { return this; }
  scoped_refptr<V4L2DecodeSurface> dec_surface() { return dec_surface_; }

 private:
  ~V4L2H265Picture() override {}

  scoped_refptr<V4L2DecodeSurface> dec_surface_;
};

V4L2VideoDecoderDelegateH265::V4L2VideoDecoderDelegateH265(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : surface_handler_(surface_handler), device_(device) {
  DCHECK(surface_handler_);
}

V4L2VideoDecoderDelegateH265::~V4L2VideoDecoderDelegateH265() {}

scoped_refptr<H265Picture> V4L2VideoDecoderDelegateH265::CreateH265Picture() {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface) {
    return nullptr;
  }

  return new V4L2H265Picture(dec_surface);
}

scoped_refptr<H265Picture>
V4L2VideoDecoderDelegateH265::CreateH265PictureSecure(uint64_t secure_handle) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSecureSurface(secure_handle);
  if (!dec_surface) {
    return nullptr;
  }

  return new V4L2H265Picture(dec_surface);
}

std::vector<scoped_refptr<V4L2DecodeSurface>>
V4L2VideoDecoderDelegateH265::FillInV4L2DPB(
    struct v4l2_ctrl_hevc_decode_params* v4l2_decode_param,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before) {
  std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces;

  memset(v4l2_decode_param->dpb, 0, sizeof(v4l2_decode_param->dpb));
  unsigned int i = 0;
  for (const auto& pic : ref_pic_list) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    unsigned int index = 0xff;
    if (!pic->IsUnused()) {
      scoped_refptr<V4L2DecodeSurface> dec_surface =
          H265PictureToV4L2DecodeSurface(pic.get());
      index = dec_surface->GetReferenceID();
      ref_surfaces.push_back(dec_surface);
    }

    v4l2_decode_param->dpb[i++] = {
        .timestamp = index,
        .flags = static_cast<__u8>(
            pic->IsLongTermRef() ? V4L2_HEVC_DPB_ENTRY_LONG_TERM_REFERENCE : 0),
        .field_pic = V4L2_HEVC_SEI_PIC_STRUCT_FRAME,  // No interlaced support
        .pic_order_cnt_val = pic->pic_order_cnt_val_,
    };
  }
  v4l2_decode_param->num_active_dpb_entries = i;

  // Set defaults
  std::fill_n(v4l2_decode_param->poc_st_curr_before,
              std::size(v4l2_decode_param->poc_st_curr_before), 0xff);
  std::fill_n(v4l2_decode_param->poc_st_curr_after,
              std::size(v4l2_decode_param->poc_st_curr_after), 0xff);
  std::fill_n(v4l2_decode_param->poc_lt_curr,
              std::size(v4l2_decode_param->poc_lt_curr), 0xff);

  i = 0;
  for (const auto& pic : ref_pic_set_st_curr_before) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < v4l2_decode_param->num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          v4l2_decode_param->dpb[j].pic_order_cnt_val) {
        v4l2_decode_param->poc_st_curr_before[i++] = j;
        break;
      }
    }
  }
  v4l2_decode_param->num_poc_st_curr_before = i;

  i = 0;
  for (const auto& pic : ref_pic_set_st_curr_after) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < v4l2_decode_param->num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          v4l2_decode_param->dpb[j].pic_order_cnt_val) {
        v4l2_decode_param->poc_st_curr_after[i++] = j;
        break;
      }
    }
  }
  v4l2_decode_param->num_poc_st_curr_after = i;

  i = 0;
  for (const auto& pic : ref_pic_set_lt_curr) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < v4l2_decode_param->num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          v4l2_decode_param->dpb[j].pic_order_cnt_val) {
        v4l2_decode_param->poc_lt_curr[i++] = j;
        break;
      }
    }
  }
  v4l2_decode_param->num_poc_lt_curr = i;

  return ref_surfaces;
}

H265Decoder::H265Accelerator::Status
V4L2VideoDecoderDelegateH265::SubmitFrameMetadata(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic) {
  struct v4l2_ext_control ctrl;
  std::vector<struct v4l2_ext_control> ctrls;

  struct v4l2_ctrl_hevc_sps v4l2_sps;
  memset(&v4l2_sps, 0, sizeof(v4l2_sps));

  int highest_tid = sps->sps_max_sub_layers_minus1;

  // In the order of |struct v4l2_ctrl_hevc_sps|
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
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_HEVC_SPS;
  ctrl.size = sizeof(v4l2_sps);
  ctrl.ptr = &v4l2_sps;
  ctrls.push_back(ctrl);

  struct v4l2_ctrl_hevc_pps v4l2_pps;
  memset(&v4l2_pps, 0, sizeof(v4l2_pps));
  // In the order of |struct v4l2_ctrl_hevc_pps|
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

    // Cedrus currently requires userspace to provide the values regardless of
    // uniform spacing. But since we don't support Cedrus's slice-based
    // decoding, lets follow the spec for now.
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

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_HEVC_PPS;
  ctrl.size = sizeof(v4l2_pps);
  ctrl.ptr = &v4l2_pps;
  ctrls.push_back(ctrl);

  struct v4l2_ctrl_hevc_scaling_matrix v4l2_scaling_matrix;
  memset(&v4l2_scaling_matrix, 0, sizeof(v4l2_scaling_matrix));
  struct H265ScalingListData checker;

  // TODO(jkardatzke): Optimize storage of the 32x32 since only indices 0 and 3
  // are actually used. See ../../video/h265_parser.h
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
    // Copied from H265VaapiVideoDecoderDelegate:
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

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_HEVC_SCALING_MATRIX;
  ctrl.size = sizeof(v4l2_scaling_matrix);
  ctrl.ptr = &v4l2_scaling_matrix;
  ctrls.push_back(ctrl);

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H265PictureToV4L2DecodeSurface(pic.get());

  // In the order seen in |struct v4l2_hevc_decode_param|.
  struct v4l2_ctrl_hevc_decode_params v4l2_decode_param = {
      .pic_order_cnt_val = pic->pic_order_cnt_val_,
      .short_term_ref_pic_set_size = static_cast<__u16>(slice_hdr->st_rps_bits),
      .long_term_ref_pic_set_size = static_cast<__u16>(slice_hdr->lt_rps_bits),
#if BUILDFLAG(IS_CHROMEOS)
      // .num_delta_pocs_of_ref_rps_idx is upstream but not yet pulled
      // into linux build sysroot.
      // TODO(wenst): Remove once linux-libc-dev package is updated to
      // at least v6.5 in the sysroots.
      .num_delta_pocs_of_ref_rps_idx =
          static_cast<__u8>(slice_hdr->st_ref_pic_set.rps_idx_num_delta_pocs),
#endif
      .flags = static_cast<__u64>(
          (pic->irap_pic_ ? V4L2_HEVC_DECODE_PARAM_FLAG_IRAP_PIC : 0) |
          ((pic->nal_unit_type_ >= H265NALU::IDR_W_RADL &&
            pic->nal_unit_type_ <= H265NALU::IDR_N_LP)
               ? V4L2_HEVC_DECODE_PARAM_FLAG_IDR_PIC
               : 0) |
          (pic->no_output_of_prior_pics_flag_
               ? V4L2_HEVC_DECODE_PARAM_FLAG_NO_OUTPUT_OF_PRIOR
               : 0)),
  };

  // Also sets remaining fields in v4l2_hevc_decode_param
  auto ref_surfaces =
      FillInV4L2DPB(&v4l2_decode_param, ref_pic_list, ref_pic_set_lt_curr,
                    ref_pic_set_st_curr_after, ref_pic_set_st_curr_before);
  dec_surface->SetReferenceSurfaces(ref_surfaces);

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_HEVC_DECODE_PARAMS;
  ctrl.size = sizeof(v4l2_decode_param);
  ctrl.ptr = &v4l2_decode_param;
  ctrls.push_back(ctrl);

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = ctrls.data();
  dec_surface->PrepareSetCtrls(&ext_ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) != 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSExtCtrls);
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return Status::kFail;
  }

  return Status::kOk;
}

H265Decoder::H265Accelerator::Status V4L2VideoDecoderDelegateH265::SubmitSlice(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list0,
    const H265Picture::Vector& ref_pic_list1,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H265PictureToV4L2DecodeSurface(pic.get());

  // Add the 3-bytes NAL start code.
  // TODO: don't do it here, but have it passed from the parser?
  const size_t data_copy_size = size + 3;
  if (dec_surface->secure_handle()) {
    // The secure world already post-processed the secure buffer so that all of
    // the slice NALUs w/ 3 byte start codes are the only contents.
    return surface_handler_->SubmitSlice(dec_surface.get(), nullptr,
                                         data_copy_size)
               ? Status::kOk
               : Status::kFail;
  }
  auto data_copy = base::HeapArray<uint8_t>::Uninit(data_copy_size);
  memset(data_copy.data(), 0, data_copy_size);
  data_copy[2] = 0x01;
  memcpy(data_copy.data() + 3, data, size);
  return surface_handler_->SubmitSlice(dec_surface.get(), data_copy.data(),
                                       data_copy_size)
             ? Status::kOk
             : Status::kFail;
}

H265Decoder::H265Accelerator::Status V4L2VideoDecoderDelegateH265::SubmitDecode(
    scoped_refptr<H265Picture> pic) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H265PictureToV4L2DecodeSurface(pic.get());

  Reset();

  DVLOGF(4) << "Submitting decode for surface: " << dec_surface->ToString();
  surface_handler_->DecodeSurface(dec_surface);
  return Status::kOk;
}

bool V4L2VideoDecoderDelegateH265::OutputPicture(
    scoped_refptr<H265Picture> pic) {
  surface_handler_->SurfaceReady(H265PictureToV4L2DecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 pic->get_colorspace());
  return true;
}

void V4L2VideoDecoderDelegateH265::Reset() {}

bool V4L2VideoDecoderDelegateH265::IsChromaSamplingSupported(
    VideoChromaSampling chroma_sampling) {
  return chroma_sampling == VideoChromaSampling::k420;
}

scoped_refptr<V4L2DecodeSurface>
V4L2VideoDecoderDelegateH265::H265PictureToV4L2DecodeSurface(H265Picture* pic) {
  V4L2H265Picture* v4l2_pic = pic->AsV4L2H265Picture();
  CHECK(v4l2_pic);
  return v4l2_pic->dec_surface();
}

}  // namespace media
