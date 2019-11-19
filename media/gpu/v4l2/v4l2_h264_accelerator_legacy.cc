// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_h264_accelerator_legacy.h"

#include <linux/media/h264-ctrls-legacy.h>
#include <linux/videodev2.h>
#include <type_traits>

#include "base/logging.h"
#include "base/stl_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

// This struct contains the kernel-specific parts of the H264 acceleration,
// that we don't want to expose in the .h file since they may differ from
// upstream.
struct V4L2LegacyH264AcceleratorPrivate {
  // TODO(posciak): This should be queried from hardware once supported.
  static constexpr size_t kMaxSlices = 16;

  struct v4l2_ctrl_h264_slice_param v4l2_slice_params[kMaxSlices];
  struct v4l2_ctrl_h264_decode_param v4l2_decode_param;
};

class V4L2H264Picture : public H264Picture {
 public:
  explicit V4L2H264Picture(const scoped_refptr<V4L2DecodeSurface>& dec_surface)
      : dec_surface_(dec_surface) {}

  V4L2H264Picture* AsV4L2H264Picture() override { return this; }
  scoped_refptr<V4L2DecodeSurface> dec_surface() { return dec_surface_; }

 private:
  ~V4L2H264Picture() override {}

  scoped_refptr<V4L2DecodeSurface> dec_surface_;

  DISALLOW_COPY_AND_ASSIGN(V4L2H264Picture);
};

V4L2LegacyH264Accelerator::V4L2LegacyH264Accelerator(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : num_slices_(0),
      surface_handler_(surface_handler),
      device_(device),
      priv_(std::make_unique<V4L2LegacyH264AcceleratorPrivate>()) {
  DCHECK(surface_handler_);
}

V4L2LegacyH264Accelerator::~V4L2LegacyH264Accelerator() {}

scoped_refptr<H264Picture> V4L2LegacyH264Accelerator::CreateH264Picture() {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface)
    return nullptr;

  return new V4L2H264Picture(dec_surface);
}

void V4L2LegacyH264Accelerator::H264PictureListToDPBIndicesList(
    const H264Picture::Vector& src_pic_list,
    uint8_t dst_list[kDPBIndicesListSize]) {
  size_t i;
  for (i = 0; i < src_pic_list.size() && i < kDPBIndicesListSize; ++i) {
    const scoped_refptr<H264Picture>& pic = src_pic_list[i];
    dst_list[i] = pic ? pic->dpb_position : VIDEO_MAX_FRAME;
  }

  while (i < kDPBIndicesListSize)
    dst_list[i++] = VIDEO_MAX_FRAME;
}

void V4L2LegacyH264Accelerator::H264DPBToV4L2DPB(
    const H264DPB& dpb,
    std::vector<scoped_refptr<V4L2DecodeSurface>>* ref_surfaces) {
  memset(priv_->v4l2_decode_param.dpb, 0, sizeof(priv_->v4l2_decode_param.dpb));
  size_t i = 0;
  for (const auto& pic : dpb) {
    if (i >= base::size(priv_->v4l2_decode_param.dpb)) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    int index = VIDEO_MAX_FRAME;
    if (!pic->nonexisting) {
      scoped_refptr<V4L2DecodeSurface> dec_surface =
          H264PictureToV4L2DecodeSurface(pic.get());
      index = dec_surface->GetReferenceID();
      ref_surfaces->push_back(dec_surface);
    }

    struct v4l2_h264_dpb_entry& entry = priv_->v4l2_decode_param.dpb[i++];
    entry.buf_index = index;
    entry.frame_num = pic->frame_num;
    entry.pic_num = pic->pic_num;
    entry.top_field_order_cnt = pic->top_field_order_cnt;
    entry.bottom_field_order_cnt = pic->bottom_field_order_cnt;
    entry.flags = (pic->ref ? V4L2_H264_DPB_ENTRY_FLAG_ACTIVE : 0) |
                  (pic->long_term ? V4L2_H264_DPB_ENTRY_FLAG_LONG_TERM : 0);
  }
}

H264Decoder::H264Accelerator::Status
V4L2LegacyH264Accelerator::SubmitFrameMetadata(
    const H264SPS* sps,
    const H264PPS* pps,
    const H264DPB& dpb,
    const H264Picture::Vector& ref_pic_listp0,
    const H264Picture::Vector& ref_pic_listb0,
    const H264Picture::Vector& ref_pic_listb1,
    scoped_refptr<H264Picture> pic) {
  struct v4l2_ext_control ctrl;
  std::vector<struct v4l2_ext_control> ctrls;

  struct v4l2_ctrl_h264_sps v4l2_sps;
  memset(&v4l2_sps, 0, sizeof(v4l2_sps));
  v4l2_sps.constraint_set_flags =
      (sps->constraint_set0_flag ? V4L2_H264_SPS_CONSTRAINT_SET0_FLAG : 0) |
      (sps->constraint_set1_flag ? V4L2_H264_SPS_CONSTRAINT_SET1_FLAG : 0) |
      (sps->constraint_set2_flag ? V4L2_H264_SPS_CONSTRAINT_SET2_FLAG : 0) |
      (sps->constraint_set3_flag ? V4L2_H264_SPS_CONSTRAINT_SET3_FLAG : 0) |
      (sps->constraint_set4_flag ? V4L2_H264_SPS_CONSTRAINT_SET4_FLAG : 0) |
      (sps->constraint_set5_flag ? V4L2_H264_SPS_CONSTRAINT_SET5_FLAG : 0);
#define SPS_TO_V4L2SPS(a) v4l2_sps.a = sps->a
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

  static_assert(std::extent<decltype(v4l2_sps.offset_for_ref_frame)>() ==
                    std::extent<decltype(sps->offset_for_ref_frame)>(),
                "offset_for_ref_frame arrays must be same size");
  for (size_t i = 0; i < base::size(v4l2_sps.offset_for_ref_frame); ++i)
    v4l2_sps.offset_for_ref_frame[i] = sps->offset_for_ref_frame[i];
  SPS_TO_V4L2SPS(max_num_ref_frames);
  SPS_TO_V4L2SPS(pic_width_in_mbs_minus1);
  SPS_TO_V4L2SPS(pic_height_in_map_units_minus1);
#undef SPS_TO_V4L2SPS

#define SET_V4L2_SPS_FLAG_IF(cond, flag) \
  v4l2_sps.flags |= ((sps->cond) ? (flag) : 0)
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
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_H264_SPS;
  ctrl.size = sizeof(v4l2_sps);
  ctrl.ptr = &v4l2_sps;
  ctrls.push_back(ctrl);

  struct v4l2_ctrl_h264_pps v4l2_pps;
  memset(&v4l2_pps, 0, sizeof(v4l2_pps));
#define PPS_TO_V4L2PPS(a) v4l2_pps.a = pps->a
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
  v4l2_pps.flags |= ((pps->cond) ? (flag) : 0)
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
                       V4L2_H264_PPS_FLAG_PIC_SCALING_MATRIX_PRESENT);
#undef SET_V4L2_PPS_FLAG_IF
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_H264_PPS;
  ctrl.size = sizeof(v4l2_pps);
  ctrl.ptr = &v4l2_pps;
  ctrls.push_back(ctrl);

  struct v4l2_ctrl_h264_scaling_matrix v4l2_scaling_matrix;
  memset(&v4l2_scaling_matrix, 0, sizeof(v4l2_scaling_matrix));

  static_assert(
      std::extent<decltype(v4l2_scaling_matrix.scaling_list_4x4)>() <=
              std::extent<decltype(pps->scaling_list4x4)>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_4x4[0])>() <=
              std::extent<decltype(pps->scaling_list4x4[0])>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_8x8)>() <=
              std::extent<decltype(pps->scaling_list8x8)>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_8x8[0])>() <=
              std::extent<decltype(pps->scaling_list8x8[0])>(),
      "scaling_lists must be of correct size");
  static_assert(
      std::extent<decltype(v4l2_scaling_matrix.scaling_list_4x4)>() <=
              std::extent<decltype(sps->scaling_list4x4)>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_4x4[0])>() <=
              std::extent<decltype(sps->scaling_list4x4[0])>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_8x8)>() <=
              std::extent<decltype(sps->scaling_list8x8)>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_8x8[0])>() <=
              std::extent<decltype(sps->scaling_list8x8[0])>(),
      "scaling_lists must be of correct size");

  const auto* scaling_list4x4 = &sps->scaling_list4x4[0];
  const auto* scaling_list8x8 = &sps->scaling_list8x8[0];
  if (pps->pic_scaling_matrix_present_flag) {
    scaling_list4x4 = &pps->scaling_list4x4[0];
    scaling_list8x8 = &pps->scaling_list8x8[0];
  }

  for (size_t i = 0; i < base::size(v4l2_scaling_matrix.scaling_list_4x4);
       ++i) {
    for (size_t j = 0; j < base::size(v4l2_scaling_matrix.scaling_list_4x4[i]);
         ++j) {
      v4l2_scaling_matrix.scaling_list_4x4[i][j] = scaling_list4x4[i][j];
    }
  }
  for (size_t i = 0; i < base::size(v4l2_scaling_matrix.scaling_list_8x8);
       ++i) {
    for (size_t j = 0; j < base::size(v4l2_scaling_matrix.scaling_list_8x8[i]);
         ++j) {
      v4l2_scaling_matrix.scaling_list_8x8[i][j] = scaling_list8x8[i][j];
    }
  }

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_H264_SCALING_MATRIX;
  ctrl.size = sizeof(v4l2_scaling_matrix);
  ctrl.ptr = &v4l2_scaling_matrix;
  ctrls.push_back(ctrl);

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H264PictureToV4L2DecodeSurface(pic.get());

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0];
  dec_surface->PrepareSetCtrls(&ext_ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return Status::kFail;
  }

  H264PictureListToDPBIndicesList(ref_pic_listp0,
                                  priv_->v4l2_decode_param.ref_pic_list_p0);
  H264PictureListToDPBIndicesList(ref_pic_listb0,
                                  priv_->v4l2_decode_param.ref_pic_list_b0);
  H264PictureListToDPBIndicesList(ref_pic_listb1,
                                  priv_->v4l2_decode_param.ref_pic_list_b1);

  std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces;
  H264DPBToV4L2DPB(dpb, &ref_surfaces);
  dec_surface->SetReferenceSurfaces(ref_surfaces);

  return Status::kOk;
}

H264Decoder::H264Accelerator::Status V4L2LegacyH264Accelerator::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
  if (num_slices_ == priv_->kMaxSlices) {
    VLOGF(1) << "Over limit of supported slices per frame";
    return Status::kFail;
  }

  struct v4l2_ctrl_h264_slice_param& v4l2_slice_param =
      priv_->v4l2_slice_params[num_slices_++];
  memset(&v4l2_slice_param, 0, sizeof(v4l2_slice_param));

  v4l2_slice_param.size = size;
#define SHDR_TO_V4L2SPARM(a) v4l2_slice_param.a = slice_hdr->a
  SHDR_TO_V4L2SPARM(header_bit_size);
  SHDR_TO_V4L2SPARM(first_mb_in_slice);
  SHDR_TO_V4L2SPARM(slice_type);
  SHDR_TO_V4L2SPARM(pic_parameter_set_id);
  SHDR_TO_V4L2SPARM(colour_plane_id);
  SHDR_TO_V4L2SPARM(frame_num);
  SHDR_TO_V4L2SPARM(idr_pic_id);
  SHDR_TO_V4L2SPARM(pic_order_cnt_lsb);
  SHDR_TO_V4L2SPARM(delta_pic_order_cnt_bottom);
  SHDR_TO_V4L2SPARM(delta_pic_order_cnt0);
  SHDR_TO_V4L2SPARM(delta_pic_order_cnt1);
  SHDR_TO_V4L2SPARM(redundant_pic_cnt);
  SHDR_TO_V4L2SPARM(dec_ref_pic_marking_bit_size);
  SHDR_TO_V4L2SPARM(cabac_init_idc);
  SHDR_TO_V4L2SPARM(slice_qp_delta);
  SHDR_TO_V4L2SPARM(slice_qs_delta);
  SHDR_TO_V4L2SPARM(disable_deblocking_filter_idc);
  SHDR_TO_V4L2SPARM(slice_alpha_c0_offset_div2);
  SHDR_TO_V4L2SPARM(slice_beta_offset_div2);
  SHDR_TO_V4L2SPARM(num_ref_idx_l0_active_minus1);
  SHDR_TO_V4L2SPARM(num_ref_idx_l1_active_minus1);
  SHDR_TO_V4L2SPARM(pic_order_cnt_bit_size);
#undef SHDR_TO_V4L2SPARM

#define SET_V4L2_SPARM_FLAG_IF(cond, flag) \
  v4l2_slice_param.flags |= ((slice_hdr->cond) ? (flag) : 0)
  SET_V4L2_SPARM_FLAG_IF(field_pic_flag, V4L2_SLICE_FLAG_FIELD_PIC);
  SET_V4L2_SPARM_FLAG_IF(bottom_field_flag, V4L2_SLICE_FLAG_BOTTOM_FIELD);
  SET_V4L2_SPARM_FLAG_IF(direct_spatial_mv_pred_flag,
                         V4L2_SLICE_FLAG_DIRECT_SPATIAL_MV_PRED);
  SET_V4L2_SPARM_FLAG_IF(sp_for_switch_flag, V4L2_SLICE_FLAG_SP_FOR_SWITCH);
#undef SET_V4L2_SPARM_FLAG_IF

  struct v4l2_h264_pred_weight_table* pred_weight_table =
      &v4l2_slice_param.pred_weight_table;

  if (((slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) &&
       pps->weighted_pred_flag) ||
      (slice_hdr->IsBSlice() && pps->weighted_bipred_idc == 1)) {
    pred_weight_table->luma_log2_weight_denom =
        slice_hdr->luma_log2_weight_denom;
    pred_weight_table->chroma_log2_weight_denom =
        slice_hdr->chroma_log2_weight_denom;

    struct v4l2_h264_weight_factors* factorsl0 =
        &pred_weight_table->weight_factors[0];

    for (int i = 0; i < 32; ++i) {
      factorsl0->luma_weight[i] =
          slice_hdr->pred_weight_table_l0.luma_weight[i];
      factorsl0->luma_offset[i] =
          slice_hdr->pred_weight_table_l0.luma_offset[i];

      for (int j = 0; j < 2; ++j) {
        factorsl0->chroma_weight[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_weight[i][j];
        factorsl0->chroma_offset[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_offset[i][j];
      }
    }

    if (slice_hdr->IsBSlice()) {
      struct v4l2_h264_weight_factors* factorsl1 =
          &pred_weight_table->weight_factors[1];

      for (int i = 0; i < 32; ++i) {
        factorsl1->luma_weight[i] =
            slice_hdr->pred_weight_table_l1.luma_weight[i];
        factorsl1->luma_offset[i] =
            slice_hdr->pred_weight_table_l1.luma_offset[i];

        for (int j = 0; j < 2; ++j) {
          factorsl1->chroma_weight[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_weight[i][j];
          factorsl1->chroma_offset[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_offset[i][j];
        }
      }
    }
  }

  H264PictureListToDPBIndicesList(ref_pic_list0,
                                  v4l2_slice_param.ref_pic_list0);
  H264PictureListToDPBIndicesList(ref_pic_list1,
                                  v4l2_slice_param.ref_pic_list1);

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H264PictureToV4L2DecodeSurface(pic.get());

  priv_->v4l2_decode_param.nal_ref_idc = slice_hdr->nal_ref_idc;

  // TODO(posciak): Don't add start code back here, but have it passed from
  // the parser.
  size_t data_copy_size = size + 3;
  std::unique_ptr<uint8_t[]> data_copy(new uint8_t[data_copy_size]);
  memset(data_copy.get(), 0, data_copy_size);
  data_copy[2] = 0x01;
  memcpy(data_copy.get() + 3, data, size);
  return surface_handler_->SubmitSlice(dec_surface, data_copy.get(),
                                       data_copy_size)
             ? Status::kOk
             : Status::kFail;
}

H264Decoder::H264Accelerator::Status V4L2LegacyH264Accelerator::SubmitDecode(
    scoped_refptr<H264Picture> pic) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H264PictureToV4L2DecodeSurface(pic.get());

  priv_->v4l2_decode_param.num_slices = num_slices_;
  priv_->v4l2_decode_param.idr_pic_flag = pic->idr;
  priv_->v4l2_decode_param.top_field_order_cnt = pic->top_field_order_cnt;
  priv_->v4l2_decode_param.bottom_field_order_cnt = pic->bottom_field_order_cnt;

  struct v4l2_ext_control ctrl;
  std::vector<struct v4l2_ext_control> ctrls;

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_H264_SLICE_PARAM;
  ctrl.size = sizeof(priv_->v4l2_slice_params);
  ctrl.ptr = priv_->v4l2_slice_params;
  ctrls.push_back(ctrl);

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_H264_DECODE_PARAM;
  ctrl.size = sizeof(priv_->v4l2_decode_param);
  ctrl.ptr = &priv_->v4l2_decode_param;
  ctrls.push_back(ctrl);

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0];
  dec_surface->PrepareSetCtrls(&ext_ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return Status::kFail;
  }

  Reset();

  DVLOGF(4) << "Submitting decode for surface: " << dec_surface->ToString();
  surface_handler_->DecodeSurface(dec_surface);
  return Status::kOk;
}

bool V4L2LegacyH264Accelerator::OutputPicture(scoped_refptr<H264Picture> pic) {
  // TODO(crbug.com/647725): Insert correct color space.
  surface_handler_->SurfaceReady(H264PictureToV4L2DecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 VideoColorSpace());
  return true;
}

void V4L2LegacyH264Accelerator::Reset() {
  num_slices_ = 0;
  memset(&priv_->v4l2_decode_param, 0, sizeof(priv_->v4l2_decode_param));
  memset(&priv_->v4l2_slice_params, 0, sizeof(priv_->v4l2_slice_params));
}

scoped_refptr<V4L2DecodeSurface>
V4L2LegacyH264Accelerator::H264PictureToV4L2DecodeSurface(H264Picture* pic) {
  V4L2H264Picture* v4l2_pic = pic->AsV4L2H264Picture();
  CHECK(v4l2_pic);
  return v4l2_pic->dec_surface();
}

}  // namespace media
