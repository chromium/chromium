// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/h265_delegate.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include "media/gpu/macros.h"
#include "media/gpu/v4l2/stateless/stateless_decode_surface_handler.h"

namespace media {
namespace {
class StatelessH265Picture : public H265Picture {
 public:
  explicit StatelessH265Picture(
      scoped_refptr<StatelessDecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  StatelessH265Picture(const StatelessH265Picture&) = delete;
  StatelessH265Picture& operator=(const StatelessH265Picture&) = delete;

  const scoped_refptr<StatelessDecodeSurface> dec_surface() const {
    return dec_surface_;
  }

 private:
  ~StatelessH265Picture() override {}

  scoped_refptr<StatelessDecodeSurface> dec_surface_;
};

scoped_refptr<StatelessDecodeSurface> H265PictureToStatelessDecodeSurface(
    H265Picture* pic) {
  CHECK(pic);
  StatelessH265Picture* stateless_h265_picture =
      static_cast<StatelessH265Picture*>(pic);

  return stateless_h265_picture->dec_surface();
}

}  // namespace

struct H265DelegateContext {
  struct v4l2_ctrl_hevc_sps v4l2_sps;
  struct v4l2_ctrl_hevc_pps v4l2_pps;
  struct v4l2_ctrl_hevc_scaling_matrix v4l2_scaling_matrix;
  struct v4l2_ctrl_hevc_decode_params v4l2_decode_param;
  std::vector<uint8_t> slice_data;
};

std::vector<scoped_refptr<StatelessDecodeSurface>> H265Delegate::FillInV4L2DPB(
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before) {
  std::vector<scoped_refptr<StatelessDecodeSurface>> ref_surfaces;
  memset(ctx_->v4l2_decode_param.dpb, 0, sizeof(ctx_->v4l2_decode_param.dpb));
  unsigned int i = 0;
  for (const auto& pic : ref_pic_list) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      LOG(ERROR) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    unsigned int index = 0xff;
    if (!pic->IsUnused()) {
      scoped_refptr<StatelessDecodeSurface> dec_surface =
          H265PictureToStatelessDecodeSurface(pic.get());
      index = dec_surface->GetReferenceTimestamp();
      ref_surfaces.push_back(dec_surface);
    }

    ctx_->v4l2_decode_param.dpb[i++] = {
        .timestamp = index,
        .flags = static_cast<__u8>(
            pic->IsLongTermRef() ? V4L2_HEVC_DPB_ENTRY_LONG_TERM_REFERENCE : 0),
        .field_pic = V4L2_HEVC_SEI_PIC_STRUCT_FRAME,  // No interlaced support
        .pic_order_cnt_val = pic->pic_order_cnt_val_,
    };
  }
  ctx_->v4l2_decode_param.num_active_dpb_entries = i;

  // Set defaults
  std::fill_n(ctx_->v4l2_decode_param.poc_st_curr_before,
              std::size(ctx_->v4l2_decode_param.poc_st_curr_before), 0xff);
  std::fill_n(ctx_->v4l2_decode_param.poc_st_curr_after,
              std::size(ctx_->v4l2_decode_param.poc_st_curr_after), 0xff);
  std::fill_n(ctx_->v4l2_decode_param.poc_lt_curr,
              std::size(ctx_->v4l2_decode_param.poc_lt_curr), 0xff);

  i = 0;
  for (const auto& pic : ref_pic_set_st_curr_before) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      LOG(ERROR) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < ctx_->v4l2_decode_param.num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          ctx_->v4l2_decode_param.dpb[j].pic_order_cnt_val) {
        ctx_->v4l2_decode_param.poc_st_curr_before[i++] = j;
        break;
      }
    }
  }
  ctx_->v4l2_decode_param.num_poc_st_curr_before = i;

  i = 0;
  for (const auto& pic : ref_pic_set_st_curr_after) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      LOG(ERROR) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < ctx_->v4l2_decode_param.num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          ctx_->v4l2_decode_param.dpb[j].pic_order_cnt_val) {
        ctx_->v4l2_decode_param.poc_st_curr_after[i++] = j;
        break;
      }
    }
  }
  ctx_->v4l2_decode_param.num_poc_st_curr_after = i;

  i = 0;
  for (const auto& pic : ref_pic_set_lt_curr) {
    if (i >= V4L2_HEVC_DPB_ENTRIES_NUM_MAX) {
      LOG(ERROR) << "Invalid DPB size";
      break;
    }

    if (!pic) {
      continue;
    }

    for (unsigned int j = 0; j < ctx_->v4l2_decode_param.num_active_dpb_entries;
         j++) {
      if (pic->pic_order_cnt_val_ ==
          ctx_->v4l2_decode_param.dpb[j].pic_order_cnt_val) {
        ctx_->v4l2_decode_param.poc_lt_curr[i++] = j;
        break;
      }
    }
  }
  ctx_->v4l2_decode_param.num_poc_lt_curr = i;

  return ref_surfaces;
}

H265Delegate::H265Delegate(StatelessDecodeSurfaceHandler* surface_handler)
    : surface_handler_(surface_handler),
      ctx_(std::make_unique<H265DelegateContext>()) {
  DCHECK(surface_handler_);
}

H265Delegate::~H265Delegate() = default;

scoped_refptr<H265Picture> H265Delegate::CreateH265Picture() {
  scoped_refptr<StatelessDecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface) {
    return nullptr;
  }

  return new StatelessH265Picture(dec_surface);
}

H265Delegate::Status H265Delegate::SubmitFrameMetadata(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    const H265Picture::Vector& ref_pic_set_lt_curr,
    const H265Picture::Vector& ref_pic_set_st_curr_after,
    const H265Picture::Vector& ref_pic_set_st_curr_before,
    scoped_refptr<H265Picture> pic) {
  memset(&ctx_->v4l2_sps, 0, sizeof(ctx_->v4l2_sps));

  int highest_tid = sps->sps_max_sub_layers_minus1;

  // In the order of |struct v4l2_ctrl_hevc_sps|
  ctx_->v4l2_sps.video_parameter_set_id = sps->sps_video_parameter_set_id;
  ctx_->v4l2_sps.seq_parameter_set_id = sps->sps_seq_parameter_set_id;
#define SPS_TO_V4L2SPS(a) ctx_->v4l2_sps.a = sps->a
  SPS_TO_V4L2SPS(pic_width_in_luma_samples);
  SPS_TO_V4L2SPS(pic_height_in_luma_samples);
  SPS_TO_V4L2SPS(bit_depth_luma_minus8);
  SPS_TO_V4L2SPS(bit_depth_chroma_minus8);
  SPS_TO_V4L2SPS(log2_max_pic_order_cnt_lsb_minus4);
#define SPS_TO_V4L2SPS_FROM_ARRAY(a) ctx_->v4l2_sps.a = sps->a[highest_tid];
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
  ctx_->v4l2_sps.flags |= ((sps->cond) ? (flag) : 0)
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

  memset(&ctx_->v4l2_pps, 0, sizeof(ctx_->v4l2_pps));
  // In the order of |struct v4l2_ctrl_hevc_pps|
#define PPS_TO_V4L2PPS(a) ctx_->v4l2_pps.a = pps->a
  ctx_->v4l2_pps.pic_parameter_set_id = pps->pps_pic_parameter_set_id;
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
      CHECK_GE(std::size(ctx_->v4l2_pps.column_width_minus1),
               std::extent<decltype(pps->column_width_minus1)>());
      for (int i = 0; i <= pps->num_tile_columns_minus1; ++i) {
        ctx_->v4l2_pps.column_width_minus1[i] = pps->column_width_minus1[i];
      }

      CHECK_GE(std::size(ctx_->v4l2_pps.row_height_minus1),
               std::extent<decltype(pps->row_height_minus1)>());
      for (int i = 0; i <= pps->num_tile_rows_minus1; ++i) {
        ctx_->v4l2_pps.row_height_minus1[i] = pps->row_height_minus1[i];
      }
    }
  }

  PPS_TO_V4L2PPS(pps_beta_offset_div2);
  PPS_TO_V4L2PPS(pps_tc_offset_div2);
  PPS_TO_V4L2PPS(log2_parallel_merge_level_minus2);
#undef PPS_TO_V4L2PPS

#define SET_V4L2_PPS_FLAG_IF(cond, flag) \
  ctx_->v4l2_pps.flags |= ((pps->cond) ? (flag) : 0)
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

  memset(&ctx_->v4l2_scaling_matrix, 0, sizeof(ctx_->v4l2_scaling_matrix));
  struct H265ScalingListData checker;

  // TODO(jkardatzke): Optimize storage of the 32x32 since only indices 0 and 3
  // are actually used. See ../../video/h265_parser.h
  CHECK(std::size(checker.scaling_list_dc_coef_16x16) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_dc_coef_16x16) &&
        std::size(checker.scaling_list_dc_coef_32x32) / 3 ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_dc_coef_32x32) &&
        std::size(checker.scaling_list_4x4) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_4x4) &&
        std::size(checker.scaling_list_4x4[0]) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_4x4[0]) &&
        std::size(checker.scaling_list_8x8) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_8x8) &&
        std::size(checker.scaling_list_8x8[0]) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_8x8[0]) &&
        std::size(checker.scaling_list_16x16) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_16x16) &&
        std::size(checker.scaling_list_16x16[0]) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_16x16[0]) &&
        std::size(checker.scaling_list_32x32) / 3 ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_32x32) &&
        std::size(checker.scaling_list_32x32[0]) ==
            std::size(ctx_->v4l2_scaling_matrix.scaling_list_32x32[0]));

  if (sps->scaling_list_enabled_flag) {
    // Copied from H265VaapiVideoDecoderDelegate:
    // We already populated the scaling list data with default values in the
    // parser if they are not present in the stream, so just fill them all in.
    const auto& scaling_list = pps->pps_scaling_list_data_present_flag
                                   ? pps->scaling_list_data
                                   : sps->scaling_list_data;

    memcpy(ctx_->v4l2_scaling_matrix.scaling_list_4x4,
           scaling_list.scaling_list_4x4,
           sizeof(ctx_->v4l2_scaling_matrix.scaling_list_4x4));
    memcpy(ctx_->v4l2_scaling_matrix.scaling_list_8x8,
           scaling_list.scaling_list_8x8,
           sizeof(ctx_->v4l2_scaling_matrix.scaling_list_8x8));
    memcpy(ctx_->v4l2_scaling_matrix.scaling_list_16x16,
           scaling_list.scaling_list_16x16,
           sizeof(ctx_->v4l2_scaling_matrix.scaling_list_16x16));
    memcpy(ctx_->v4l2_scaling_matrix.scaling_list_32x32[0],
           scaling_list.scaling_list_32x32[0],
           sizeof(ctx_->v4l2_scaling_matrix.scaling_list_32x32[0]));
    memcpy(ctx_->v4l2_scaling_matrix.scaling_list_32x32[1],
           scaling_list.scaling_list_32x32[3],
           sizeof(ctx_->v4l2_scaling_matrix.scaling_list_32x32[1]));
    memcpy(ctx_->v4l2_scaling_matrix.scaling_list_dc_coef_16x16,
           scaling_list.scaling_list_dc_coef_16x16,
           sizeof(ctx_->v4l2_scaling_matrix.scaling_list_dc_coef_16x16));
    ctx_->v4l2_scaling_matrix.scaling_list_dc_coef_32x32[0] =
        scaling_list.scaling_list_dc_coef_32x32[0];
    ctx_->v4l2_scaling_matrix.scaling_list_dc_coef_32x32[1] =
        scaling_list.scaling_list_dc_coef_32x32[3];
  }

  // In the order seen in |struct v4l2_hevc_decode_param|.
  ctx_->v4l2_decode_param = {
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
      FillInV4L2DPB(ref_pic_list, ref_pic_set_lt_curr,
                    ref_pic_set_st_curr_after, ref_pic_set_st_curr_before);

  scoped_refptr<StatelessDecodeSurface> dec_surface =
      H265PictureToStatelessDecodeSurface(pic.get());

  dec_surface->SetReferenceSurfaces(ref_surfaces);

  return H265Delegate::Status::kOk;
}

H265Delegate::Status H265Delegate::SubmitSlice(
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
  // Adds a start code prefix, a unique sequence of 3 bytes equal to 0x000001
  // embedded in the byte stream as a prefix to each NAL unit. All hardwares
  // supported in ChromeOS require the start code prefix.
  std::vector<uint8_t> slice_data = {0x00, 0x00, 0x01};
  slice_data.insert(slice_data.end(), data, data + size);
  ctx_->slice_data = slice_data;

  return H265Delegate::Status::kOk;
}

H265Delegate::Status H265Delegate::SubmitDecode(
    scoped_refptr<H265Picture> pic) {
  std::vector<struct v4l2_ext_control> ext_ctrls = {
      {.id = V4L2_CID_STATELESS_HEVC_SPS,
       .size = sizeof(ctx_->v4l2_sps),
       .ptr = &ctx_->v4l2_sps},
      {.id = V4L2_CID_STATELESS_HEVC_PPS,
       .size = sizeof(ctx_->v4l2_pps),
       .ptr = &ctx_->v4l2_pps},
      {.id = V4L2_CID_STATELESS_HEVC_SCALING_MATRIX,
       .size = sizeof(ctx_->v4l2_scaling_matrix),
       .ptr = &ctx_->v4l2_scaling_matrix},
      {.id = V4L2_CID_STATELESS_HEVC_DECODE_PARAMS,
       .size = sizeof(ctx_->v4l2_decode_param),
       .ptr = &ctx_->v4l2_decode_param},
  };

  const __u32 ext_ctrls_size = base::checked_cast<__u32>(ext_ctrls.size());
  struct v4l2_ext_controls ctrls = {.count = ext_ctrls_size,
                                    .controls = ext_ctrls.data()};

  StatelessH265Picture* stateless_h265_picture =
      static_cast<StatelessH265Picture*>(pic.get());
  if (!stateless_h265_picture) {
    return H265Delegate::Status::kFail;
  }
  if (!surface_handler_->SubmitFrame(&ctrls, &ctx_->slice_data[0],
                                     ctx_->slice_data.size(),
                                     stateless_h265_picture->dec_surface())) {
    return H265Delegate::Status::kFail;
  }

  return H265Delegate::Status::kOk;
}

bool H265Delegate::OutputPicture(scoped_refptr<H265Picture> pic) {
  DVLOGF(4);
  surface_handler_->SurfaceReady(H265PictureToStatelessDecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 pic->get_colorspace());
  return true;
}

void H265Delegate::Reset() {}

bool H265Delegate::IsChromaSamplingSupported(VideoChromaSampling format) {
  return format == VideoChromaSampling::k420;
}

}  // namespace media
