// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/h265_vaapi_wrapper.h"

#include "build/chromeos_buildflags.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/test/macros.h"

#include <va/va.h>
namespace media {

namespace {

// Equation 5-8 in spec.
int Clip3(int x, int y, int z) {
  if (z < x)
    return x;
  if (z > y)
    return y;
  return z;
}

// Fill |va_pic| with default/neutral values.
void InitVAPicture(VAPictureHEVC* va_pic) {
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_HEVC_INVALID;
}

constexpr int kInvalidRefPicIndex = -1;

bool IsValidVABufferType(VABufferType type) {
  return type < VABufferTypeMax;
}

}  // namespace

namespace vaapi_test {

H265VaapiWrapper::H265VaapiWrapper(const VaapiDevice& va_device)
    : va_device_(va_device), va_config_(nullptr), va_context_(nullptr) {
  ref_pic_list_pocs_.reserve(kMaxRefIdxActive);
}

H265VaapiWrapper::~H265VaapiWrapper() = default;

scoped_refptr<H265Picture> H265VaapiWrapper::CreateH265Picture(
    const H265SPS* sps) {
  const VAProfile profile = GetProfile(sps);
  const gfx::Size size = sps->GetVisibleRect().size();

  if (!va_config_) {
    va_config_ = std::make_unique<ScopedVAConfig>(*va_device_, profile,
                                                  GetFormatForProfile(profile));
  }
  if (!va_context_) {
    va_context_ =
        std::make_unique<ScopedVAContext>(*va_device_, *va_config_, size);
  }

  VASurfaceAttrib attribute{};
  memset(&attribute, 0, sizeof(attribute));
  attribute.type = VASurfaceAttribUsageHint;
  attribute.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribute.value.type = VAGenericValueTypeInteger;
  attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;

  scoped_refptr<SharedVASurface> surface = SharedVASurface::Create(
      *va_device_, va_config_->va_rt_format(), size, attribute);
  return base::WrapRefCounted(new vaapi_test::H265Picture(surface));
}

bool H265VaapiWrapper::IsChromaSamplingSupported(
    VideoChromaSampling chroma_sampling) {
  return chroma_sampling == VideoChromaSampling::k420;
}

bool H265VaapiWrapper::SubmitFrameMetadata(
    const H265SPS* sps,
    const H265PPS* pps,
    const H265SliceHeader* slice_hdr,
    const H265Picture::Vector& ref_pic_list,
    scoped_refptr<H265Picture> pic) {
  DCHECK(!last_slice_data_.size());
  VAPictureParameterBufferHEVC pic_param;
  memset(&pic_param, 0, sizeof(pic_param));

  int highest_tid = sps->sps_max_sub_layers_minus1;
#define FROM_SPS_TO_PP(a) pic_param.a = sps->a
#define FROM_SPS_TO_PP2(a, b) pic_param.b = sps->a
#define FROM_PPS_TO_PP(a) pic_param.a = pps->a
#define FROM_SPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = sps->a
#define FROM_PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps->a
#define FROM_SPS_TO_PP_SPF(a) pic_param.slice_parsing_fields.bits.a = sps->a
#define FROM_PPS_TO_PP_SPF(a) pic_param.slice_parsing_fields.bits.a = pps->a
#define FROM_PPS_TO_PP_SPF2(a, b) pic_param.slice_parsing_fields.bits.b = pps->a
  FROM_SPS_TO_PP(pic_width_in_luma_samples);
  FROM_SPS_TO_PP(pic_height_in_luma_samples);
  FROM_SPS_TO_PP_PF(chroma_format_idc);
  FROM_SPS_TO_PP_PF(separate_colour_plane_flag);
  FROM_SPS_TO_PP_PF(pcm_enabled_flag);
  FROM_SPS_TO_PP_PF(scaling_list_enabled_flag);
  FROM_PPS_TO_PP_PF(transform_skip_enabled_flag);
  FROM_SPS_TO_PP_PF(amp_enabled_flag);
  FROM_SPS_TO_PP_PF(strong_intra_smoothing_enabled_flag);
  FROM_PPS_TO_PP_PF(sign_data_hiding_enabled_flag);
  FROM_PPS_TO_PP_PF(constrained_intra_pred_flag);
  FROM_PPS_TO_PP_PF(cu_qp_delta_enabled_flag);
  FROM_PPS_TO_PP_PF(weighted_pred_flag);
  FROM_PPS_TO_PP_PF(weighted_bipred_flag);
  FROM_PPS_TO_PP_PF(transquant_bypass_enabled_flag);
  FROM_PPS_TO_PP_PF(tiles_enabled_flag);
  FROM_PPS_TO_PP_PF(entropy_coding_sync_enabled_flag);
  FROM_PPS_TO_PP_PF(pps_loop_filter_across_slices_enabled_flag);
  FROM_PPS_TO_PP_PF(loop_filter_across_tiles_enabled_flag);
  FROM_SPS_TO_PP_PF(pcm_loop_filter_disabled_flag);
  pic_param.pic_fields.bits.NoPicReorderingFlag =
      (sps->sps_max_num_reorder_pics[highest_tid] == 0) ? 1 : 0;

  FROM_SPS_TO_PP2(sps_max_dec_pic_buffering_minus1[highest_tid],
                  sps_max_dec_pic_buffering_minus1);
  FROM_SPS_TO_PP(bit_depth_luma_minus8);
  FROM_SPS_TO_PP(bit_depth_chroma_minus8);
  FROM_SPS_TO_PP(pcm_sample_bit_depth_luma_minus1);
  FROM_SPS_TO_PP(pcm_sample_bit_depth_chroma_minus1);
  FROM_SPS_TO_PP(log2_min_luma_coding_block_size_minus3);
  FROM_SPS_TO_PP(log2_diff_max_min_luma_coding_block_size);
  FROM_SPS_TO_PP2(log2_min_luma_transform_block_size_minus2,
                  log2_min_transform_block_size_minus2);
  FROM_SPS_TO_PP2(log2_diff_max_min_luma_transform_block_size,
                  log2_diff_max_min_transform_block_size);
  FROM_SPS_TO_PP(log2_min_pcm_luma_coding_block_size_minus3);
  FROM_SPS_TO_PP(log2_diff_max_min_pcm_luma_coding_block_size);
  FROM_SPS_TO_PP(max_transform_hierarchy_depth_intra);
  FROM_SPS_TO_PP(max_transform_hierarchy_depth_inter);
  FROM_PPS_TO_PP(init_qp_minus26);
  FROM_PPS_TO_PP(diff_cu_qp_delta_depth);
  FROM_PPS_TO_PP(pps_cb_qp_offset);
  FROM_PPS_TO_PP(pps_cr_qp_offset);
  FROM_PPS_TO_PP(log2_parallel_merge_level_minus2);
  FROM_PPS_TO_PP(num_tile_columns_minus1);
  FROM_PPS_TO_PP(num_tile_rows_minus1);
  if (pps->uniform_spacing_flag) {
    // We need to calculate this ourselves per 6.5.1 in the spec. We subtract 1
    // as well so it matches the 'minus1' usage in the struct.
    for (int i = 0; i <= pps->num_tile_columns_minus1; ++i) {
      pic_param.column_width_minus1[i] = (((i + 1) * sps->pic_width_in_ctbs_y) /
                                          (pps->num_tile_columns_minus1 + 1)) -
                                         ((i * sps->pic_width_in_ctbs_y) /
                                          (pps->num_tile_columns_minus1 + 1)) -
                                         1;
    }
    for (int j = 0; j <= pps->num_tile_rows_minus1; ++j) {
      pic_param.row_height_minus1[j] =
          (((j + 1) * sps->pic_height_in_ctbs_y) /
           (pps->num_tile_rows_minus1 + 1)) -
          ((j * sps->pic_height_in_ctbs_y) / (pps->num_tile_rows_minus1 + 1)) -
          1;
    }
  } else {
    for (int i = 0; i <= pps->num_tile_columns_minus1; ++i)
      FROM_PPS_TO_PP(column_width_minus1[i]);
    for (int i = 0; i <= pps->num_tile_rows_minus1; ++i)
      FROM_PPS_TO_PP(row_height_minus1[i]);
  }
  FROM_PPS_TO_PP_SPF(lists_modification_present_flag);
  FROM_SPS_TO_PP_SPF(long_term_ref_pics_present_flag);
  FROM_SPS_TO_PP_SPF(sps_temporal_mvp_enabled_flag);
  FROM_PPS_TO_PP_SPF(cabac_init_present_flag);
  FROM_PPS_TO_PP_SPF(output_flag_present_flag);
  FROM_PPS_TO_PP_SPF(dependent_slice_segments_enabled_flag);
  FROM_PPS_TO_PP_SPF(pps_slice_chroma_qp_offsets_present_flag);
  FROM_SPS_TO_PP_SPF(sample_adaptive_offset_enabled_flag);
  FROM_PPS_TO_PP_SPF(deblocking_filter_override_enabled_flag);
  FROM_PPS_TO_PP_SPF2(pps_deblocking_filter_disabled_flag,
                      pps_disable_deblocking_filter_flag);
  FROM_PPS_TO_PP_SPF(slice_segment_header_extension_present_flag);
  pic_param.slice_parsing_fields.bits.RapPicFlag =
      pic->nal_unit_type_ >= H265NALU::BLA_W_LP &&
      pic->nal_unit_type_ <= H265NALU::CRA_NUT;
  pic_param.slice_parsing_fields.bits.IdrPicFlag =
      pic->nal_unit_type_ >= H265NALU::IDR_W_RADL &&
      pic->nal_unit_type_ <= H265NALU::IDR_N_LP;
  pic_param.slice_parsing_fields.bits.IntraPicFlag = pic->irap_pic_;

  FROM_SPS_TO_PP(log2_max_pic_order_cnt_lsb_minus4);
  FROM_SPS_TO_PP(num_short_term_ref_pic_sets);
  FROM_SPS_TO_PP2(num_long_term_ref_pics_sps, num_long_term_ref_pic_sps);
  FROM_PPS_TO_PP(num_ref_idx_l0_default_active_minus1);
  FROM_PPS_TO_PP(num_ref_idx_l1_default_active_minus1);
  FROM_PPS_TO_PP(pps_beta_offset_div2);
  FROM_PPS_TO_PP(pps_tc_offset_div2);
  FROM_PPS_TO_PP(num_extra_slice_header_bits);
#undef FROM_SPS_TO_PP
#undef FROM_SPS_TO_PP2
#undef FROM_PPS_TO_PP
#undef FROM_SPS_TO_PP_PF
#undef FROM_PPS_TO_PP_PF
#undef FROM_SPS_TO_PP_SPF
#undef FROM_PPS_TO_PP_SPF
#undef FROM_PPS_TO_PP_SPF2
  if (slice_hdr->short_term_ref_pic_set_sps_flag)
    pic_param.st_rps_bits = 0;
  else
    pic_param.st_rps_bits = slice_hdr->st_rps_bits;

  InitVAPicture(&pic_param.CurrPic);
  FillVAPicture(&pic_param.CurrPic, std::move(pic));

  // Init reference pictures' array.
  for (auto& reference_frame : pic_param.ReferenceFrames)
    InitVAPicture(&reference_frame);

  // And fill it with picture info from DPB.
  FillVARefFramesFromRefList(ref_pic_list, pic_param.ReferenceFrames);

  if (!SubmitBuffer(VAPictureParameterBufferType, &pic_param)) {
    LOG(ERROR) << "Failure on submitting pic param buffer";
    return false;
  }

  if (!sps->scaling_list_enabled_flag)
    return true;

  VAIQMatrixBufferHEVC iq_matrix_buf;
  memset(&iq_matrix_buf, 0, sizeof(iq_matrix_buf));

  // We already populated the IQMatrix with default values in the parser if they
  // are not present in the stream, so just fill them all in.
  const H265ScalingListData& scaling_list =
      pps->pps_scaling_list_data_present_flag ? pps->scaling_list_data
                                              : sps->scaling_list_data;

  // We need another one of these since we can't use |scaling_list| above in
  // the static_assert checks below.
  H265ScalingListData checker;
  static_assert((std::size(checker.scaling_list_4x4) ==
                 std::size(iq_matrix_buf.ScalingList4x4)) &&
                    (std::size(checker.scaling_list_4x4[0]) ==
                     std::size(iq_matrix_buf.ScalingList4x4[0])) &&
                    (std::size(checker.scaling_list_8x8) ==
                     std::size(iq_matrix_buf.ScalingList8x8)) &&
                    (std::size(checker.scaling_list_8x8[0]) ==
                     std::size(iq_matrix_buf.ScalingList8x8[0])) &&
                    (std::size(checker.scaling_list_16x16) ==
                     std::size(iq_matrix_buf.ScalingList16x16)) &&
                    (std::size(checker.scaling_list_16x16[0]) ==
                     std::size(iq_matrix_buf.ScalingList16x16[0])) &&
                    (std::size(checker.scaling_list_32x32) / 3 ==
                     std::size(iq_matrix_buf.ScalingList32x32)) &&
                    (std::size(checker.scaling_list_32x32[0]) ==
                     std::size(iq_matrix_buf.ScalingList32x32[0])) &&
                    (std::size(checker.scaling_list_dc_coef_16x16) ==
                     std::size(iq_matrix_buf.ScalingListDC16x16)) &&
                    (std::size(checker.scaling_list_dc_coef_32x32) / 3 ==
                     std::size(iq_matrix_buf.ScalingListDC32x32)),
                "Mismatched HEVC scaling list matrix sizes");

  for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; ++i) {
    for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId0Count; ++j) {
      iq_matrix_buf.ScalingList4x4[i][j] =
          scaling_list.GetScalingList4x4EntryInRasterOrder(/*matrix_id=*/i,
                                                           /*raster_idx=*/j);
    }
  }

  for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; ++i) {
    for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId1To3Count;
         ++j) {
      iq_matrix_buf.ScalingList8x8[i][j] =
          scaling_list.GetScalingList8x8EntryInRasterOrder(/*matrix_id=*/i,
                                                           /*raster_idx=*/j);
    }
  }

  for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; ++i) {
    for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId1To3Count;
         ++j) {
      iq_matrix_buf.ScalingList16x16[i][j] =
          scaling_list.GetScalingList16x16EntryInRasterOrder(/*matrix_id=*/i,
                                                             /*raster_idx=*/j);
    }
  }

  for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; i += 3) {
    for (size_t j = 0; j < H265ScalingListData::kScalingListSizeId1To3Count;
         ++j) {
      iq_matrix_buf.ScalingList32x32[i / 3][j] =
          scaling_list.GetScalingList32x32EntryInRasterOrder(/*matrix_id=*/i,
                                                             /*raster_idx=*/j);
    }
  }

  for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; ++i) {
    iq_matrix_buf.ScalingListDC16x16[i] =
        scaling_list.scaling_list_dc_coef_16x16[i];
  }

  for (size_t i = 0; i < H265ScalingListData::kNumScalingListMatrices; i += 3) {
    iq_matrix_buf.ScalingListDC32x32[i / 3] =
        scaling_list.scaling_list_dc_coef_32x32[i];
  }

  return SubmitBuffer(VAIQMatrixBufferType, &iq_matrix_buf);
}

bool H265VaapiWrapper::SubmitSlice(
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
  if (!SubmitPriorSliceDataIfPresent(false)) {
    LOG(ERROR) << "Failure submitting prior slice data";
    return false;
  }

  memset(&slice_param_, 0, sizeof(slice_param_));

  slice_param_.slice_data_size = slice_hdr->nalu_size;
  slice_param_.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param_.slice_data_byte_offset = slice_hdr->header_size;

#define SHDR_TO_SP(a) slice_param_.a = slice_hdr->a
#define SHDR_TO_SP2(a, b) slice_param_.b = slice_hdr->a
#define SHDR_TO_SP_LSF(a) slice_param_.LongSliceFlags.fields.a = slice_hdr->a
#define SHDR_TO_SP_LSF2(a, b) \
  slice_param_.LongSliceFlags.fields.a = slice_hdr->b
  SHDR_TO_SP(slice_segment_address);
  const auto ref_pic_list0_size = ref_pic_list0.size();
  const auto ref_pic_list1_size = ref_pic_list1.size();
  // Fill in ref pic lists.
  if (ref_pic_list0_size > std::size(slice_param_.RefPicList[0]) ||
      ref_pic_list1_size > std::size(slice_param_.RefPicList[1])) {
    LOG(ERROR) << "Error, slice reference picture list is larger than 15";
    return false;
  }

  constexpr int kVaInvalidRefPicIndex = 0xFF;
  std::fill_n(slice_param_.RefPicList[0], std::size(slice_param_.RefPicList[0]),
              kVaInvalidRefPicIndex);
  std::fill_n(slice_param_.RefPicList[1], std::size(slice_param_.RefPicList[1]),
              kVaInvalidRefPicIndex);
  // There may be null entries in |ref_pic_list0| or |ref_pic_list1| for missing
  // reference pictures, just leave those marked as 0xFF and the accelerator
  // will do the right thing to deal with missing reference pictures.
  for (size_t i = 0; i < ref_pic_list0_size; ++i) {
    if (ref_pic_list0[i]) {
      int idx = GetRefPicIndex(ref_pic_list0[i]->pic_order_cnt_val_);
      if (idx == kInvalidRefPicIndex) {
        LOG(ERROR) << "Error, slice reference picture is not in reference list";
        return false;
      }
      slice_param_.RefPicList[0][i] = idx;
    }
  }
  for (size_t i = 0; i < ref_pic_list1_size; ++i) {
    if (ref_pic_list1[i]) {
      int idx = GetRefPicIndex(ref_pic_list1[i]->pic_order_cnt_val_);
      if (idx == kInvalidRefPicIndex) {
        LOG(ERROR) << "Error, slice reference picture is not in reference list";
        return false;
      }
      slice_param_.RefPicList[1][i] = idx;
    }
  }

  SHDR_TO_SP_LSF(dependent_slice_segment_flag);
  SHDR_TO_SP_LSF(slice_type);
  SHDR_TO_SP_LSF2(color_plane_id, colour_plane_id);
  SHDR_TO_SP_LSF(slice_sao_luma_flag);
  SHDR_TO_SP_LSF(slice_sao_chroma_flag);
  SHDR_TO_SP_LSF(mvd_l1_zero_flag);
  SHDR_TO_SP_LSF(cabac_init_flag);
  SHDR_TO_SP_LSF(slice_temporal_mvp_enabled_flag);
  SHDR_TO_SP_LSF(slice_deblocking_filter_disabled_flag);
  SHDR_TO_SP_LSF(collocated_from_l0_flag);
  SHDR_TO_SP_LSF(slice_loop_filter_across_slices_enabled_flag);
  if (!slice_hdr->slice_temporal_mvp_enabled_flag)
    slice_param_.collocated_ref_idx = kVaInvalidRefPicIndex;
  else
    SHDR_TO_SP(collocated_ref_idx);

  slice_param_.num_ref_idx_l0_active_minus1 =
      ref_pic_list0_size ? (ref_pic_list0_size - 1) : 0;
  slice_param_.num_ref_idx_l1_active_minus1 =
      ref_pic_list1_size ? (ref_pic_list1_size - 1) : 0;
  SHDR_TO_SP(slice_qp_delta);
  SHDR_TO_SP(slice_cb_qp_offset);
  SHDR_TO_SP(slice_cr_qp_offset);
  SHDR_TO_SP(slice_beta_offset_div2);
  SHDR_TO_SP(slice_tc_offset_div2);
  SHDR_TO_SP2(pred_weight_table.luma_log2_weight_denom, luma_log2_weight_denom);
  SHDR_TO_SP2(pred_weight_table.delta_chroma_log2_weight_denom,
              delta_chroma_log2_weight_denom);
  for (int i = 0; i < kMaxRefIdxActive; ++i) {
    SHDR_TO_SP2(pred_weight_table.delta_luma_weight_l0[i],
                delta_luma_weight_l0[i]);
    SHDR_TO_SP2(pred_weight_table.luma_offset_l0[i], luma_offset_l0[i]);
    if (slice_hdr->IsBSlice()) {
      SHDR_TO_SP2(pred_weight_table.delta_luma_weight_l1[i],
                  delta_luma_weight_l1[i]);
      SHDR_TO_SP2(pred_weight_table.luma_offset_l1[i], luma_offset_l1[i]);
    }
    for (int j = 0; j < 2; ++j) {
      SHDR_TO_SP2(pred_weight_table.delta_chroma_weight_l0[i][j],
                  delta_chroma_weight_l0[i][j]);
      int chroma_weight_l0 =
          (1 << slice_hdr->pred_weight_table.chroma_log2_weight_denom) +
          slice_hdr->pred_weight_table.delta_chroma_weight_l0[i][j];
      slice_param_.ChromaOffsetL0[i][j] =
          Clip3(-sps->wp_offset_half_range_c, sps->wp_offset_half_range_c - 1,
                (sps->wp_offset_half_range_c +
                 slice_hdr->pred_weight_table.delta_chroma_offset_l0[i][j] -
                 ((sps->wp_offset_half_range_c * chroma_weight_l0) >>
                  slice_hdr->pred_weight_table.chroma_log2_weight_denom)));
      if (slice_hdr->IsBSlice()) {
        SHDR_TO_SP2(pred_weight_table.delta_chroma_weight_l1[i][j],
                    delta_chroma_weight_l1[i][j]);
        int chroma_weight_l1 =
            (1 << slice_hdr->pred_weight_table.chroma_log2_weight_denom) +
            slice_hdr->pred_weight_table.delta_chroma_weight_l1[i][j];
        slice_param_.ChromaOffsetL1[i][j] =
            Clip3(-sps->wp_offset_half_range_c, sps->wp_offset_half_range_c - 1,
                  (sps->wp_offset_half_range_c +
                   slice_hdr->pred_weight_table.delta_chroma_offset_l1[i][j] -
                   ((sps->wp_offset_half_range_c * chroma_weight_l1) >>
                    slice_hdr->pred_weight_table.chroma_log2_weight_denom)));
      }
    }
  }
  SHDR_TO_SP(five_minus_max_num_merge_cand);

  // TODO(jchinlee): Remove this guard once Chrome has libva uprev'd to 2.6.0.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  slice_param_.slice_data_num_emu_prevn_bytes =
      slice_hdr->header_emulation_prevention_bytes;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  last_slice_data_.assign(data, data + size);
  return true;
}

bool H265VaapiWrapper::SubmitDecode(scoped_refptr<H265Picture> pic) {
  if (!SubmitPriorSliceDataIfPresent(true)) {
    LOG(ERROR) << "Failure submitting prior slice data";
    return false;
  }

  CHECK(gfx::Rect(pic->surface->size()).Contains(pic->visible_rect()));

  const bool success = ExecuteAndDestroyPendingBuffers(pic->surface->id());
  ref_pic_list_pocs_.clear();
  return success;
}

void H265VaapiWrapper::Reset() {
  DestroyPendingBuffers();
  ref_pic_list_pocs_.clear();
  last_slice_data_.clear();
}

void H265VaapiWrapper::FillVAPicture(VAPictureHEVC* va_pic,
                                     scoped_refptr<H265Picture> pic) {
  va_pic->picture_id = pic->surface->id();
  va_pic->pic_order_cnt = pic->pic_order_cnt_val_;
  va_pic->flags = 0;

  switch (pic->ref_) {
    case H265Picture::kShortTermCurrBefore:
      va_pic->flags |= VA_PICTURE_HEVC_RPS_ST_CURR_BEFORE;
      break;
    case H265Picture::kShortTermCurrAfter:
      va_pic->flags |= VA_PICTURE_HEVC_RPS_ST_CURR_AFTER;
      break;
    case H265Picture::kLongTermCurr:
      va_pic->flags |= VA_PICTURE_HEVC_RPS_LT_CURR;
      break;
    default:  // We don't flag the other ref pic types.
      break;
  }

  if (pic->IsLongTermRef())
    va_pic->flags |= VA_PICTURE_HEVC_LONG_TERM_REFERENCE;
}

void H265VaapiWrapper::FillVARefFramesFromRefList(
    const H265Picture::Vector& ref_pic_list,
    VAPictureHEVC* va_pics) {
  ref_pic_list_pocs_.clear();
  for (auto& it : ref_pic_list) {
    if (!it->IsUnused()) {
      FillVAPicture(&va_pics[ref_pic_list_pocs_.size()], it);
      ref_pic_list_pocs_.push_back(it->pic_order_cnt_val_);
    }
  }
}

int H265VaapiWrapper::GetRefPicIndex(int poc) {
  for (size_t i = 0; i < ref_pic_list_pocs_.size(); ++i) {
    if (ref_pic_list_pocs_[i] == poc)
      return static_cast<int>(i);
  }
  return kInvalidRefPicIndex;
}

bool H265VaapiWrapper::SubmitPriorSliceDataIfPresent(bool last_slice) {
  if (last_slice_data_.empty()) {
    // No prior slice data to submit.
    return true;
  }
  if (last_slice)
    slice_param_.LongSliceFlags.fields.LastSliceOfPic = 1;

  bool success;
  success = SubmitBuffers(
      {{VASliceParameterBufferType, sizeof(slice_param_), &slice_param_},
       {VASliceDataBufferType, last_slice_data_.size(),
        last_slice_data_.data()}});
  last_slice_data_.clear();
  return success;
}

bool H265VaapiWrapper::SubmitBuffer(VABufferType va_buffer_type,
                                    size_t size,
                                    const void* data) {
  CHECK(IsValidVABufferType(va_buffer_type));

  if (!data) {
    LOG(INFO) << "Data pointer is null.";
    return false;
  }

  DVLOG(3) << "Submitting Buffer:(size=" << size << ")";
  VABufferID buffer_id;
  {
    const VAStatus va_res =
        vaCreateBuffer(va_device_->display(), va_context_->id(), va_buffer_type,
                       size, 1, const_cast<void*>(data), &buffer_id);
    VA_LOG_ASSERT(va_res, "vaCreateBuffer");
  }

  pending_buffers_.push_back(buffer_id);
  return true;
}

bool H265VaapiWrapper::SubmitBuffers(
    const std::vector<VABufferDescriptor>& va_buffers) {
  for (const VABufferDescriptor& va_buffer : va_buffers) {
    CHECK(SubmitBuffer(va_buffer.type, va_buffer.size, va_buffer.data));
  }
  return true;
}

void H265VaapiWrapper::DestroyPendingBuffers() {
  for (const auto& pending_va_buf : pending_buffers_) {
    vaDestroyBuffer(va_device_->display(), pending_va_buf);
  }
  pending_buffers_.clear();
}

bool H265VaapiWrapper::ExecuteAndDestroyPendingBuffers(
    VASurfaceID va_surface_id) {
  DVLOG(3) << "Pending VA bufs to commit: " << pending_buffers_.size();
  DVLOG(3) << "Target VA surface " << va_surface_id;

  // Get ready to execute for given surface.
  VAStatus va_res =
      vaBeginPicture(va_device_->display(), va_context_->id(), va_surface_id);
  VA_LOG_ASSERT(va_res, "vaBeginPicture");

  if (!pending_buffers_.empty()) {
    // vaRenderPicture() needs a non-const pointer, possibly unnecessarily.
    VABufferID* va_buffers_data =
        const_cast<VABufferID*>(pending_buffers_.data());
    va_res = vaRenderPicture(va_device_->display(), va_context_->id(),
                             va_buffers_data,
                             base::checked_cast<int>(pending_buffers_.size()));
    VA_LOG_ASSERT(va_res, "vaRenderPicture_VABuffers");
  }

  // Instruct HW codec to start processing the submitted commands. In theory,
  // this shouldn't be blocking, relying on vaSyncSurface() instead, however
  // evidence points to it actually waiting for the job to be done.
  va_res = vaEndPicture(va_device_->display(), va_context_->id());
  VA_LOG_ASSERT(va_res, "vaEndPicture");
  // Destroy the pending buffers after job is complete.
  DestroyPendingBuffers();
  return true;
}

VAProfile H265VaapiWrapper::GetProfile(const H265SPS* sps) {
  switch (sps->profile_tier_level.general_profile_idc) {
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdcMain:
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdcMainStill:
      return VAProfile::VAProfileHEVCMain;
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdcMain10:
      return VAProfile::VAProfileHEVCMain10;
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdcRangeExtensions:
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdcHighThroughput:
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdcScalableMain:
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdc3dMain:
    case H265ProfileTierLevel::H265ProfileIdc::kProfileIdcScreenContentCoding:
    case H265ProfileTierLevel::H265ProfileIdc::
        kProfileIdcScalableRangeExtensions:
    case H265ProfileTierLevel::H265ProfileIdc::
        kProfileIdcHighThroughputScreenContentCoding:
    default:
      LOG(FATAL) << "Invalid IDC profile "
                 << sps->profile_tier_level.general_profile_idc;
  }
}

unsigned int H265VaapiWrapper::GetFormatForProfile(const VAProfile& profile) {
  return VA_RT_FORMAT_YUV420;
}

}  // namespace vaapi_test
}  // namespace media
