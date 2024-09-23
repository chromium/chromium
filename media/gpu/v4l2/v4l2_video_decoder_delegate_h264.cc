// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_delegate_h264.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include <algorithm>
#include <type_traits>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_device.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// gn check does not account for BUILDFLAG(), so including this header will
// make gn check fail for builds other than ash-chrome. See gn help nogncheck
// for more information.
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"  // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace media {

namespace {

constexpr uint8_t zigzag_4x4[] = {
    0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15,
};

constexpr uint8_t zigzag_8x8[] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

}  // namespace

// This struct contains the kernel-specific parts of the H264 acceleration,
// that we don't want to expose in the .h file since they may differ from
// upstream.
struct V4L2VideoDecoderDelegateH264Private {
  struct v4l2_ctrl_h264_decode_params v4l2_decode_param;
};

class V4L2H264Picture : public H264Picture {
 public:
  explicit V4L2H264Picture(scoped_refptr<V4L2DecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  V4L2H264Picture(const V4L2H264Picture&) = delete;
  V4L2H264Picture& operator=(const V4L2H264Picture&) = delete;

  V4L2H264Picture* AsV4L2H264Picture() override { return this; }
  scoped_refptr<V4L2DecodeSurface> dec_surface() { return dec_surface_; }

 private:
  ~V4L2H264Picture() override {}

  scoped_refptr<V4L2DecodeSurface> dec_surface_;
};

// Structure used when we parse encrypted slice headers in secure buffers. The
// same structure exists in the secure world code. This is all the fields we
// need to satisfy the V4L2 interface and the needs of the H264Decoder.
typedef struct CencV1SliceParameterBufferH264 {
  uint8_t nal_ref_idc;
  uint8_t idr_pic_flag;
  uint8_t slice_type;
  uint8_t field_pic_flag;
  uint32_t frame_num;
  uint32_t idr_pic_id;
  uint32_t pic_order_cnt_lsb;
  int32_t delta_pic_order_cnt_bottom;
  int32_t delta_pic_order_cnt0;
  int32_t delta_pic_order_cnt1;
  union {
    struct {
      uint32_t no_output_of_prior_pics_flag : 1;
      uint32_t long_term_reference_flag : 1;
      uint32_t adaptive_ref_pic_marking_mode_flag : 1;
      uint32_t dec_ref_pic_marking_count : 8;
      uint32_t reserved : 21;
    } bits;
    uint32_t value;
  } ref_pic_fields;
  uint8_t memory_management_control_operation[32];
  int32_t difference_of_pic_nums_minus1[32];
  int32_t long_term_pic_num[32];
  int32_t max_long_term_frame_idx_plus1[32];
  int32_t long_term_frame_idx[32];
  uint32_t dec_ref_pic_marking_bit_size;
  uint32_t pic_order_cnt_bit_size;
} CencV1SliceParameterBufferH264;

V4L2VideoDecoderDelegateH264::V4L2VideoDecoderDelegateH264(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device,
    CdmContext* cdm_context)
    : surface_handler_(surface_handler),
      device_(device),
      cdm_context_(cdm_context),
      priv_(std::make_unique<V4L2VideoDecoderDelegateH264Private>()) {
  DCHECK(surface_handler_);
}

V4L2VideoDecoderDelegateH264::~V4L2VideoDecoderDelegateH264() {}

scoped_refptr<H264Picture> V4L2VideoDecoderDelegateH264::CreateH264Picture() {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface) {
    return nullptr;
  }

  return new V4L2H264Picture(dec_surface);
}

scoped_refptr<H264Picture>
V4L2VideoDecoderDelegateH264::CreateH264PictureSecure(uint64_t secure_handle) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSecureSurface(secure_handle);
  if (!dec_surface)
    return nullptr;

  return new V4L2H264Picture(dec_surface);
}

void V4L2VideoDecoderDelegateH264::ProcessSPS(
    const H264SPS* sps,
    base::span<const uint8_t> sps_nalu_data) {
  if (cdm_context_) {
    cencv1_stream_data_.log2_max_frame_num_minus4 =
        sps->log2_max_frame_num_minus4;
    cencv1_stream_data_.log2_max_pic_order_cnt_lsb_minus4 =
        sps->log2_max_pic_order_cnt_lsb_minus4;
    cencv1_stream_data_.pic_order_cnt_type = sps->pic_order_cnt_type;
    cencv1_stream_data_.chroma_array_type = sps->chroma_array_type;
    cencv1_stream_data_.frame_mbs_only_flag = sps->frame_mbs_only_flag;
    cencv1_stream_data_.delta_pic_order_always_zero_flag =
        sps->delta_pic_order_always_zero_flag;
  }
}

void V4L2VideoDecoderDelegateH264::ProcessPPS(
    const H264PPS* pps,
    base::span<const uint8_t> pps_nalu_data) {
  if (cdm_context_) {
    cencv1_stream_data_.num_ref_idx_l0_default_active_minus1 =
        pps->num_ref_idx_l0_default_active_minus1;
    cencv1_stream_data_.num_ref_idx_l1_default_active_minus1 =
        pps->num_ref_idx_l1_default_active_minus1;
    cencv1_stream_data_.weighted_bipred_idc = pps->weighted_bipred_idc;
    cencv1_stream_data_.bottom_field_pic_order_in_frame_present_flag =
        pps->bottom_field_pic_order_in_frame_present_flag;
    cencv1_stream_data_.redundant_pic_cnt_present_flag =
        pps->redundant_pic_cnt_present_flag;
    cencv1_stream_data_.weighted_pred_flag = pps->weighted_pred_flag;
  }
}

std::vector<scoped_refptr<V4L2DecodeSurface>>
V4L2VideoDecoderDelegateH264::H264DPBToV4L2DPB(const H264DPB& dpb) {
  std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces;

  memset(priv_->v4l2_decode_param.dpb, 0, sizeof(priv_->v4l2_decode_param.dpb));
  size_t i = 0;
  for (const auto& pic : dpb) {
    if (i >= std::size(priv_->v4l2_decode_param.dpb)) {
      VLOGF(1) << "Invalid DPB size";
      break;
    }

    int index = VIDEO_MAX_FRAME;
    if (!pic->nonexisting) {
      scoped_refptr<V4L2DecodeSurface> dec_surface =
          H264PictureToV4L2DecodeSurface(pic.get());
      index = dec_surface->GetReferenceID();
      ref_surfaces.push_back(dec_surface);
    }

    struct v4l2_h264_dpb_entry& entry = priv_->v4l2_decode_param.dpb[i++];
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

H264Decoder::H264Accelerator::Status
V4L2VideoDecoderDelegateH264::SubmitFrameMetadata(
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
  for (size_t i = 0; i < std::size(v4l2_sps.offset_for_ref_frame); ++i) {
    v4l2_sps.offset_for_ref_frame[i] = sps->offset_for_ref_frame[i];
  }
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
  ctrl.id = V4L2_CID_STATELESS_H264_SPS;
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
                       V4L2_H264_PPS_FLAG_SCALING_MATRIX_PRESENT);
#undef SET_V4L2_PPS_FLAG_IF
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_H264_PPS;
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
      "PPS scaling_lists must be of correct size");
  static_assert(
      std::extent<decltype(v4l2_scaling_matrix.scaling_list_4x4)>() <=
              std::extent<decltype(sps->scaling_list4x4)>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_4x4[0])>() <=
              std::extent<decltype(sps->scaling_list4x4[0])>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_8x8)>() <=
              std::extent<decltype(sps->scaling_list8x8)>() &&
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_8x8[0])>() <=
              std::extent<decltype(sps->scaling_list8x8[0])>(),
      "SPS scaling_lists must be of correct size");

  const auto* scaling_list4x4 = &sps->scaling_list4x4[0];
  const auto* scaling_list8x8 = &sps->scaling_list8x8[0];
  if (pps->pic_scaling_matrix_present_flag) {
    scaling_list4x4 = &pps->scaling_list4x4[0];
    scaling_list8x8 = &pps->scaling_list8x8[0];
  }

  for (size_t i = 0; i < std::size(v4l2_scaling_matrix.scaling_list_4x4); ++i) {
    for (size_t j = 0; j < std::size(v4l2_scaling_matrix.scaling_list_4x4[i]);
         ++j) {
      // Parser uses source (zigzag) order, while V4L2 API requires raster
      // order.
      static_assert(
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_4x4), 1>() ==
          std::extent<decltype(zigzag_4x4)>());
      v4l2_scaling_matrix.scaling_list_4x4[i][zigzag_4x4[j]] =
          scaling_list4x4[i][j];
    }
  }
  for (size_t i = 0; i < std::size(v4l2_scaling_matrix.scaling_list_8x8); ++i) {
    for (size_t j = 0; j < std::size(v4l2_scaling_matrix.scaling_list_8x8[i]);
         ++j) {
      static_assert(
          std::extent<decltype(v4l2_scaling_matrix.scaling_list_8x8), 1>() ==
          std::extent<decltype(zigzag_8x8)>());
      v4l2_scaling_matrix.scaling_list_8x8[i][zigzag_8x8[j]] =
          scaling_list8x8[i][j];
    }
  }

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_H264_SCALING_MATRIX;
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
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSExtCtrls);
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return Status::kFail;
  }

  auto ref_surfaces = H264DPBToV4L2DPB(dpb);
  dec_surface->SetReferenceSurfaces(ref_surfaces);

  return Status::kOk;
}

H264Decoder::H264Accelerator::Status
V4L2VideoDecoderDelegateH264::ParseEncryptedSliceHeader(
    const std::vector<base::span<const uint8_t>>& data,
    const std::vector<SubsampleEntry>& /*subsamples*/,
    uint64_t secure_handle,
    H264SliceHeader* slice_header_out) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!cdm_context_ || !cdm_context_->GetChromeOsCdmContext()) {
    LOG(ERROR) << "Missing ChromeOSCdmContext";
    return Status::kFail;
  }
  if (!secure_handle) {
    LOG(ERROR) << "Invalid secure buffer";
    return Status::kFail;
  }
  if (encrypted_slice_header_parsing_failed_) {
    encrypted_slice_header_parsing_failed_ = false;
    last_parsed_encrypted_slice_header_.clear();
    return Status::kFail;
  }

  std::vector<uint8_t> stream_data_vec(
      reinterpret_cast<uint8_t*>(&cencv1_stream_data_),
      reinterpret_cast<uint8_t*>(&cencv1_stream_data_) +
          sizeof(cencv1_stream_data_));

  // Send the request for the slice header if we don't have a pending result.
  if (last_parsed_encrypted_slice_header_.empty()) {
    cdm_context_->GetChromeOsCdmContext()->ParseEncryptedSliceHeader(
        secure_handle,
        base::checked_cast<uint32_t>(encrypted_slice_header_offset_),
        stream_data_vec,
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &V4L2VideoDecoderDelegateH264::OnEncryptedSliceHeaderParsed,
            weak_factory_.GetWeakPtr())));
    return Status::kTryAgain;
  }
  // We have the result, map it to the structure and copy the fields.
  if (last_parsed_encrypted_slice_header_.size() !=
      sizeof(CencV1SliceParameterBufferH264)) {
    return Status::kFail;
  }
  CencV1SliceParameterBufferH264 slice_param_buf;
  memcpy(&slice_param_buf, last_parsed_encrypted_slice_header_.data(),
         sizeof(slice_param_buf));
  last_parsed_encrypted_slice_header_.clear();

  // Read the parsed slice header data back and populate the structure with it.
  slice_header_out->idr_pic_flag = !!slice_param_buf.idr_pic_flag;
  slice_header_out->nal_ref_idc = slice_param_buf.nal_ref_idc;
  slice_header_out->field_pic_flag = slice_param_buf.field_pic_flag;
  // The last span in |data| will be the slice header NALU.
  slice_header_out->nalu_data = data.back().data();
  slice_header_out->nalu_size = data.back().size();
  slice_header_out->slice_type = slice_param_buf.slice_type;
  slice_header_out->frame_num = slice_param_buf.frame_num;
  slice_header_out->idr_pic_id = slice_param_buf.idr_pic_id;
  slice_header_out->pic_order_cnt_lsb = slice_param_buf.pic_order_cnt_lsb;
  slice_header_out->delta_pic_order_cnt_bottom =
      slice_param_buf.delta_pic_order_cnt_bottom;
  slice_header_out->delta_pic_order_cnt0 = slice_param_buf.delta_pic_order_cnt0;
  slice_header_out->delta_pic_order_cnt1 = slice_param_buf.delta_pic_order_cnt1;
  slice_header_out->no_output_of_prior_pics_flag =
      slice_param_buf.ref_pic_fields.bits.no_output_of_prior_pics_flag;
  slice_header_out->long_term_reference_flag =
      slice_param_buf.ref_pic_fields.bits.long_term_reference_flag;
  slice_header_out->adaptive_ref_pic_marking_mode_flag =
      slice_param_buf.ref_pic_fields.bits.adaptive_ref_pic_marking_mode_flag;
  const size_t num_dec_ref_pics =
      slice_param_buf.ref_pic_fields.bits.dec_ref_pic_marking_count;
  if (num_dec_ref_pics > H264SliceHeader::kRefListSize) {
    DVLOG(1) << "Invalid number of dec_ref_pics: " << num_dec_ref_pics;
    return Status::kFail;
  }
  for (size_t i = 0; i < num_dec_ref_pics; ++i) {
    slice_header_out->ref_pic_marking[i].memory_mgmnt_control_operation =
        slice_param_buf.memory_management_control_operation[i];
    slice_header_out->ref_pic_marking[i].difference_of_pic_nums_minus1 =
        slice_param_buf.difference_of_pic_nums_minus1[i];
    slice_header_out->ref_pic_marking[i].long_term_pic_num =
        slice_param_buf.long_term_pic_num[i];
    slice_header_out->ref_pic_marking[i].long_term_frame_idx =
        slice_param_buf.long_term_frame_idx[i];
    slice_header_out->ref_pic_marking[i].max_long_term_frame_idx_plus1 =
        slice_param_buf.max_long_term_frame_idx_plus1[i];
  }
  slice_header_out->dec_ref_pic_marking_bit_size =
      slice_param_buf.dec_ref_pic_marking_bit_size;
  slice_header_out->pic_order_cnt_bit_size =
      slice_param_buf.pic_order_cnt_bit_size;
  slice_header_out->full_sample_encryption = true;
  return Status::kOk;
#else
  return Status::kFail;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

H264Decoder::H264Accelerator::Status V4L2VideoDecoderDelegateH264::SubmitSlice(
    const H264PPS* pps,
    const H264SliceHeader* slice_hdr,
    const H264Picture::Vector& ref_pic_list0,
    const H264Picture::Vector& ref_pic_list1,
    scoped_refptr<H264Picture> pic,
    const uint8_t* data,
    size_t size,
    const std::vector<SubsampleEntry>& subsamples) {
#define SHDR_TO_V4L2DPARM(a) priv_->v4l2_decode_param.a = slice_hdr->a
  SHDR_TO_V4L2DPARM(frame_num);
  SHDR_TO_V4L2DPARM(idr_pic_id);
  SHDR_TO_V4L2DPARM(pic_order_cnt_lsb);
  SHDR_TO_V4L2DPARM(delta_pic_order_cnt_bottom);
  SHDR_TO_V4L2DPARM(delta_pic_order_cnt0);
  SHDR_TO_V4L2DPARM(delta_pic_order_cnt1);
  SHDR_TO_V4L2DPARM(dec_ref_pic_marking_bit_size);
  SHDR_TO_V4L2DPARM(pic_order_cnt_bit_size);
#undef SHDR_TO_V4L2DPARM

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H264PictureToV4L2DecodeSurface(pic.get());

  priv_->v4l2_decode_param.nal_ref_idc = slice_hdr->nal_ref_idc;

  // Add the 3-bytes NAL start code.
  // TODO: don't do it here, but have it passed from the parser?
  const size_t data_copy_size = size + 3;
  if (dec_surface->secure_handle()) {
    // If this is multi-slice CENCv1, then we need to increase this offset.
    encrypted_slice_header_offset_ += size;
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

H264Decoder::H264Accelerator::Status V4L2VideoDecoderDelegateH264::SubmitDecode(
    scoped_refptr<H264Picture> pic) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      H264PictureToV4L2DecodeSurface(pic.get());

  switch (pic->field) {
    case H264Picture::FIELD_NONE:
      priv_->v4l2_decode_param.flags = 0;
      break;
    case H264Picture::FIELD_TOP:
      priv_->v4l2_decode_param.flags = V4L2_H264_DECODE_PARAM_FLAG_FIELD_PIC;
      break;
    case H264Picture::FIELD_BOTTOM:
      priv_->v4l2_decode_param.flags =
          (V4L2_H264_DECODE_PARAM_FLAG_FIELD_PIC |
           V4L2_H264_DECODE_PARAM_FLAG_BOTTOM_FIELD);
      break;
  }

  if (pic->idr)
    priv_->v4l2_decode_param.flags |= V4L2_H264_DECODE_PARAM_FLAG_IDR_PIC;

  priv_->v4l2_decode_param.top_field_order_cnt = pic->top_field_order_cnt;
  priv_->v4l2_decode_param.bottom_field_order_cnt = pic->bottom_field_order_cnt;

  struct v4l2_ext_control ctrl;
  std::vector<struct v4l2_ext_control> ctrls;

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_H264_DECODE_PARAMS;
  ctrl.size = sizeof(priv_->v4l2_decode_param);
  ctrl.ptr = &priv_->v4l2_decode_param;
  ctrls.push_back(ctrl);

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_STATELESS_H264_DECODE_MODE;
  ctrl.value = V4L2_STATELESS_H264_DECODE_MODE_FRAME_BASED;
  ctrls.push_back(ctrl);

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0];
  dec_surface->PrepareSetCtrls(&ext_ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) != 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSExtCtrls);
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return Status::kFail;
  }

  Reset();

  DVLOGF(4) << "Submitting decode for surface: " << dec_surface->ToString();
  surface_handler_->DecodeSurface(dec_surface);
  return Status::kOk;
}

bool V4L2VideoDecoderDelegateH264::OutputPicture(
    scoped_refptr<H264Picture> pic) {
  surface_handler_->SurfaceReady(H264PictureToV4L2DecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 pic->get_colorspace());
  return true;
}

void V4L2VideoDecoderDelegateH264::Reset() {
  memset(&priv_->v4l2_decode_param, 0, sizeof(priv_->v4l2_decode_param));
  encrypted_slice_header_offset_ = 0;
  last_parsed_encrypted_slice_header_.clear();
  encrypted_slice_header_parsing_failed_ = false;
}

scoped_refptr<V4L2DecodeSurface>
V4L2VideoDecoderDelegateH264::H264PictureToV4L2DecodeSurface(H264Picture* pic) {
  V4L2H264Picture* v4l2_pic = pic->AsV4L2H264Picture();
  CHECK(v4l2_pic);
  return v4l2_pic->dec_surface();
}

void V4L2VideoDecoderDelegateH264::OnEncryptedSliceHeaderParsed(
    bool status,
    const std::vector<uint8_t>& parsed_headers) {
  encrypted_slice_header_parsing_failed_ = !status;
  last_parsed_encrypted_slice_header_ = parsed_headers;
  surface_handler_->ResumeDecoding();
}

}  // namespace media
