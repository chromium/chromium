// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_delegate_vp9.h"

#include <linux/v4l2-controls.h>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "media/parsers/vp9_parser.h"

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

  constexpr size_t kV4L2VP9SegmentationFeaturesLength =
      std::extent<decltype(v4l2_seg->feature_enabled), 0>::value;

  static_assert(static_cast<size_t>(Vp9SegmentationParams::SEG_LVL_MAX) ==
                    static_cast<size_t>(V4L2_VP9_SEG_LVL_MAX),
                "mismatch in number of segmentation features");
  for (size_t j = 0; j < kV4L2VP9SegmentationFeaturesLength; j++) {
    for (size_t i = 0; i < V4L2_VP9_SEG_LVL_MAX; i++) {
      if (vp9_seg_params.feature_enabled[j][i])
        v4l2_seg->feature_enabled[j] |= V4L2_VP9_SEGMENT_FEATURE_ENABLED(i);
    }
  }

  SafeArrayMemcpy(v4l2_seg->feature_data, vp9_seg_params.feature_data);
}

void FillV4L2VP9MvProbsParams(const Vp9FrameContext& vp9_ctx,
                              struct v4l2_vp9_mv_probs* v4l2_mv_probs) {
  SafeArrayMemcpy(v4l2_mv_probs->joint, vp9_ctx.mv_joint_probs);
  SafeArrayMemcpy(v4l2_mv_probs->sign, vp9_ctx.mv_sign_prob);
  SafeArrayMemcpy(v4l2_mv_probs->classes, vp9_ctx.mv_class_probs);
  SafeArrayMemcpy(v4l2_mv_probs->class0_bit, vp9_ctx.mv_class0_bit_prob);
  SafeArrayMemcpy(v4l2_mv_probs->bits, vp9_ctx.mv_bits_prob);
  SafeArrayMemcpy(v4l2_mv_probs->class0_fr, vp9_ctx.mv_class0_fr_probs);
  SafeArrayMemcpy(v4l2_mv_probs->fr, vp9_ctx.mv_fr_probs);
  SafeArrayMemcpy(v4l2_mv_probs->class0_hp, vp9_ctx.mv_class0_hp_prob);
  SafeArrayMemcpy(v4l2_mv_probs->hp, vp9_ctx.mv_hp_prob);
}

void FillV4L2VP9ProbsParams(const Vp9FrameContext& vp9_ctx,
                            struct v4l2_ctrl_vp9_compressed_hdr* v4l2_probs) {
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

}  // namespace

V4L2VideoDecoderDelegateVP9::V4L2VideoDecoderDelegateVP9(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : surface_handler_(surface_handler),
      device_(device),
      supports_compressed_header_(
          device->IsCtrlExposed(V4L2_CID_STATELESS_VP9_COMPRESSED_HDR)) {
  VLOGF(1);
  DCHECK(surface_handler_);
  DCHECK(device_);

  // This control, originally landed in v5.17, is pretty much a marker that the
  // driver supports the stable API.
  const bool supports_stable_api =
      device_->IsCtrlExposed(V4L2_CID_STATELESS_VP9_FRAME);
  DCHECK(supports_stable_api);
}

V4L2VideoDecoderDelegateVP9::~V4L2VideoDecoderDelegateVP9() = default;

scoped_refptr<VP9Picture> V4L2VideoDecoderDelegateVP9::CreateVP9Picture() {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface)
    return nullptr;

  return new V4L2VP9Picture(std::move(dec_surface));
}

scoped_refptr<VP9Picture> V4L2VideoDecoderDelegateVP9::CreateVP9PictureSecure(
    uint64_t secure_handle) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSecureSurface(secure_handle);
  if (!dec_surface) {
    return nullptr;
  }

  return new V4L2VP9Picture(std::move(dec_surface));
}

DecodeStatus V4L2VideoDecoderDelegateVP9::SubmitDecode(
    scoped_refptr<VP9Picture> pic,
    const Vp9SegmentationParams& segm_params,
    const Vp9LoopFilterParams& lf_params,
    const Vp9ReferenceFrameVector& ref_frames) {
  const Vp9FrameHeader* frame_hdr = pic->frame_hdr.get();
  DCHECK(frame_hdr);
  struct v4l2_ctrl_vp9_frame v4l2_frame_params;
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
  if (supports_compressed_header_) {
    v4l2_frame_params.reference_mode =
        frame_hdr->compressed_header.reference_mode;
  }
  for (size_t i = 0; i < Vp9RefType::VP9_FRAME_MAX - VP9_FRAME_LAST; i++) {
    v4l2_frame_params.ref_frame_sign_bias |=
        (frame_hdr->ref_frame_sign_bias[i + VP9_FRAME_LAST] ? (1 << i) : 0);
  }
  v4l2_frame_params.frame_width_minus_1 = frame_hdr->frame_width - 1;
  v4l2_frame_params.frame_height_minus_1 = frame_hdr->frame_height - 1;
  v4l2_frame_params.render_width_minus_1 = frame_hdr->render_width - 1;
  v4l2_frame_params.render_height_minus_1 = frame_hdr->render_height - 1;

  for (size_t i = 0; i < std::size(frame_hdr->ref_frame_idx); i++) {
    uint8_t idx = frame_hdr->ref_frame_idx[i];
    if (idx >= kVp9NumRefFrames) {
      VLOGF(1) << "Invalid reference frame index!";
      return DecodeStatus::kFail;
    }

    auto ref_pic = ref_frames.GetFrame(idx);
    if (ref_pic) {
      auto ref_surface = VP9PictureToV4L2DecodeSurface(ref_pic.get());
      // Only partially/indirectly documented in the VP9 spec, but this array
      // contains LAST, GOLDEN, and ALT, in that order.
      switch (i) {
        case 0:
          v4l2_frame_params.last_frame_ts = ref_surface->GetReferenceID();
          break;
        case 1:
          v4l2_frame_params.golden_frame_ts = ref_surface->GetReferenceID();
          break;
        case 2:
          v4l2_frame_params.alt_frame_ts = ref_surface->GetReferenceID();
          break;
        default:
          NOTREACHED_IN_MIGRATION() << "Invalid reference frame index";
      }
    }
  }

  FillV4L2VP9LoopFilterParams(lf_params, &v4l2_frame_params.lf);
  FillV4L2VP9QuantizationParams(frame_hdr->quant_params,
                                &v4l2_frame_params.quant);
  FillV4L2VP9SegmentationParams(segm_params, &v4l2_frame_params.seg);

  std::vector<struct v4l2_ext_control> ext_ctrls = {
      {.id = V4L2_CID_STATELESS_VP9_FRAME,
       .size = sizeof(v4l2_frame_params),
       .ptr = &v4l2_frame_params},
  };

  struct v4l2_ctrl_vp9_compressed_hdr v4l2_compressed_hdr_probs;
  if (supports_compressed_header_) {
    memset(&v4l2_compressed_hdr_probs, 0, sizeof(v4l2_compressed_hdr_probs));
    v4l2_compressed_hdr_probs.tx_mode = frame_hdr->compressed_header.tx_mode;
    FillV4L2VP9ProbsParams(frame_hdr->frame_context,
                           &v4l2_compressed_hdr_probs);
    ext_ctrls.push_back({.id = V4L2_CID_STATELESS_VP9_COMPRESSED_HDR,
                         .size = sizeof(v4l2_compressed_hdr_probs),
                         .ptr = &v4l2_compressed_hdr_probs});
  }

  const __u32 ext_ctrls_size = base::checked_cast<__u32>(ext_ctrls.size());
  struct v4l2_ext_controls ctrls = {.count = ext_ctrls_size,
                                    .controls = ext_ctrls.data()};
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      VP9PictureToV4L2DecodeSurface(pic.get());
  dec_surface->PrepareSetCtrls(&ctrls);
  if (device_->Ioctl(VIDIOC_S_EXT_CTRLS, &ctrls) != 0) {
    RecordVidiocIoctlErrorUMA(VidiocIoctlRequests::kVidiocSExtCtrls);
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

  // Copy the frame data into the V4L2 buffer.
  if (!surface_handler_->SubmitSlice(
          dec_surface.get(),
          dec_surface->secure_handle() ? nullptr : frame_hdr->data,
          frame_hdr->frame_size)) {
    return DecodeStatus::kFail;
  }

  // Queue the buffers to the kernel driver.
  DVLOGF(4) << "Submitting decode for surface: " << dec_surface->ToString();
  surface_handler_->DecodeSurface(dec_surface);

  return DecodeStatus::kOk;
}

bool V4L2VideoDecoderDelegateVP9::OutputPicture(scoped_refptr<VP9Picture> pic) {
  VLOGF(3);
  surface_handler_->SurfaceReady(VP9PictureToV4L2DecodeSurface(pic.get()),
                                 pic->bitstream_id(), pic->visible_rect(),
                                 pic->get_colorspace());
  return true;
}

bool V4L2VideoDecoderDelegateVP9::NeedsCompressedHeaderParsed() const {
  return supports_compressed_header_;
}

}  // namespace media
