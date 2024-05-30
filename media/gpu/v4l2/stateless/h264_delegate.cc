// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/h264_delegate.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include "media/gpu/macros.h"
#include "media/gpu/v4l2/stateless/stateless_decode_surface_handler.h"

namespace media {
namespace {
class StatelessH264Picture : public H264Picture {
 public:
  explicit StatelessH264Picture(
      scoped_refptr<StatelessDecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  StatelessH264Picture(const StatelessH264Picture&) = delete;
  StatelessH264Picture& operator=(const StatelessH264Picture&) = delete;

  const scoped_refptr<StatelessDecodeSurface> dec_surface() const {
    return dec_surface_;
  }

 private:
  ~StatelessH264Picture() override {}

  scoped_refptr<StatelessDecodeSurface> dec_surface_;
};

scoped_refptr<StatelessDecodeSurface> H264PictureToStatelessDecodeSurface(
    H264Picture* pic) {
  CHECK(pic);
  StatelessH264Picture* stateless_h264_picture =
      static_cast<StatelessH264Picture*>(pic);

  return stateless_h264_picture->dec_surface();
}

constexpr uint8_t zigzag_4x4[] = {
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

constexpr uint8_t zigzag_8x8[] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

}  // namespace

struct H264DelegateContext {
  struct v4l2_ctrl_h264_sps v4l2_sps;
  struct v4l2_ctrl_h264_pps v4l2_pps;
  struct v4l2_ctrl_h264_scaling_matrix v4l2_scaling_matrix;
  struct v4l2_ctrl_h264_decode_params v4l2_decode_param;
  std::vector<uint8_t> slice_data;
};

std::vector<scoped_refptr<StatelessDecodeSurface>>
H264Delegate::GetRefSurfacesFromDPB(const H264DPB& dpb) {
  std::vector<scoped_refptr<StatelessDecodeSurface>> ref_surfaces;

  memset(ctx_->v4l2_decode_param.dpb, 0, sizeof(ctx_->v4l2_decode_param.dpb));
  size_t i = 0;
  for (const auto& pic : dpb) {
    if (i >= std::size(ctx_->v4l2_decode_param.dpb)) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    int index = VIDEO_MAX_FRAME;
    if (!pic->nonexisting) {
      scoped_refptr<StatelessDecodeSurface> dec_surface =
          H264PictureToStatelessDecodeSurface(pic.get());
      index = dec_surface->GetReferenceTimestamp();
      ref_surfaces.push_back(dec_surface);
    }

    struct v4l2_h264_dpb_entry& entry = ctx_->v4l2_decode_param.dpb[i++];
    entry.reference_ts = index;
    if (pic->long_term) {
      entry.frame_num = pic->long_term_pic_num;
      entry.pic_num = pic->long_term_frame_idx;
    } else {
      entry.frame_num = pic->frame_num;
      entry.pic_num = pic->pic_num;
    }

    DCHECK_EQ(pic->field, H264Picture::FIELD_NONE)
        << "Interlacing not supported";
    entry.fields = V4L2_H264_FRAME_REF;

    entry.top_field_order_cnt = pic->top_field_order_cnt;
    entry.bottom_field_order_cnt = pic->bottom_field_order_cnt;
    entry.flags = V4L2_H264_DPB_ENTRY_FLAG_VALID |
                  (pic->ref ? V4L2_H264_DPB_ENTRY_FLAG_ACTIVE : 0) |
                  (pic->long_term ? V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM : 0);
  }

  return ref_surfaces;
}

H264Delegate::H264Delegate(StatelessDecodeSurfaceHandler* surface_handler)
    : surface_handler_(surface_handler),
      ctx_(std::make_unique<H264DelegateContext>()) {
  DCHECK(surface_handler_);
}

H264Delegate::~H264Delegate() = default;

scoped_refptr<H264Picture> H264Delegate::CreateH264Picture() {
  scoped_refptr<StatelessDecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface) {
    return nullptr;
  }

  return new StatelessH264Picture(dec_surface);
}

H264Delegate::Status H264Delegate::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    scoped_refptr<H264Picture> pic) {
  memset(&ctx_->v4l2_sps, 0, sizeof(ctx_->v4l2_sps));
  ctx_->v4l2_sps.constraint_set_flags =
      (sps->constraint_set0_flag ? V4L2_H264_SPS_CONSTRAINT_SET0_FLAG : 0) |
      (sps->constraint_set1_flag ? V4L2_H264_SPS_CONSTRAINT_SET1_FLAG : 0) |
      (sps->constraint_set2_flag ? V4L2_H264_SPS_CONSTRAINT_SET2_FLAG : 0) |
      (sps->constraint_set3_flag ? V4L2_H264_SPS_CONSTRAINT_SET3_FLAG : 0) |
      (sps->constraint_set4_flag ? V4L2_H264_SPS_CONSTRAINT_SET4_FLAG : 0) |
      (sps->constraint_set5_flag ? V4L2_H264_SPS_CONSTRAINT_SET5_FLAG : 0);
#define SPS_TO_V4L2SPS(a) ctx_->v4l2_sps.a = sps->a
  SPS_TO_V4L2SPS(profile_idc);
  SPS_TO_V4L2SPS(level_idc);
  SPS_TO_V4L2SPS(seq_parameter_set_id);
  SPS_TO_V4L2SPS(chroma_format_idc);
  SPS_TO_V4L2SPS(bit_depth_luma_minus8);
  SPS_TO_V4L2SPS(bit_depth_chroma_minus8);
  SPS_TO_V4L2SPS(log2_max_frame_num_minus4);
  SPS_TO_V4L2SPS(pic_order_cnt_type);
  SPS_TO_V4L2SPS(log2_max_pic_order_cnt_lsb_minus4);
  SPS_TO_V4L2SPS(offset_for_non_ref_pic);
  SPS_TO_V4L2SPS(offset_for_top_to_bottom_field);
  SPS_TO_V4L2SPS(num_ref_frames_in_pic_order_cnt_cycle);

  static_assert(std::extent<decltype(ctx_->v4l2_sps.offset_for_ref_frame)>() ==
                    std::extent<decltype(sps->offset_for_ref_frame)>(),
                "offset_for_ref_frame arrays must be same size");
  for (size_t i = 0; i < std::size(ctx_->v4l2_sps.offset_for_ref_frame); ++i) {
    ctx_->v4l2_sps.offset_for_ref_frame[i] = sps->offset_for_ref_frame[i];
  }
  SPS_TO_V4L2SPS(max_num_ref_frames);
  SPS_TO_V4L2SPS(pic_width_in_mbs_minus1);
  SPS_TO_V4L2SPS(pic_height_in_map_units_minus1);
#undef SPS_TO_V4L2SPS

#define SET_V4L2_SPS_FLAG_IF(cond, flag) \
  ctx_->v4l2_sps.flags |= ((sps->cond) ? (flag) : 0)
  SET_V4L2_SPS_FLAG_IF(separate_colour_plane_flag,
                       V4L2_H264_SPS_FLAG_SEPARATE_COLOUR_PLANE);
  SET_V4L2_SPS_FLAG_IF(qpprime_y_zero_transform_bypass_flag,
                       V4L2_H264_SPS_FLAG_QPPRIME_Y_ZERO_TRANSFORM_BYPASS);
  SET_V4L2_SPS_FLAG_IF(delta_pic_order_always_zero_flag,
                       V4L2_H264_SPS_FLAG_DELTA_PIC_ORDER_ALWAYS_ZERO);
  SET_V4L2_SPS_FLAG_IF(gaps_in_frame_num_value_allowed_flag,
                       V4L2_H264_SPS_FLAG_GAPS_IN_FRAME_NUM_VALUE_ALLOWED);
  SET_V4L2_SPS_FLAG_IF(frame_mbs_only_flag, V4L2_H264_SPS_FLAG_FRAME_MBS_ONLY);
  SET_V4L2_SPS_FLAG_IF(mb_adaptive_frame_field_flag,
                       V4L2_H264_SPS_FLAG_MB_ADAPTIVE_FRAME_FIELD);
  SET_V4L2_SPS_FLAG_IF(direct_8x8_inference_flag,
                       V4L2_H264_SPS_FLAG_DIRECT_8X8_INFERENCE);
#undef SET_V4L2_SPS_FLAG_IF

  memset(&ctx_->v4l2_pps, 0, sizeof(ctx_->v4l2_pps));
#define PPS_TO_V4L2PPS(a) ctx_->v4l2_pps.a = pps->a
  PPS_TO_V4L2PPS(pic_parameter_set_id);
  PPS_TO_V4L2PPS(seq_parameter_set_id);
  PPS_TO_V4L2PPS(num_slice_groups_minus1);
  PPS_TO_V4L2PPS(num_ref_idx_l0_default_active_minus1);
  PPS_TO_V4L2PPS(num_ref_idx_l1_default_active_minus1);
  PPS_TO_V4L2PPS(weighted_bipred_idc);
  PPS_TO_V4L2PPS(pic_init_qp_minus26);
  PPS_TO_V4L2PPS(pic_init_qs_minus26);
  PPS_TO_V4L2PPS(chroma_qp_index_offset);
  PPS_TO_V4L2PPS(second_chroma_qp_index_offset);
#undef PPS_TO_V4L2PPS

#define SET_V4L2_PPS_FLAG_IF(cond, flag) \
  ctx_->v4l2_pps.flags |= ((pps->cond) ? (flag) : 0)
  SET_V4L2_PPS_FLAG_IF(entropy_coding_mode_flag,
                       V4L2_H264_PPS_FLAG_ENTROPY_CODING_MODE);
  SET_V4L2_PPS_FLAG_IF(
      bottom_field_pic_order_in_frame_present_flag,
      V4L2_H264_PPS_FLAG_BOTTOM_FIELD_PIC_ORDER_IN_FRAME_PRESENT);
  SET_V4L2_PPS_FLAG_IF(weighted_pred_flag, V4L2_H264_PPS_FLAG_WEIGHTED_PRED);
  SET_V4L2_PPS_FLAG_IF(deblocking_filter_control_present_flag,
                       V4L2_H264_PPS_FLAG_DEBLOCKING_FILTER_CONTROL_PRESENT);
  SET_V4L2_PPS_FLAG_IF(constrained_intra_pred_flag,
                       V4L2_H264_PPS_FLAG_CONSTRAINED_INTRA_PRED);
  SET_V4L2_PPS_FLAG_IF(redundant_pic_cnt_present_flag,
                       V4L2_H264_PPS_FLAG_REDUNDANT_PIC_CNT_PRESENT);
  SET_V4L2_PPS_FLAG_IF(transform_8x8_mode_flag,
                       V4L2_H264_PPS_FLAG_TRANSFORM_8X8_MODE);
  SET_V4L2_PPS_FLAG_IF(pic_scaling_matrix_present_flag,
                       V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT);
#undef SET_V4L2_PPS_FLAG_IF

  memset(&ctx_->v4l2_scaling_matrix, 0, sizeof(ctx_->v4l2_scaling_matrix));

  static_assert(
      std::extent<decltype(ctx_->v4l2_scaling_matrix.scaling_list_4x4)>() <=
              std::extent<decltype(pps->scaling_list4x4)>() &&
          std::extent<
              decltype(ctx_->v4l2_scaling_matrix.scaling_list_4x4[0])>() <=
              std::extent<decltype(pps->scaling_list4x4[0])>() &&
          std::extent<decltype(ctx_->v4l2_scaling_matrix.scaling_list_8x8)>() <=
              std::extent<decltype(pps->scaling_list8x8)>() &&
          std::extent<
              decltype(ctx_->v4l2_scaling_matrix.scaling_list_8x8[0])>() <=
              std::extent<decltype(pps->scaling_list8x8[0])>(),
      "PPS scaling_lists must be of correct size");
  static_assert(
      std::extent<decltype(ctx_->v4l2_scaling_matrix.scaling_list_4x4)>() <=
              std::extent<decltype(sps->scaling_list4x4)>() &&
          std::extent<
              decltype(ctx_->v4l2_scaling_matrix.scaling_list_4x4[0])>() <=
              std::extent<decltype(sps->scaling_list4x4[0])>() &&
          std::extent<decltype(ctx_->v4l2_scaling_matrix.scaling_list_8x8)>() <=
              std::extent<decltype(sps->scaling_list8x8)>() &&
          std::extent<
              decltype(ctx_->v4l2_scaling_matrix.scaling_list_8x8[0])>() <=
              std::extent<decltype(sps->scaling_list8x8[0])>(),
      "SPS scaling_lists must be of correct size");

  const auto* scaling_list4x4 = &sps->scaling_list4x4[0];
  const auto* scaling_list8x8 = &sps->scaling_list8x8[0];
  if (pps->pic_scaling_matrix_present_flag) {
    scaling_list4x4 = &pps->scaling_list4x4[0];
    scaling_list8x8 = &pps->scaling_list8x8[0];
  }

  for (size_t i = 0; i < std::size(ctx_->v4l2_scaling_matrix.scaling_list_4x4);
       ++i) {
    for (size_t j = 0;
         j < std::size(ctx_->v4l2_scaling_matrix.scaling_list_4x4[i]); ++j) {
      // Parser uses source (zigzag) order, while V4L2 API requires raster
      // order.
      static_assert(
          std::extent<decltype(ctx_->v4l2_scaling_matrix.scaling_list_4x4),
                      1>() == std::extent<decltype(zigzag_4x4)>());
      ctx_->v4l2_scaling_matrix.scaling_list_4x4[i][zigzag_4x4[j]] =
          scaling_list4x4[i][j];
    }
  }
  for (size_t i = 0; i < std::size(ctx_->v4l2_scaling_matrix.scaling_list_8x8);
       ++i) {
    for (size_t j = 0;
         j < std::size(ctx_->v4l2_scaling_matrix.scaling_list_8x8[i]); ++j) {
      static_assert(
          std::extent<decltype(ctx_->v4l2_scaling_matrix.scaling_list_8x8),
                      1>() == std::extent<decltype(zigzag_8x8)>());
      ctx_->v4l2_scaling_matrix.scaling_list_8x8[i][zigzag_8x8[j]] =
          scaling_list8x8[i][j];
    }
  }

  scoped_refptr<StatelessDecodeSurface> dec_surface =
      H264PictureToStatelessDecodeSurface(pic.get());

  auto references = GetRefSurfacesFromDPB(dpb);
  dec_surface->SetReferenceSurfaces(references);

  return H264Delegate::Status::kOk;
}

H264Delegate::Status H264Delegate::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
#define SHDR_TO_V4L2DPARM(a) ctx_->v4l2_decode_param.a = slice_hdr->a
  SHDR_TO_V4L2DPARM(frame_num);
  SHDR_TO_V4L2DPARM(idr_pic_id);
  SHDR_TO_V4L2DPARM(pic_order_cnt_lsb);
  SHDR_TO_V4L2DPARM(delta_pic_order_cnt_bottom);
  SHDR_TO_V4L2DPARM(delta_pic_order_cnt0);
  SHDR_TO_V4L2DPARM(delta_pic_order_cnt1);
  SHDR_TO_V4L2DPARM(dec_ref_pic_marking_bit_size);
  SHDR_TO_V4L2DPARM(pic_order_cnt_bit_size);
#undef SHDR_TO_V4L2DPARM

  scoped_refptr<StatelessDecodeSurface> dec_surface =
      H264PictureToStatelessDecodeSurface(pic.get());
  ctx_->v4l2_decode_param.nal_ref_idc = slice_hdr->nal_ref_idc;

  std::vector<uint8_t> slice_data(
      sizeof(V4L2_STATELESS_H264_START_CODE_ANNEX_B) - 1);
  slice_data[2] = V4L2_STATELESS_H264_START_CODE_ANNEX_B;
  slice_data.insert(slice_data.end(), data, data + size);
  ctx_->slice_data.insert(ctx_->slice_data.end(), slice_data.begin(),
                          slice_data.end());

  return H264Delegate::Status::kOk;
}

H264Delegate::Status H264Delegate::SubmitDecode(
    scoped_refptr<H264Picture> pic) {
  ctx_->v4l2_decode_param.flags = 0;
  if (pic->idr) {
    ctx_->v4l2_decode_param.flags |= V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC;
  }
  ctx_->v4l2_decode_param.top_field_order_cnt = pic->top_field_order_cnt;
  ctx_->v4l2_decode_param.bottom_field_order_cnt = pic->bottom_field_order_cnt;

  std::vector<struct v4l2_ext_control> ext_ctrls = {
      {.id = V4L2_CID_STATELESS_H264_SPS,
       .size = sizeof(ctx_->v4l2_sps),
       .ptr = &ctx_->v4l2_sps},
      {.id = V4L2_CID_STATELESS_H264_PPS,
       .size = sizeof(ctx_->v4l2_pps),
       .ptr = &ctx_->v4l2_pps},
      {.id = V4L2_CID_STATELESS_H264_SCALING_MATRIX,
       .size = sizeof(ctx_->v4l2_scaling_matrix),
       .ptr = &ctx_->v4l2_scaling_matrix},
      {.id = V4L2_CID_STATELESS_H264_DECODE_PARAMS,
       .size = sizeof(ctx_->v4l2_decode_param),
       .ptr = &ctx_->v4l2_decode_param},
      {.id = V4L2_CID_STATELESS_H264_DECODE_MODE,
       .value = V4L2_STATELESS_H264_DECODE_MODE_FRAME_BASED},
  };

  const __u32 ext_ctrls_size = base::checked_cast<__u32>(ext_ctrls.size());
  struct v4l2_ext_controls ctrls = {.count = ext_ctrls_size,
                                    .controls = ext_ctrls.data()};

  StatelessH264Picture* stateless_h264_picture =
      static_cast<StatelessH264Picture*>(pic.get());
  if (!stateless_h264_picture) {
    return H264Delegate::Status::kFail;
  }
  if (!surface_handler_->SubmitFrame(&ctrls, &ctx_->slice_data[0],
                                     ctx_->slice_data.size(),
                                     stateless_h264_picture->dec_surface())) {
    return H264Delegate::Status::kFail;
  }

  ctx_->slice_data.clear();

  return H264Delegate::Status::kOk;
}

bool H264Delegate::OutputPicture(scoped_refptr<H264Picture> pic) {
  DVLOGF(4);
  surface_handler_->SurfaceReady(H264PictureToStatelessDecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 pic->get_colorspace());
  return true;
}

void H264Delegate::Reset() {
  memset(ctx_->v4l2_decode_param.dpb, 0, sizeof(ctx_->v4l2_decode_param.dpb));
}

}  // namespace media
