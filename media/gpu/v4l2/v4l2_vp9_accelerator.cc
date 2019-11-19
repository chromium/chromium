// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_vp9_accelerator.h"

#include <type_traits>

#include <linux/videodev2.h>
#include <string.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/vp9_picture.h"

namespace media {
namespace {

void FillV4L2VP9LoopFilterParams(
    const Vp9LoopFilterParams& vp9_lf_params,
    struct v4l2_vp9_loop_filter_params* v4l2_lf_params) {
#define SET_LF_PARAMS_FLAG_IF(cond, flag) \
  v4l2_lf_params->flags |= ((vp9_lf_params.cond) ? (flag) : 0)
  SET_LF_PARAMS_FLAG_IF(delta_enabled, V4L2_VP9_LOOP_FLTR_FLAG_DELTA_ENABLED);
  SET_LF_PARAMS_FLAG_IF(delta_update, V4L2_VP9_LOOP_FLTR_FLAG_DELTA_UPDATE);
#undef SET_LF_PARAMS_FLAG_IF

  v4l2_lf_params->level = vp9_lf_params.level;
  v4l2_lf_params->sharpness = vp9_lf_params.sharpness;

  SafeArrayMemcpy(v4l2_lf_params->deltas, vp9_lf_params.ref_deltas);
  SafeArrayMemcpy(v4l2_lf_params->mode_deltas, vp9_lf_params.mode_deltas);
  SafeArrayMemcpy(v4l2_lf_params->lvl_lookup, vp9_lf_params.lvl);
}

void FillV4L2VP9QuantizationParams(
    const Vp9QuantizationParams& vp9_quant_params,
    struct v4l2_vp9_quantization_params* v4l2_q_params) {
#define SET_Q_PARAMS_FLAG_IF(cond, flag) \
  v4l2_q_params->flags |= ((vp9_quant_params.cond) ? (flag) : 0)
  SET_Q_PARAMS_FLAG_IF(IsLossless(), V4L2_VP9_QUANT_PARAMS_FLAG_LOSSLESS);
#undef SET_Q_PARAMS_FLAG_IF

#define Q_PARAMS_TO_V4L2_Q_PARAMS(a) v4l2_q_params->a = vp9_quant_params.a
  Q_PARAMS_TO_V4L2_Q_PARAMS(base_q_idx);
  Q_PARAMS_TO_V4L2_Q_PARAMS(delta_q_y_dc);
  Q_PARAMS_TO_V4L2_Q_PARAMS(delta_q_uv_dc);
  Q_PARAMS_TO_V4L2_Q_PARAMS(delta_q_uv_ac);
#undef Q_PARAMS_TO_V4L2_Q_PARAMS
}

void FillV4L2VP9SegmentationParams(
    const Vp9SegmentationParams& vp9_segm_params,
    struct v4l2_vp9_segmentation_params* v4l2_segm_params) {
#define SET_SEG_PARAMS_FLAG_IF(cond, flag) \
  v4l2_segm_params->flags |= ((vp9_segm_params.cond) ? (flag) : 0)
  SET_SEG_PARAMS_FLAG_IF(enabled, V4L2_VP9_SGMNT_PARAM_FLAG_ENABLED);
  SET_SEG_PARAMS_FLAG_IF(update_map, V4L2_VP9_SGMNT_PARAM_FLAG_UPDATE_MAP);
  SET_SEG_PARAMS_FLAG_IF(temporal_update,
                         V4L2_VP9_SGMNT_PARAM_FLAG_TEMPORAL_UPDATE);
  SET_SEG_PARAMS_FLAG_IF(update_data, V4L2_VP9_SGMNT_PARAM_FLAG_UPDATE_DATA);
  SET_SEG_PARAMS_FLAG_IF(abs_or_delta_update,
                         V4L2_VP9_SGMNT_PARAM_FLAG_ABS_OR_DELTA_UPDATE);
#undef SET_SEG_PARAMS_FLAG_IF

  SafeArrayMemcpy(v4l2_segm_params->tree_probs, vp9_segm_params.tree_probs);
  SafeArrayMemcpy(v4l2_segm_params->pred_probs, vp9_segm_params.pred_probs);
  SafeArrayMemcpy(v4l2_segm_params->feature_data, vp9_segm_params.feature_data);

  static_assert(
      std::extent<decltype(v4l2_segm_params->feature_enabled)>() ==
              std::extent<decltype(vp9_segm_params.feature_enabled)>() &&
          std::extent<decltype(v4l2_segm_params->feature_enabled[0])>() ==
              std::extent<decltype(vp9_segm_params.feature_enabled[0])>(),
      "feature_enabled arrays must be of same size");
  for (size_t i = 0; i < base::size(v4l2_segm_params->feature_enabled); ++i) {
    for (size_t j = 0; j < base::size(v4l2_segm_params->feature_enabled[i]);
         ++j) {
      v4l2_segm_params->feature_enabled[i][j] =
          vp9_segm_params.feature_enabled[i][j];
    }
  }
}

void FillV4L2Vp9EntropyContext(const Vp9FrameContext& vp9_frame_ctx,
                               struct v4l2_vp9_entropy_ctx* v4l2_entropy_ctx) {
#define ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(a) \
  SafeArrayMemcpy(v4l2_entropy_ctx->a, vp9_frame_ctx.a)
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(tx_probs_8x8);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(tx_probs_16x16);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(tx_probs_32x32);

  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(coef_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(skip_prob);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(inter_mode_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(interp_filter_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(is_inter_prob);

  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(comp_mode_prob);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(single_ref_prob);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(comp_ref_prob);

  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(y_mode_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(uv_mode_probs);

  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(partition_probs);

  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_joint_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_sign_prob);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_class_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_class0_bit_prob);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_bits_prob);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_class0_fr_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_fr_probs);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_class0_hp_prob);
  ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR(mv_hp_prob);
#undef ARRAY_MEMCPY_CHECKED_FRM_CTX_TO_V4L2_ENTR
}

void FillVp9FrameContext(struct v4l2_vp9_entropy_ctx& v4l2_entropy_ctx,
                         Vp9FrameContext* vp9_frame_ctx) {
#define ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(a) \
  SafeArrayMemcpy(vp9_frame_ctx->a, v4l2_entropy_ctx.a)
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(tx_probs_8x8);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(tx_probs_16x16);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(tx_probs_32x32);

  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(coef_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(skip_prob);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(inter_mode_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(interp_filter_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(is_inter_prob);

  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(comp_mode_prob);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(single_ref_prob);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(comp_ref_prob);

  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(y_mode_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(uv_mode_probs);

  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(partition_probs);

  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_joint_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_sign_prob);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_class_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_class0_bit_prob);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_bits_prob);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_class0_fr_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_fr_probs);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_class0_hp_prob);
  ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX(mv_hp_prob);
#undef ARRAY_MEMCPY_CHECKED_V4L2_ENTR_TO_FRM_CTX
}

}  // namespace

class V4L2VP9Picture : public VP9Picture {
 public:
  explicit V4L2VP9Picture(scoped_refptr<V4L2DecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  V4L2VP9Picture* AsV4L2VP9Picture() override { return this; }
  scoped_refptr<V4L2DecodeSurface> dec_surface() { return dec_surface_; }

 private:
  ~V4L2VP9Picture() override {}

  scoped_refptr<VP9Picture> CreateDuplicate() override {
    return new V4L2VP9Picture(dec_surface_);
  }

  scoped_refptr<V4L2DecodeSurface> dec_surface_;

  DISALLOW_COPY_AND_ASSIGN(V4L2VP9Picture);
};

V4L2VP9Accelerator::V4L2VP9Accelerator(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : surface_handler_(surface_handler), device_(device) {
  DCHECK(surface_handler_);

  struct v4l2_queryctrl query_ctrl;
  memset(&query_ctrl, 0, sizeof(query_ctrl));
  query_ctrl.id = V4L2_CID_MPEG_VIDEO_VP9_ENTROPY;
  device_needs_frame_context_ =
      (device_->Ioctl(VIDIOC_QUERYCTRL, &query_ctrl) == 0);

  DVLOG_IF(1, device_needs_frame_context_)
      << "Device requires frame context parsing";
}

V4L2VP9Accelerator::~V4L2VP9Accelerator() {}

scoped_refptr<VP9Picture> V4L2VP9Accelerator::CreateVP9Picture() {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface)
    return nullptr;

  return new V4L2VP9Picture(std::move(dec_surface));
}

bool V4L2VP9Accelerator::SubmitDecode(scoped_refptr<VP9Picture> pic,
                                      const Vp9SegmentationParams& segm_params,
                                      const Vp9LoopFilterParams& lf_params,
                                      const Vp9ReferenceFrameVector& ref_frames,
                                      const base::Closure& done_cb) {
  const Vp9FrameHeader* frame_hdr = pic->frame_hdr.get();
  DCHECK(frame_hdr);

  struct v4l2_ctrl_vp9_frame_hdr v4l2_frame_hdr;
  memset(&v4l2_frame_hdr, 0, sizeof(v4l2_frame_hdr));

#define FHDR_TO_V4L2_FHDR(a) v4l2_frame_hdr.a = frame_hdr->a
  FHDR_TO_V4L2_FHDR(profile);
  FHDR_TO_V4L2_FHDR(frame_type);

  FHDR_TO_V4L2_FHDR(bit_depth);
  FHDR_TO_V4L2_FHDR(color_range);
  FHDR_TO_V4L2_FHDR(subsampling_x);
  FHDR_TO_V4L2_FHDR(subsampling_y);

  FHDR_TO_V4L2_FHDR(frame_width);
  FHDR_TO_V4L2_FHDR(frame_height);
  FHDR_TO_V4L2_FHDR(render_width);
  FHDR_TO_V4L2_FHDR(render_height);

  FHDR_TO_V4L2_FHDR(reset_frame_context);

  FHDR_TO_V4L2_FHDR(interpolation_filter);
  FHDR_TO_V4L2_FHDR(frame_context_idx);

  FHDR_TO_V4L2_FHDR(tile_cols_log2);
  FHDR_TO_V4L2_FHDR(tile_rows_log2);

  FHDR_TO_V4L2_FHDR(header_size_in_bytes);
#undef FHDR_TO_V4L2_FHDR
  v4l2_frame_hdr.color_space = static_cast<uint8_t>(frame_hdr->color_space);

  FillV4L2VP9QuantizationParams(frame_hdr->quant_params,
                                &v4l2_frame_hdr.quant_params);

#define SET_V4L2_FRM_HDR_FLAG_IF(cond, flag) \
  v4l2_frame_hdr.flags |= ((frame_hdr->cond) ? (flag) : 0)
  SET_V4L2_FRM_HDR_FLAG_IF(show_frame, V4L2_VP9_FRAME_HDR_FLAG_SHOW_FRAME);
  SET_V4L2_FRM_HDR_FLAG_IF(error_resilient_mode,
                           V4L2_VP9_FRAME_HDR_FLAG_ERR_RES);
  SET_V4L2_FRM_HDR_FLAG_IF(intra_only, V4L2_VP9_FRAME_HDR_FLAG_FRAME_INTRA);
  SET_V4L2_FRM_HDR_FLAG_IF(allow_high_precision_mv,
                           V4L2_VP9_FRAME_HDR_ALLOW_HIGH_PREC_MV);
  SET_V4L2_FRM_HDR_FLAG_IF(refresh_frame_context,
                           V4L2_VP9_FRAME_HDR_REFRESH_FRAME_CTX);
  SET_V4L2_FRM_HDR_FLAG_IF(frame_parallel_decoding_mode,
                           V4L2_VP9_FRAME_HDR_PARALLEL_DEC_MODE);
#undef SET_V4L2_FRM_HDR_FLAG_IF

  FillV4L2VP9LoopFilterParams(lf_params, &v4l2_frame_hdr.lf_params);
  FillV4L2VP9SegmentationParams(segm_params, &v4l2_frame_hdr.sgmnt_params);

  std::vector<struct v4l2_ext_control> ctrls;

  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_VP9_FRAME_HDR;
  ctrl.size = sizeof(v4l2_frame_hdr);
  ctrl.p_vp9_frame_hdr = &v4l2_frame_hdr;
  ctrls.push_back(ctrl);

  struct v4l2_ctrl_vp9_decode_param v4l2_decode_param;
  memset(&v4l2_decode_param, 0, sizeof(v4l2_decode_param));
  DCHECK_EQ(kVp9NumRefFrames, base::size(v4l2_decode_param.ref_frames));

  std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces;
  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    auto ref_pic = ref_frames.GetFrame(i);
    if (ref_pic) {
      scoped_refptr<V4L2DecodeSurface> ref_surface =
          VP9PictureToV4L2DecodeSurface(ref_pic.get());

      v4l2_decode_param.ref_frames[i] = ref_surface->GetReferenceID();
      ref_surfaces.push_back(ref_surface);
    } else {
      v4l2_decode_param.ref_frames[i] = VIDEO_MAX_FRAME;
    }
  }

  static_assert(std::extent<decltype(v4l2_decode_param.active_ref_frames)>() ==
                    std::extent<decltype(frame_hdr->ref_frame_idx)>(),
                "active reference frame array sizes mismatch");

  for (size_t i = 0; i < base::size(frame_hdr->ref_frame_idx); ++i) {
    uint8_t idx = frame_hdr->ref_frame_idx[i];
    if (idx >= kVp9NumRefFrames)
      return false;

    struct v4l2_vp9_reference_frame* v4l2_ref_frame =
        &v4l2_decode_param.active_ref_frames[i];

    scoped_refptr<VP9Picture> ref_pic = ref_frames.GetFrame(idx);
    if (ref_pic) {
      scoped_refptr<V4L2DecodeSurface> ref_surface =
          VP9PictureToV4L2DecodeSurface(ref_pic.get());
      v4l2_ref_frame->buf_index = ref_surface->GetReferenceID();
#define REF_TO_V4L2_REF(a) v4l2_ref_frame->a = ref_pic->frame_hdr->a
      REF_TO_V4L2_REF(frame_width);
      REF_TO_V4L2_REF(frame_height);
      REF_TO_V4L2_REF(bit_depth);
      REF_TO_V4L2_REF(subsampling_x);
      REF_TO_V4L2_REF(subsampling_y);
#undef REF_TO_V4L2_REF
    } else {
      v4l2_ref_frame->buf_index = VIDEO_MAX_FRAME;
    }
  }

  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_VP9_DECODE_PARAM;
  ctrl.size = sizeof(v4l2_decode_param);
  ctrl.p_vp9_decode_param = &v4l2_decode_param;
  ctrls.push_back(ctrl);

  // Defined outside of the if() clause below as it must remain valid until
  // the call to SubmitExtControls().
  struct v4l2_ctrl_vp9_entropy v4l2_entropy;
  if (device_needs_frame_context_) {
    memset(&v4l2_entropy, 0, sizeof(v4l2_entropy));
    FillV4L2Vp9EntropyContext(frame_hdr->initial_frame_context,
                              &v4l2_entropy.initial_entropy_ctx);
    FillV4L2Vp9EntropyContext(frame_hdr->frame_context,
                              &v4l2_entropy.current_entropy_ctx);
    v4l2_entropy.tx_mode = frame_hdr->compressed_header.tx_mode;
    v4l2_entropy.reference_mode = frame_hdr->compressed_header.reference_mode;

    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = V4L2_CID_MPEG_VIDEO_VP9_ENTROPY;
    ctrl.size = sizeof(v4l2_entropy);
    ctrl.p_vp9_entropy = &v4l2_entropy;
    ctrls.push_back(ctrl);
  }

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      VP9PictureToV4L2DecodeSurface(pic.get());

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = ctrls.size();
  ext_ctrls.controls = &ctrls[0];
  dec_surface->PrepareSetCtrls(&ext_ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ext_ctrls) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return false;
  }

  dec_surface->SetReferenceSurfaces(ref_surfaces);
  dec_surface->SetDecodeDoneCallback(done_cb);

  if (!surface_handler_->SubmitSlice(dec_surface, frame_hdr->data,
                                     frame_hdr->frame_size))
    return false;

  DVLOGF(4) << "Submitting decode for surface: " << dec_surface->ToString();
  surface_handler_->DecodeSurface(dec_surface);
  return true;
}

bool V4L2VP9Accelerator::OutputPicture(scoped_refptr<VP9Picture> pic) {
  // TODO(crbug.com/647725): Insert correct color space.
  surface_handler_->SurfaceReady(VP9PictureToV4L2DecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 VideoColorSpace());
  return true;
}

bool V4L2VP9Accelerator::GetFrameContext(scoped_refptr<VP9Picture> pic,
                                         Vp9FrameContext* frame_ctx) {
  struct v4l2_ctrl_vp9_entropy v4l2_entropy;
  memset(&v4l2_entropy, 0, sizeof(v4l2_entropy));

  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_VP9_ENTROPY;
  ctrl.size = sizeof(v4l2_entropy);
  ctrl.p_vp9_entropy = &v4l2_entropy;

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      VP9PictureToV4L2DecodeSurface(pic.get());

  struct v4l2_ext_controls ext_ctrls;
  memset(&ext_ctrls, 0, sizeof(ext_ctrls));
  ext_ctrls.count = 1;
  ext_ctrls.controls = &ctrl;
  dec_surface->PrepareSetCtrls(&ext_ctrls);
  if (device_->Ioctl(VIDIOC_G_EXT_CTRLS, &ext_ctrls) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_G_EXT_CTRLS";
    return false;
  }

  FillVp9FrameContext(v4l2_entropy.current_entropy_ctx, frame_ctx);
  return true;
}

bool V4L2VP9Accelerator::IsFrameContextRequired() const {
  return device_needs_frame_context_;
}

scoped_refptr<V4L2DecodeSurface>
V4L2VP9Accelerator::VP9PictureToV4L2DecodeSurface(VP9Picture* pic) {
  V4L2VP9Picture* v4l2_pic = pic->AsV4L2VP9Picture();
  CHECK(v4l2_pic);
  return v4l2_pic->dec_surface();
}

}  // namespace media
