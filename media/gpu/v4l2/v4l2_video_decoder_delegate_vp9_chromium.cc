// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Prevent inclusion of legacy controls.
#define __LINUX_MEDIA_VP9_CTRLS_LEGACY_H_

#include <linux/media/vp9-ctrls.h>

#include "base/logging.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/gpu/v4l2/v4l2_video_decoder_delegate_vp9_chromium.h"

namespace media {

using DecodeStatus = VP9Decoder::VP9Accelerator::Status;

class V4L2VP9Picture : public VP9Picture {
 public:
  explicit V4L2VP9Picture(scoped_refptr<V4L2DecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  V4L2VP9Picture(const V4L2VP9Picture&) = delete;
  V4L2VP9Picture& operator=(const V4L2VP9Picture&) = delete;

  V4L2VP9Picture* AsV4L2VP9Picture() override { return this; }
  scoped_refptr<V4L2DecodeSurface> dec_surface() { return dec_surface_; }

 private:
  ~V4L2VP9Picture() override = default;

  scoped_refptr<VP9Picture> CreateDuplicate() override {
    return new V4L2VP9Picture(dec_surface_);
  }

  scoped_refptr<V4L2DecodeSurface> dec_surface_;
};

namespace {

scoped_refptr<V4L2DecodeSurface> VP9PictureToV4L2DecodeSurface(
    VP9Picture* pic) {
  V4L2VP9Picture* v4l2_pic = pic->AsV4L2VP9Picture();
  CHECK(v4l2_pic);
  return v4l2_pic->dec_surface();
}

void FillV4L2VP9LoopFilterParams(const Vp9LoopFilterParams& vp9_lf_params,
                                 struct v4l2_vp9_loop_filter* v4l2_lf) {
#define SET_FLAG_IF(cond, flag) \
  v4l2_lf->flags |= ((vp9_lf_params.cond) ? (flag) : 0)

  SET_FLAG_IF(delta_enabled, V4L2_VP9_LOOP_FILTER_FLAG_DELTA_ENABLED);
  SET_FLAG_IF(delta_update, V4L2_VP9_LOOP_FILTER_FLAG_DELTA_UPDATE);
#undef SET_FLAG_IF

  v4l2_lf->level = vp9_lf_params.level;
  v4l2_lf->sharpness = vp9_lf_params.sharpness;
  SafeArrayMemcpy(v4l2_lf->ref_deltas, vp9_lf_params.ref_deltas);
  SafeArrayMemcpy(v4l2_lf->mode_deltas, vp9_lf_params.mode_deltas);
  SafeArrayMemcpy(v4l2_lf->level_lookup, vp9_lf_params.lvl);
}

void FillV4L2VP9QuantizationParams(
    const Vp9QuantizationParams& vp9_quant_params,
    struct v4l2_vp9_quantization* v4l2_quant) {
  v4l2_quant->base_q_idx = vp9_quant_params.base_q_idx;
  v4l2_quant->delta_q_y_dc = vp9_quant_params.delta_q_y_dc;
  v4l2_quant->delta_q_uv_dc = vp9_quant_params.delta_q_uv_dc;
  v4l2_quant->delta_q_uv_ac = vp9_quant_params.delta_q_uv_ac;
}

void FillV4L2VP9SegmentationParams(const Vp9SegmentationParams& vp9_seg_params,
                                   struct v4l2_vp9_segmentation* v4l2_seg) {
#define SET_FLAG_IF(cond, flag) \
  v4l2_seg->flags |= ((vp9_seg_params.cond) ? (flag) : 0)

  SET_FLAG_IF(enabled, V4L2_VP9_SEGMENTATION_FLAG_ENABLED);
  SET_FLAG_IF(update_map, V4L2_VP9_SEGMENTATION_FLAG_UPDATE_MAP);
  SET_FLAG_IF(temporal_update, V4L2_VP9_SEGMENTATION_FLAG_TEMPORAL_UPDATE);
  SET_FLAG_IF(update_data, V4L2_VP9_SEGMENTATION_FLAG_UPDATE_DATA);
  SET_FLAG_IF(abs_or_delta_update,
              V4L2_VP9_SEGMENTATION_FLAG_ABS_OR_DELTA_UPDATE);
#undef SET_FLAG_IF

  SafeArrayMemcpy(v4l2_seg->tree_probs, vp9_seg_params.tree_probs);
  SafeArrayMemcpy(v4l2_seg->pred_probs, vp9_seg_params.pred_probs);

  static_assert(static_cast<size_t>(Vp9SegmentationParams::SEG_LVL_MAX) ==
                    static_cast<size_t>(V4L2_VP9_SEGMENT_FEATURE_CNT),
                "mismatch in number of segmentation features");
  for (size_t j = 0; j < 8; j++) {
    for (size_t i = 0; i < V4L2_VP9_SEGMENT_FEATURE_CNT; i++) {
      if (vp9_seg_params.feature_enabled[j][i])
        v4l2_seg->feature_enabled[j] |= V4L2_VP9_SEGMENT_FEATURE_ENABLED(i);
    }
  }

  SafeArrayMemcpy(v4l2_seg->feature_data, vp9_seg_params.feature_data);
}

void FillV4L2VP9MvProbsParams(const Vp9FrameContext& vp9_ctx,
                              struct v4l2_vp9_mv_probabilities* v4l2_mv_probs) {
  SafeArrayMemcpy(v4l2_mv_probs->joint, vp9_ctx.mv_joint_probs);
  SafeArrayMemcpy(v4l2_mv_probs->sign, vp9_ctx.mv_sign_prob);
  SafeArrayMemcpy(v4l2_mv_probs->class_, vp9_ctx.mv_class_probs);
  SafeArrayMemcpy(v4l2_mv_probs->class0_bit, vp9_ctx.mv_class0_bit_prob);
  SafeArrayMemcpy(v4l2_mv_probs->bits, vp9_ctx.mv_bits_prob);
  SafeArrayMemcpy(v4l2_mv_probs->class0_fr, vp9_ctx.mv_class0_fr_probs);
  SafeArrayMemcpy(v4l2_mv_probs->fr, vp9_ctx.mv_fr_probs);
  SafeArrayMemcpy(v4l2_mv_probs->class0_hp, vp9_ctx.mv_class0_hp_prob);
  SafeArrayMemcpy(v4l2_mv_probs->hp, vp9_ctx.mv_hp_prob);
}

void GetVP9MvProbsParams(const struct v4l2_vp9_mv_probabilities* v4l2_mv_probs,
                         Vp9FrameContext* vp9_ctx) {
  SafeArrayMemcpy(vp9_ctx->mv_joint_probs, v4l2_mv_probs->joint);
  SafeArrayMemcpy(vp9_ctx->mv_sign_prob, v4l2_mv_probs->sign);
  SafeArrayMemcpy(vp9_ctx->mv_class_probs, v4l2_mv_probs->class_);
  SafeArrayMemcpy(vp9_ctx->mv_class0_bit_prob, v4l2_mv_probs->class0_bit);
  SafeArrayMemcpy(vp9_ctx->mv_bits_prob, v4l2_mv_probs->bits);
  SafeArrayMemcpy(vp9_ctx->mv_class0_fr_probs, v4l2_mv_probs->class0_fr);
  SafeArrayMemcpy(vp9_ctx->mv_fr_probs, v4l2_mv_probs->fr);
  SafeArrayMemcpy(vp9_ctx->mv_class0_hp_prob, v4l2_mv_probs->class0_hp);
  SafeArrayMemcpy(vp9_ctx->mv_hp_prob, v4l2_mv_probs->hp);
}

void FillV4L2VP9ProbsParams(const Vp9FrameContext& vp9_ctx,
                            struct v4l2_vp9_probabilities* v4l2_probs) {
  SafeArrayMemcpy(v4l2_probs->tx8, vp9_ctx.tx_probs_8x8);
  SafeArrayMemcpy(v4l2_probs->tx16, vp9_ctx.tx_probs_16x16);
  SafeArrayMemcpy(v4l2_probs->tx32, vp9_ctx.tx_probs_32x32);
  SafeArrayMemcpy(v4l2_probs->coef, vp9_ctx.coef_probs);
  SafeArrayMemcpy(v4l2_probs->skip, vp9_ctx.skip_prob);
  SafeArrayMemcpy(v4l2_probs->inter_mode, vp9_ctx.inter_mode_probs);
  SafeArrayMemcpy(v4l2_probs->interp_filter, vp9_ctx.interp_filter_probs);
  SafeArrayMemcpy(v4l2_probs->is_inter, vp9_ctx.is_inter_prob);
  SafeArrayMemcpy(v4l2_probs->comp_mode, vp9_ctx.comp_mode_prob);
  SafeArrayMemcpy(v4l2_probs->single_ref, vp9_ctx.single_ref_prob);
  SafeArrayMemcpy(v4l2_probs->comp_ref, vp9_ctx.comp_ref_prob);
  SafeArrayMemcpy(v4l2_probs->y_mode, vp9_ctx.y_mode_probs);
  SafeArrayMemcpy(v4l2_probs->uv_mode, vp9_ctx.uv_mode_probs);
  SafeArrayMemcpy(v4l2_probs->partition, vp9_ctx.partition_probs);

  FillV4L2VP9MvProbsParams(vp9_ctx, &v4l2_probs->mv);
}

void GetVP9ProbsParams(const struct v4l2_vp9_probabilities* v4l2_probs,
                       Vp9FrameContext* vp9_ctx) {
  SafeArrayMemcpy(vp9_ctx->tx_probs_8x8, v4l2_probs->tx8);
  SafeArrayMemcpy(vp9_ctx->tx_probs_16x16, v4l2_probs->tx16);
  SafeArrayMemcpy(vp9_ctx->tx_probs_32x32, v4l2_probs->tx32);
  SafeArrayMemcpy(vp9_ctx->coef_probs, v4l2_probs->coef);
  SafeArrayMemcpy(vp9_ctx->skip_prob, v4l2_probs->skip);
  SafeArrayMemcpy(vp9_ctx->inter_mode_probs, v4l2_probs->inter_mode);
  SafeArrayMemcpy(vp9_ctx->interp_filter_probs, v4l2_probs->interp_filter);
  SafeArrayMemcpy(vp9_ctx->is_inter_prob, v4l2_probs->is_inter);
  SafeArrayMemcpy(vp9_ctx->comp_mode_prob, v4l2_probs->comp_mode);
  SafeArrayMemcpy(vp9_ctx->single_ref_prob, v4l2_probs->single_ref);
  SafeArrayMemcpy(vp9_ctx->comp_ref_prob, v4l2_probs->comp_ref);
  SafeArrayMemcpy(vp9_ctx->y_mode_probs, v4l2_probs->y_mode);
  SafeArrayMemcpy(vp9_ctx->uv_mode_probs, v4l2_probs->uv_mode);
  SafeArrayMemcpy(vp9_ctx->partition_probs, v4l2_probs->partition);

  GetVP9MvProbsParams(&v4l2_probs->mv, vp9_ctx);
}

}  // namespace

V4L2VideoDecoderDelegateVP9Chromium::V4L2VideoDecoderDelegateVP9Chromium(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : surface_handler_(surface_handler),
      device_(device),
      device_needs_compressed_header_parsed_(
          device->IsCtrlExposed(V4L2_CID_MPEG_VIDEO_VP9_FRAME_CONTEXT(0))) {
  DCHECK(surface_handler_);
}

V4L2VideoDecoderDelegateVP9Chromium::~V4L2VideoDecoderDelegateVP9Chromium() =
    default;

scoped_refptr<VP9Picture>
V4L2VideoDecoderDelegateVP9Chromium::CreateVP9Picture() {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface)
    return nullptr;

  return new V4L2VP9Picture(std::move(dec_surface));
}

DecodeStatus V4L2VideoDecoderDelegateVP9Chromium::SubmitDecode(
    scoped_refptr<VP9Picture> pic,
    const Vp9SegmentationParams& segm_params,
    const Vp9LoopFilterParams& lf_params,
    const Vp9ReferenceFrameVector& ref_frames,
    base::OnceClosure done_cb) {
  const Vp9FrameHeader* frame_hdr = pic->frame_hdr.get();
  DCHECK(frame_hdr);
  struct v4l2_ctrl_vp9_frame_decode_params v4l2_frame_params;
  memset(&v4l2_frame_params, 0, sizeof(v4l2_frame_params));

#define SET_FLAG_IF(cond, flag) \
  v4l2_frame_params.flags |= ((frame_hdr->cond) ? (flag) : 0)

  SET_FLAG_IF(frame_type == Vp9FrameHeader::KEYFRAME,
              V4L2_VP9_FRAME_FLAG_KEY_FRAME);
  SET_FLAG_IF(show_frame, V4L2_VP9_FRAME_FLAG_SHOW_FRAME);
  SET_FLAG_IF(error_resilient_mode, V4L2_VP9_FRAME_FLAG_ERROR_RESILIENT);
  SET_FLAG_IF(intra_only, V4L2_VP9_FRAME_FLAG_INTRA_ONLY);
  SET_FLAG_IF(allow_high_precision_mv, V4L2_VP9_FRAME_FLAG_ALLOW_HIGH_PREC_MV);
  SET_FLAG_IF(refresh_frame_context, V4L2_VP9_FRAME_FLAG_REFRESH_FRAME_CTX);
  SET_FLAG_IF(frame_parallel_decoding_mode,
              V4L2_VP9_FRAME_FLAG_PARALLEL_DEC_MODE);
  SET_FLAG_IF(subsampling_x, V4L2_VP9_FRAME_FLAG_X_SUBSAMPLING);
  SET_FLAG_IF(subsampling_y, V4L2_VP9_FRAME_FLAG_Y_SUBSAMPLING);
  SET_FLAG_IF(color_range, V4L2_VP9_FRAME_FLAG_COLOR_RANGE_FULL_SWING);
#undef SET_FLAG_IF

  v4l2_frame_params.compressed_header_size = frame_hdr->header_size_in_bytes;
  v4l2_frame_params.uncompressed_header_size =
      frame_hdr->uncompressed_header_size;
  v4l2_frame_params.profile = frame_hdr->profile;
  // As per the VP9 specification:
  switch (frame_hdr->reset_frame_context) {
    // "0 or 1 implies don’t reset."
    case 0:
    case 1:
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_NONE;
      break;
    // "2 resets just the context specified in the frame header."
    case 2:
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_SPEC;
      break;
    // "3 reset all contexts."
    case 3:
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_ALL;
      break;
    default:
      VLOGF(1) << "Invalid reset frame context value!";
      v4l2_frame_params.reset_frame_context = V4L2_VP9_RESET_FRAME_CTX_NONE;
      break;
  }
  v4l2_frame_params.frame_context_idx =
      frame_hdr->frame_context_idx_to_save_probs;
  v4l2_frame_params.bit_depth = frame_hdr->bit_depth;

  v4l2_frame_params.interpolation_filter = frame_hdr->interpolation_filter;
  v4l2_frame_params.tile_cols_log2 = frame_hdr->tile_cols_log2;
  v4l2_frame_params.tile_rows_log2 = frame_hdr->tile_rows_log2;
  v4l2_frame_params.tx_mode = frame_hdr->compressed_header.tx_mode;
  v4l2_frame_params.reference_mode =
      frame_hdr->compressed_header.reference_mode;
  for (size_t i = 0; i < V4L2_REF_ID_CNT; i++) {
    v4l2_frame_params.ref_frame_sign_biases |=
        (frame_hdr->ref_frame_sign_bias[i + VP9_FRAME_LAST] ? (1 << i) : 0);
  }
  v4l2_frame_params.frame_width_minus_1 = frame_hdr->frame_width - 1;
  v4l2_frame_params.frame_height_minus_1 = frame_hdr->frame_height - 1;
  v4l2_frame_params.render_width_minus_1 = frame_hdr->render_width - 1;
  v4l2_frame_params.render_height_minus_1 = frame_hdr->render_height - 1;

  // Reference frames
  for (size_t i = 0; i < std::size(frame_hdr->ref_frame_idx); i++) {
    uint8_t idx = frame_hdr->ref_frame_idx[i];
    if (idx >= kVp9NumRefFrames) {
      VLOGF(1) << "Invalid reference frame index!";
      return DecodeStatus::kFail;
    }

    auto ref_pic = ref_frames.GetFrame(idx);
    if (ref_pic) {
      auto ref_surface = VP9PictureToV4L2DecodeSurface(ref_pic.get());
      v4l2_frame_params.refs[i] = ref_surface->GetReferenceID();
    } else {
      v4l2_frame_params.refs[i] = 0xffffffff;
    }
  }

  FillV4L2VP9LoopFilterParams(lf_params, &v4l2_frame_params.lf);
  FillV4L2VP9QuantizationParams(frame_hdr->quant_params,
                                &v4l2_frame_params.quant);
  FillV4L2VP9SegmentationParams(segm_params, &v4l2_frame_params.seg);
  FillV4L2VP9ProbsParams(frame_hdr->frame_context, &v4l2_frame_params.probs);

  scoped_refptr<V4L2DecodeSurface> dec_surface =
      VP9PictureToV4L2DecodeSurface(pic.get());

  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_VP9_FRAME_DECODE_PARAMS;
  ctrl.size = sizeof(v4l2_frame_params);
  ctrl.ptr = &v4l2_frame_params;

  struct v4l2_ext_controls ctrls;
  memset(&ctrls, 0, sizeof(ctrls));
  ctrls.count = 1;
  ctrls.controls = &ctrl;
  dec_surface->PrepareSetCtrls(&ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ctrls) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_S_EXT_CTRLS";
    return DecodeStatus::kFail;
  }

  std::vector<scoped_refptr<V4L2DecodeSurface>> ref_surfaces;
  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    auto ref_pic = ref_frames.GetFrame(i);
    if (ref_pic) {
      auto ref_surface = VP9PictureToV4L2DecodeSurface(ref_pic.get());
      ref_surfaces.emplace_back(std::move(ref_surface));
    }
  }

  dec_surface->SetReferenceSurfaces(std::move(ref_surfaces));
  dec_surface->SetDecodeDoneCallback(std::move(done_cb));

  // Copy the frame data into the V4L2 buffer.
  if (!surface_handler_->SubmitSlice(dec_surface.get(), frame_hdr->data,
                                     frame_hdr->frame_size))
    return DecodeStatus::kFail;

  // Queue the buffers to the kernel driver.
  DVLOGF(4) << "Submitting decode for surface: " << dec_surface->ToString();
  surface_handler_->DecodeSurface(dec_surface);

  return DecodeStatus::kOk;
}

bool V4L2VideoDecoderDelegateVP9Chromium::OutputPicture(
    scoped_refptr<VP9Picture> pic) {
  surface_handler_->SurfaceReady(VP9PictureToV4L2DecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 pic->get_colorspace());
  return true;
}

bool V4L2VideoDecoderDelegateVP9Chromium::GetFrameContext(
    scoped_refptr<VP9Picture> pic,
    Vp9FrameContext* frame_ctx) {
  auto ctx_id = pic->frame_hdr->frame_context_idx_to_save_probs;

  struct v4l2_ctrl_vp9_frame_ctx v4l2_vp9_ctx;
  memset(&v4l2_vp9_ctx, 0, sizeof(v4l2_vp9_ctx));

  struct v4l2_ext_control ctrl;
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.id = V4L2_CID_MPEG_VIDEO_VP9_FRAME_CONTEXT(ctx_id);
  ctrl.size = sizeof(v4l2_vp9_ctx);
  ctrl.ptr = &v4l2_vp9_ctx;

  struct v4l2_ext_controls ctrls;
  memset(&ctrls, 0, sizeof(ctrls));
  ctrls.count = 1;
  ctrls.controls = &ctrl;
  if (device_->Ioctl(VIDIOC_G_EXT_CTRLS, &ctrls) != 0) {
    VPLOGF(1) << "ioctl() failed: VIDIOC_G_EXT_CTRLS";
    return false;
  }

  GetVP9ProbsParams(&v4l2_vp9_ctx.probs, frame_ctx);
  return true;
}

bool V4L2VideoDecoderDelegateVP9Chromium::NeedsCompressedHeaderParsed() const {
  return device_needs_compressed_header_parsed_;
}

bool V4L2VideoDecoderDelegateVP9Chromium::SupportsContextProbabilityReadback()
    const {
  return true;
}

}  // namespace media
