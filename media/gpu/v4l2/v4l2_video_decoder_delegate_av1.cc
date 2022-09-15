// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/v4l2_video_decoder_delegate_av1.h"

#include <linux/media/av1-ctrls.h>

#include "media/gpu/macros.h"
#include "media/gpu/v4l2/v4l2_decode_surface.h"
#include "media/gpu/v4l2/v4l2_decode_surface_handler.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace media {

using DecodeStatus = AV1Decoder::AV1Accelerator::Status;

class V4L2AV1Picture : public AV1Picture {
 public:
  V4L2AV1Picture(scoped_refptr<V4L2DecodeSurface> dec_surface)
      : dec_surface_(std::move(dec_surface)) {}

  V4L2AV1Picture(const V4L2AV1Picture&) = delete;
  V4L2AV1Picture& operator=(const V4L2AV1Picture&) = delete;

  const scoped_refptr<V4L2DecodeSurface>& dec_surface() const {
    return dec_surface_;
  }

 private:
  ~V4L2AV1Picture() override = default;

  scoped_refptr<AV1Picture> CreateDuplicate() override {
    return new V4L2AV1Picture(dec_surface_);
  }

  scoped_refptr<V4L2DecodeSurface> dec_surface_;
};

namespace {
// TODO(stevecho): Remove this when AV1 uAPI RFC v3 change
// (crrev/c/3859126) lands.
#ifndef BIT
#define BIT(nr) (1U << (nr))
#endif

// Section 5.5. Sequence header OBU syntax in the AV1 spec.
// https://aomediacodec.github.io/av1-spec
void FillSequenceParams(v4l2_ctrl_av1_sequence& v4l2_seq_params,
                        const libgav1::ObuSequenceHeader& seq_header) {
  if (seq_header.still_picture)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_STILL_PICTURE;

  if (seq_header.use_128x128_superblock)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_USE_128X128_SUPERBLOCK;

  if (seq_header.enable_filter_intra)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_FILTER_INTRA;

  if (seq_header.enable_intra_edge_filter)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTRA_EDGE_FILTER;

  if (seq_header.enable_interintra_compound)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTERINTRA_COMPOUND;

  if (seq_header.enable_masked_compound)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_MASKED_COMPOUND;

  if (seq_header.enable_warped_motion)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_WARPED_MOTION;

  if (seq_header.enable_dual_filter)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_DUAL_FILTER;

  if (seq_header.enable_order_hint)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_ORDER_HINT;

  if (seq_header.enable_jnt_comp)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_JNT_COMP;

  if (seq_header.enable_ref_frame_mvs)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_REF_FRAME_MVS;

  if (seq_header.enable_superres)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_SUPERRES;

  if (seq_header.enable_cdef)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_CDEF;

  if (seq_header.enable_restoration)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_ENABLE_RESTORATION;

  if (seq_header.color_config.is_monochrome)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_MONO_CHROME;

  if (seq_header.color_config.color_range)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_COLOR_RANGE;

  if (seq_header.color_config.subsampling_x)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_X;

  if (seq_header.color_config.subsampling_y)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_Y;

  if (seq_header.film_grain_params_present)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_FILM_GRAIN_PARAMS_PRESENT;

  if (seq_header.color_config.separate_uv_delta_q)
    v4l2_seq_params.flags |= V4L2_AV1_SEQUENCE_FLAG_SEPARATE_UV_DELTA_Q;

  v4l2_seq_params.seq_profile = seq_header.profile;
  v4l2_seq_params.order_hint_bits = seq_header.order_hint_bits;
  v4l2_seq_params.bit_depth = seq_header.color_config.bitdepth;
  v4l2_seq_params.max_frame_width_minus_1 = seq_header.max_frame_width - 1;
  v4l2_seq_params.max_frame_height_minus_1 = seq_header.max_frame_height - 1;
}

// Section 5.9.11. Loop filter params syntax.
// Note that |update_ref_delta| and |update_mode_delta| flags in the spec
// are not needed for V4L2 AV1 API.
void FillLoopFilterParams(v4l2_av1_loop_filter& v4l2_lf,
                          const libgav1::LoopFilter& lf) {
  if (lf.delta_enabled)
    v4l2_lf.flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_ENABLED;

  if (lf.delta_update)
    v4l2_lf.flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_UPDATE;

  static_assert(std::size(decltype(v4l2_lf.level){}) == libgav1::kFrameLfCount,
                "Invalid size of loop filter level (strength) array");
  for (size_t i = 0; i < libgav1::kFrameLfCount; i++)
    v4l2_lf.level[i] = base::checked_cast<__u8>(lf.level[i]);

  v4l2_lf.sharpness = lf.sharpness;

  static_assert(std::size(decltype(v4l2_lf.ref_deltas){}) ==
                    libgav1::kNumReferenceFrameTypes,
                "Invalid size of ref deltas array");
  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; i++)
    v4l2_lf.ref_deltas[i] = lf.ref_deltas[i];

  static_assert(std::size(decltype(v4l2_lf.mode_deltas){}) ==
                    libgav1::kLoopFilterMaxModeDeltas,
                "Invalid size of mode deltas array");
  for (size_t i = 0; i < libgav1::kLoopFilterMaxModeDeltas; i++)
    v4l2_lf.mode_deltas[i] = lf.mode_deltas[i];
}

// Section 5.9.12. Quantization params syntax
void FillQuantizationParams(v4l2_av1_quantization& v4l2_quant,
                            const libgav1::QuantizerParameters& quant) {
  if (quant.use_matrix)
    v4l2_quant.flags |= V4L2_AV1_QUANTIZATION_FLAG_USING_QMATRIX;

  v4l2_quant.base_q_idx = quant.base_index;

  // Note that quant.delta_ac[0] is useless
  // because it is always 0 according to libgav1.
  v4l2_quant.delta_q_y_dc = quant.delta_dc[0];

  v4l2_quant.delta_q_u_dc = quant.delta_dc[1];
  v4l2_quant.delta_q_u_ac = quant.delta_ac[1];

  v4l2_quant.delta_q_v_dc = quant.delta_dc[2];
  v4l2_quant.delta_q_v_ac = quant.delta_ac[2];

  if (!quant.use_matrix)
    return;

  v4l2_quant.qm_y = base::checked_cast<uint8_t>(quant.matrix_level[0]);
  v4l2_quant.qm_u = base::checked_cast<uint8_t>(quant.matrix_level[1]);
  v4l2_quant.qm_v = base::checked_cast<uint8_t>(quant.matrix_level[2]);
}

}  // namespace

// Section 5.9.14. Segmentation params syntax
void FillSegmentationParams(struct v4l2_av1_segmentation& v4l2_seg,
                            const libgav1::Segmentation& seg) {
  if (seg.enabled)
    v4l2_seg.flags |= V4L2_AV1_SEGMENTATION_FLAG_ENABLED;

  if (seg.update_map)
    v4l2_seg.flags |= V4L2_AV1_SEGMENTATION_FLAG_UPDATE_MAP;

  if (seg.temporal_update)
    v4l2_seg.flags |= V4L2_AV1_SEGMENTATION_FLAG_TEMPORAL_UPDATE;

  if (seg.update_data)
    v4l2_seg.flags |= V4L2_AV1_SEGMENTATION_FLAG_UPDATE_DATA;

  if (seg.segment_id_pre_skip)
    v4l2_seg.flags |= V4L2_AV1_SEGMENTATION_FLAG_SEG_ID_PRE_SKIP;

  static_assert(
      std::size(decltype(v4l2_seg.feature_enabled){}) == libgav1::kMaxSegments,
      "Invalid size of |feature_enabled| array in |v4l2_av1_segmentation| "
      "struct");

  static_assert(
      std::size(decltype(v4l2_seg.feature_data){}) == libgav1::kMaxSegments &&
          std::extent<decltype(v4l2_seg.feature_data), 0>::value ==
              libgav1::kSegmentFeatureMax,
      "Invalid size of |feature_data| array in |v4l2_av1_segmentation| struct");

  for (size_t i = 0; i < libgav1::kMaxSegments; ++i) {
    for (size_t j = 0; j < libgav1::kSegmentFeatureMax; ++j) {
      v4l2_seg.feature_enabled[i] |= (seg.feature_enabled[i][j] << j);
      v4l2_seg.feature_data[i][j] = seg.feature_data[i][j];
    }
  }

  v4l2_seg.last_active_seg_id = seg.last_active_segment_id;
}

// Section 5.9.17. Quantizer index delta parameters syntax
void FillQuantizerIndexDeltaParams(struct v4l2_av1_quantization& v4l2_quant,
                                   const libgav1::ObuSequenceHeader& seq_header,
                                   const libgav1::ObuFrameHeader& frm_header) {
  // |diff_uv_delta| in the spec doesn't exist in libgav1,
  // because libgav1 infers it using the following logic.
  const bool diff_uv_delta = (frm_header.quantizer.base_index != 0) &&
                             (!seq_header.color_config.is_monochrome) &&
                             (seq_header.color_config.separate_uv_delta_q);
  if (diff_uv_delta)
    v4l2_quant.flags |= V4L2_AV1_QUANTIZATION_FLAG_DIFF_UV_DELTA;

  if (frm_header.delta_q.present)
    v4l2_quant.flags |= V4L2_AV1_QUANTIZATION_FLAG_DELTA_Q_PRESENT;

  // |scale| is used to store |delta_q_res| value. This is because libgav1 uses
  // the same struct |Delta| both for quantizer index delta parameters and loop
  // filter delta parameters.
  v4l2_quant.delta_q_res = frm_header.delta_q.scale;
}

V4L2VideoDecoderDelegateAV1::V4L2VideoDecoderDelegateAV1(
    V4L2DecodeSurfaceHandler* surface_handler,
    V4L2Device* device)
    : surface_handler_(surface_handler), device_(device) {
  VLOGF(1);
  DCHECK(surface_handler_);
  DCHECK(device_);
}

V4L2VideoDecoderDelegateAV1::~V4L2VideoDecoderDelegateAV1() = default;

scoped_refptr<AV1Picture> V4L2VideoDecoderDelegateAV1::CreateAV1Picture(
    bool apply_grain) {
  scoped_refptr<V4L2DecodeSurface> dec_surface =
      surface_handler_->CreateSurface();
  if (!dec_surface)
    return nullptr;

  return new V4L2AV1Picture(std::move(dec_surface));
}

DecodeStatus V4L2VideoDecoderDelegateAV1::SubmitDecode(
    const AV1Picture& pic,
    const libgav1::ObuSequenceHeader& sequence_header,
    const AV1ReferenceFrameVector& ref_frames,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    base::span<const uint8_t> data) {
  struct v4l2_ctrl_av1_sequence v4l2_seq_params = {};
  FillSequenceParams(v4l2_seq_params, sequence_header);

  const libgav1::ObuFrameHeader& frame_header = pic.frame_header;

  struct v4l2_av1_loop_filter v4l2_lf = {};
  FillLoopFilterParams(v4l2_lf, frame_header.loop_filter);

  struct v4l2_av1_quantization v4l2_quant = {};
  FillQuantizationParams(v4l2_quant, frame_header.quantizer);

  FillQuantizerIndexDeltaParams(v4l2_quant, sequence_header, frame_header);

  struct v4l2_av1_segmentation v4l2_seg = {};
  FillSegmentationParams(v4l2_seg, frame_header.segmentation);

  NOTIMPLEMENTED();

  return DecodeStatus::kFail;
}

bool V4L2VideoDecoderDelegateAV1::OutputPicture(const AV1Picture& pic) {
  VLOGF(3);
  const auto* v4l2_pic = static_cast<const V4L2AV1Picture*>(&pic);

  surface_handler_->SurfaceReady(
      v4l2_pic->dec_surface(), v4l2_pic->bitstream_id(),
      v4l2_pic->visible_rect(), v4l2_pic->get_colorspace());

  return true;
}

}  // namespace media
