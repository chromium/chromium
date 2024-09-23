// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/h264_vaapi_wrapper.h"

#include <va/va.h>

#include <algorithm>
#include <memory>

#include "base/trace_event/trace_event.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/test/h264_dpb.h"
#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/scoped_va_config.h"
#include "media/gpu/vaapi/test/scoped_va_context.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/parsers/h264_parser.h"

namespace media::vaapi_test {

namespace {

// from ITU-T REC H.264 spec
// section 8.5.6
// "Inverse scanning process for 4x4 transform coefficients and scaling lists"
static constexpr int kZigzagScan4x4[16] = {0, 1,  4,  8,  5, 2,  3,  6,
                                           9, 12, 13, 10, 7, 11, 14, 15};

// section 8.5.7
// "Inverse scanning process for 8x8 transform coefficients and scaling lists"
static constexpr uint8_t kZigzagScan8x8[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

VAProfile GetProfile(const H264SPS* sps) {
  switch (sps->profile_idc) {
    case H264SPS::kProfileIDCBaseline:
      return VAProfileH264ConstrainedBaseline;
    case H264SPS::kProfileIDCMain:
      return VAProfileH264Main;
    case H264SPS::kProfileIDCHigh:
      return VAProfileH264High;
    case H264SPS::kProfileIDSMultiviewHigh:
      return VAProfileH264MultiviewHigh;
    case H264SPS::kProfileIDStereoHigh:
      return VAProfileH264StereoHigh;
    default:
      LOG(FATAL) << "Invalid IDC profile " << sps->profile_idc;
  }
}

unsigned int GetFormatForProfile(const VAProfile& profile) {
  // VAAPI doesn't support H.264 10 bit color.
  return VA_RT_FORMAT_YUV420;
}

void InitVAPicture(VAPictureH264* va_pic) {
  memset(va_pic, 0, sizeof(*va_pic));
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

void FillVAPicture(VAPictureH264* va_pic, scoped_refptr<H264Picture> pic) {
  VASurfaceID va_surface_id = VA_INVALID_SURFACE;

  if (!pic->nonexisting)
    va_surface_id = pic->surface->id();

  va_pic->picture_id = va_surface_id;
  va_pic->frame_idx = pic->frame_num;
  va_pic->flags = 0;

  switch (pic->field) {
    case H264Picture::FIELD_NONE:
      break;
    case H264Picture::FIELD_TOP:
      va_pic->flags |= VA_PICTURE_H264_TOP_FIELD;
      break;
    case H264Picture::FIELD_BOTTOM:
      va_pic->flags |= VA_PICTURE_H264_BOTTOM_FIELD;
      break;
  }

  if (pic->ref) {
    va_pic->flags |= pic->long_term ? VA_PICTURE_H264_LONG_TERM_REFERENCE
                                    : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
  }

  va_pic->TopFieldOrderCnt = pic->top_field_order_cnt;
  va_pic->BottomFieldOrderCnt = pic->bottom_field_order_cnt;
}

}  // namespace

H264VaapiWrapper::H264VaapiWrapper(const VaapiDevice& va_device)
    : va_device_(va_device), va_config_(nullptr), va_context_(nullptr) {}

H264VaapiWrapper::~H264VaapiWrapper() = default;

scoped_refptr<H264Picture> H264VaapiWrapper::CreatePicture(const H264SPS* sps) {
  const VAProfile profile = GetProfile(sps);
  const gfx::Size size = sps->GetVisibleRect().value().size();

  if (!va_config_) {
    va_config_ = std::make_unique<ScopedVAConfig>(*va_device_, profile,
                                                  GetFormatForProfile(profile));
  }
  if (!va_context_) {
    va_context_ =
        std::make_unique<ScopedVAContext>(*va_device_, *va_config_, size);
  }

  VASurfaceAttrib attribute{};
  attribute.type = VASurfaceAttribUsageHint;
  attribute.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribute.value.type = VAGenericValueTypeInteger;
  attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;

  scoped_refptr<SharedVASurface> surface = SharedVASurface::Create(
      *va_device_, va_config_->va_rt_format(), size, attribute);

  return base::WrapRefCounted(new H264Picture(surface));
}

void H264VaapiWrapper::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    scoped_refptr<H264Picture> pic) {
  VAPictureParameterBufferH264 pic_param;
  memset(&pic_param, 0, sizeof(pic_param));

#define FROM_SPS_TO_PP(a) pic_param.a = sps->a
#define FROM_SPS_TO_PP2(a, b) pic_param.b = sps->a
  FROM_SPS_TO_PP2(pic_width_in_mbs_minus1, picture_width_in_mbs_minus1);
  // This assumes non-interlaced video
  FROM_SPS_TO_PP2(pic_height_in_map_units_minus1, picture_height_in_mbs_minus1);
  FROM_SPS_TO_PP(bit_depth_luma_minus8);
  FROM_SPS_TO_PP(bit_depth_chroma_minus8);
#undef FROM_SPS_TO_PP
#undef FROM_SPS_TO_PP2

#define FROM_SPS_TO_PP_SF(a) pic_param.seq_fields.bits.a = sps->a
#define FROM_SPS_TO_PP_SF2(a, b) pic_param.seq_fields.bits.b = sps->a
  FROM_SPS_TO_PP_SF(chroma_format_idc);
  FROM_SPS_TO_PP_SF2(separate_colour_plane_flag,
                     residual_colour_transform_flag);
  FROM_SPS_TO_PP_SF(gaps_in_frame_num_value_allowed_flag);
  FROM_SPS_TO_PP_SF(frame_mbs_only_flag);
  FROM_SPS_TO_PP_SF(mb_adaptive_frame_field_flag);
  FROM_SPS_TO_PP_SF(direct_8x8_inference_flag);
  pic_param.seq_fields.bits.MinLumaBiPredSize8x8 = (sps->level_idc >= 31);
  FROM_SPS_TO_PP_SF(log2_max_frame_num_minus4);
  FROM_SPS_TO_PP_SF(pic_order_cnt_type);
  FROM_SPS_TO_PP_SF(log2_max_pic_order_cnt_lsb_minus4);
  FROM_SPS_TO_PP_SF(delta_pic_order_always_zero_flag);
#undef FROM_SPS_TO_PP_SF
#undef FROM_SPS_TO_PP_SF2

#define FROM_PPS_TO_PP(a) pic_param.a = pps->a
  FROM_PPS_TO_PP(pic_init_qp_minus26);
  FROM_PPS_TO_PP(pic_init_qs_minus26);
  FROM_PPS_TO_PP(chroma_qp_index_offset);
  FROM_PPS_TO_PP(second_chroma_qp_index_offset);
#undef FROM_PPS_TO_PP

#define FROM_PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps->a
#define FROM_PPS_TO_PP_PF2(a, b) pic_param.pic_fields.bits.b = pps->a
  FROM_PPS_TO_PP_PF(entropy_coding_mode_flag);
  FROM_PPS_TO_PP_PF(weighted_pred_flag);
  FROM_PPS_TO_PP_PF(weighted_bipred_idc);
  FROM_PPS_TO_PP_PF(transform_8x8_mode_flag);

  pic_param.pic_fields.bits.field_pic_flag = 0;
  FROM_PPS_TO_PP_PF(constrained_intra_pred_flag);
  FROM_PPS_TO_PP_PF2(bottom_field_pic_order_in_frame_present_flag,
                     pic_order_present_flag);
  FROM_PPS_TO_PP_PF(deblocking_filter_control_present_flag);
  FROM_PPS_TO_PP_PF(redundant_pic_cnt_present_flag);
  pic_param.pic_fields.bits.reference_pic_flag = pic->ref;
#undef FROM_PPS_TO_PP_PF
#undef FROM_PPS_TO_PP_PF2

  pic_param.frame_num = pic->frame_num;

  InitVAPicture(&pic_param.CurrPic);
  FillVAPicture(&pic_param.CurrPic, std::move(pic));

  // Init reference pictures' array.
  for (int i = 0; i < 16; ++i)
    InitVAPicture(&pic_param.ReferenceFrames[i]);

  // And fill it with our reference frames.
  for (size_t i = 0; i < ref_pic_listp0.size(); i++) {
    FillVAPicture(pic_param.ReferenceFrames + i, ref_pic_listp0[i]);
  }

  pic_param.num_ref_frames = sps->max_num_ref_frames;

  VAIQMatrixBufferH264 iq_matrix_buf;
  memset(&iq_matrix_buf, 0, sizeof(iq_matrix_buf));

  if (pps->pic_scaling_matrix_present_flag) {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][kZigzagScan4x4[j]] =
            pps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][kZigzagScan8x8[j]] =
            pps->scaling_list8x8[i][j];
    }
  } else {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][kZigzagScan4x4[j]] =
            sps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][kZigzagScan8x8[j]] =
            sps->scaling_list8x8[i][j];
    }
  }

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(
      va_device_->display(), va_context_->id(), VAPictureParameterBufferType,
      sizeof(pic_param), 1, &pic_param, &buffer_id);
  VA_LOG_ASSERT(va_res, "vaCreateBuffer");
  buffers_.push_back(buffer_id);
  va_res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                          VAIQMatrixBufferType, sizeof(iq_matrix_buf), 1,
                          &iq_matrix_buf, &buffer_id);
  VA_LOG_ASSERT(va_res, "vaCreateBuffer");
  buffers_.push_back(buffer_id);
}

void H264VaapiWrapper::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  VASliceParameterBufferH264 slice_param;
  memset(&slice_param, 0, sizeof(slice_param));

  slice_param.slice_data_size = slice_hdr->nalu_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.slice_data_bit_offset = slice_hdr->header_bit_size;

#define SHDRToSP(a) slice_param.a = slice_hdr->a
  SHDRToSP(first_mb_in_slice);
  slice_param.slice_type = slice_hdr->slice_type % 5;
  SHDRToSP(direct_spatial_mv_pred_flag);

  SHDRToSP(num_ref_idx_l0_active_minus1);
  SHDRToSP(num_ref_idx_l1_active_minus1);
  SHDRToSP(cabac_init_idc);
  SHDRToSP(slice_qp_delta);
  SHDRToSP(disable_deblocking_filter_idc);
  SHDRToSP(slice_alpha_c0_offset_div2);
  SHDRToSP(slice_beta_offset_div2);

  if (((slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) &&
       pps->weighted_pred_flag) ||
      (slice_hdr->IsBSlice() && pps->weighted_bipred_idc == 1)) {
    SHDRToSP(luma_log2_weight_denom);
    SHDRToSP(chroma_log2_weight_denom);

    SHDRToSP(luma_weight_l0_flag);
    SHDRToSP(luma_weight_l1_flag);

    SHDRToSP(chroma_weight_l0_flag);
    SHDRToSP(chroma_weight_l1_flag);

    for (int i = 0; i <= slice_param.num_ref_idx_l0_active_minus1; ++i) {
      slice_param.luma_weight_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_weight[i];
      slice_param.luma_offset_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_offset[i];

      for (int j = 0; j < 2; ++j) {
        slice_param.chroma_weight_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_weight[i][j];
        slice_param.chroma_offset_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_offset[i][j];
      }
    }

    if (slice_hdr->IsBSlice()) {
      for (int i = 0; i <= slice_param.num_ref_idx_l1_active_minus1; ++i) {
        slice_param.luma_weight_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_weight[i];
        slice_param.luma_offset_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_offset[i];

        for (int j = 0; j < 2; ++j) {
          slice_param.chroma_weight_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_weight[i][j];
          slice_param.chroma_offset_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_offset[i][j];
        }
      }
    }
  }

  static_assert(
      std::size(slice_param.RefPicList0) == std::size(slice_param.RefPicList1),
      "Invalid RefPicList sizes");

  for (size_t i = 0; i < std::size(slice_param.RefPicList0); ++i) {
    InitVAPicture(&slice_param.RefPicList0[i]);
    InitVAPicture(&slice_param.RefPicList1[i]);
  }

  for (size_t i = 0;
       i < ref_pic_list0.size() && i < std::size(slice_param.RefPicList0);
       ++i) {
    if (ref_pic_list0[i])
      FillVAPicture(&slice_param.RefPicList0[i], ref_pic_list0[i]);
  }
  for (size_t i = 0;
       i < ref_pic_list1.size() && i < std::size(slice_param.RefPicList1);
       ++i) {
    if (ref_pic_list1[i])
      FillVAPicture(&slice_param.RefPicList1[i], ref_pic_list1[i]);
  }

  pic->slice_data_buffers.emplace_back(std::make_unique<uint8_t[]>(size));
  memcpy(pic->slice_data_buffers.back().get(), data, size);

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(
      va_device_->display(), va_context_->id(), VASliceParameterBufferType,
      sizeof(slice_param), 1, &slice_param, &buffer_id);
  VA_LOG_ASSERT(va_res, "vaCreateBuffer");
  buffers_.push_back(buffer_id);
  va_res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                          VASliceDataBufferType, size, 1,
                          pic->slice_data_buffers.back().get(), &buffer_id);
  VA_LOG_ASSERT(va_res, "vaCreateBuffer");
  buffers_.push_back(buffer_id);
}

void H264VaapiWrapper::SubmitDecode(scoped_refptr<H264Picture> pic) {
  CHECK(gfx::Rect(pic->surface->size()).Contains(pic->visible_rect));

  VAStatus va_res = vaBeginPicture(va_device_->display(), va_context_->id(),
                                   pic->surface->id());
  VA_LOG_ASSERT(va_res, "vaBeginPicture");

  va_res = vaRenderPicture(va_device_->display(), va_context_->id(),
                           buffers_.data(), buffers_.size());
  VA_LOG_ASSERT(va_res, "vaRenderPicture");

  va_res = vaEndPicture(va_device_->display(), va_context_->id());
  VA_LOG_ASSERT(va_res, "vaEndPicture");

  for (auto id : buffers_) {
    vaDestroyBuffer(va_device_->display(), id);
  }
  buffers_.clear();
}

}  // namespace media::vaapi_test
