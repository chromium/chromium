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

// Section 5.9.15. Tile info syntax
void FillTileInfo(v4l2_av1_tile_info& v4l2_ti, const libgav1::TileInfo& ti) {
  if (ti.uniform_spacing)
    v4l2_ti.flags |= V4L2_AV1_TILE_INFO_FLAG_UNIFORM_TILE_SPACING;

  static_assert(std::size(decltype(v4l2_ti.mi_col_starts){}) ==
                    (libgav1::kMaxTileColumns + 1),
                "Size of |mi_col_starts| array in |v4l2_av1_tile_info| struct "
                "does not match libgav1 expectation");

  for (size_t i = 0; i < libgav1::kMaxTileColumns + 1; i++) {
    v4l2_ti.mi_col_starts[i] =
        base::checked_cast<uint32_t>(ti.tile_column_start[i]);
  }
  static_assert(std::size(decltype(v4l2_ti.mi_row_starts){}) ==
                    (libgav1::kMaxTileRows + 1),
                "Size of |mi_row_starts| array in |v4l2_av1_tile_info| struct "
                "does not match libgav1 expectation");
  for (size_t i = 0; i < libgav1::kMaxTileRows + 1; i++) {
    v4l2_ti.mi_row_starts[i] =
        base::checked_cast<uint32_t>(ti.tile_row_start[i]);
  }

  if (!ti.uniform_spacing) {
    // Confirmed that |kMaxTileColumns| is enough size for
    // |width_in_sbs_minus_1| and |kMaxTileRows| is enough size for
    // |height_in_sbs_minus_1|
    // https://b.corp.google.com/issues/187828854#comment19
    static_assert(
        std::size(decltype(v4l2_ti.width_in_sbs_minus_1){}) ==
            libgav1::kMaxTileColumns,
        "Size of |width_in_sbs_minus_1| array in |v4l2_av1_tile_info| struct "
        "does not match libgav1 expectation");
    for (size_t i = 0; i < libgav1::kMaxTileColumns; i++) {
      if (ti.tile_column_width_in_superblocks[i] >= 1) {
        v4l2_ti.width_in_sbs_minus_1[i] = base::checked_cast<uint32_t>(
            ti.tile_column_width_in_superblocks[i] - 1);
      }
    }

    static_assert(
        std::size(decltype(v4l2_ti.height_in_sbs_minus_1){}) ==
            libgav1::kMaxTileRows,
        "Size of |height_in_sbs_minus_1| array in |v4l2_av1_tile_info| struct "
        "does not match libgav1 expectation");
    for (size_t i = 0; i < libgav1::kMaxTileRows; i++) {
      if (ti.tile_row_height_in_superblocks[i] >= 1) {
        v4l2_ti.height_in_sbs_minus_1[i] = base::checked_cast<uint32_t>(
            ti.tile_row_height_in_superblocks[i] - 1);
      }
    }
  }

  v4l2_ti.tile_size_bytes = ti.tile_size_bytes;
  v4l2_ti.context_update_tile_id = ti.context_update_id;
  v4l2_ti.tile_cols = ti.tile_columns;
  v4l2_ti.tile_rows = ti.tile_rows;
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

// Section 5.9.18. Loop filter delta parameters syntax.
// Note that |delta_lf_res| in |v4l2_av1_loop_filter| corresponds to
// |delta_lf.scale| in the frame header defined in libgav1.
void FillLoopFilterDeltaParams(struct v4l2_av1_loop_filter& v4l2_lf,
                               const libgav1::Delta& delta_lf) {
  if (delta_lf.present)
    v4l2_lf.flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_PRESENT;

  if (delta_lf.multi)
    v4l2_lf.flags |= V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_MULTI;

  v4l2_lf.delta_lf_res = delta_lf.scale;
}

// Section 5.9.19. CDEF params syntax
void FillCdefParams(struct v4l2_av1_cdef& v4l2_cdef,
                    const libgav1::Cdef& cdef,
                    uint8_t color_bitdepth) {
  // Damping value parsed in libgav1 is from the spec + (bitdepth - 8).
  // All the strength values parsed in libgav1 are from the spec and left
  // shifted by (bitdepth - 8).
  CHECK_GE(color_bitdepth, 8u);
  const uint8_t coeff_shift = color_bitdepth - 8u;

  v4l2_cdef.damping_minus_3 =
      base::checked_cast<uint8_t>(cdef.damping - coeff_shift - 3u);

  v4l2_cdef.bits = cdef.bits;

  static_assert(std::size(decltype(v4l2_cdef.y_pri_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef y_pri_strength strength");

  static_assert(std::size(decltype(v4l2_cdef.y_sec_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef y_sec_strength strength");

  static_assert(std::size(decltype(v4l2_cdef.uv_pri_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef uv_pri_strength strength");

  static_assert(std::size(decltype(v4l2_cdef.uv_sec_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef uv_sec_strength strength");

  SafeArrayMemcpy(v4l2_cdef.y_pri_strength, cdef.y_primary_strength);
  SafeArrayMemcpy(v4l2_cdef.y_sec_strength, cdef.y_secondary_strength);
  SafeArrayMemcpy(v4l2_cdef.uv_pri_strength, cdef.uv_primary_strength);
  SafeArrayMemcpy(v4l2_cdef.uv_sec_strength, cdef.uv_secondary_strength);
}

// 5.9.20. Loop restoration params syntax
void FillLoopRestorationParams(v4l2_av1_loop_restoration& v4l2_lr,
                               const libgav1::LoopRestoration& lr) {
  for (size_t i = 0; i < V4L2_AV1_NUM_PLANES_MAX; i++) {
    switch (lr.type[i]) {
      case libgav1::LoopRestorationType::kLoopRestorationTypeNone:
        v4l2_lr.frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_NONE;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeWiener:
        v4l2_lr.frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_WIENER;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSgrProj:
        v4l2_lr.frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_SGRPROJ;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSwitchable:
        v4l2_lr.frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_SWITCHABLE;
        break;
      default:
        NOTREACHED() << "Invalid loop restoration type";
    }

    if (v4l2_lr.frame_restoration_type[i] != V4L2_AV1_FRAME_RESTORE_NONE) {
      if (true)
        v4l2_lr.flags |= V4L2_AV1_LOOP_RESTORATION_FLAG_USES_LR;

      if (i > 0)
        v4l2_lr.flags |= V4L2_AV1_LOOP_RESTORATION_FLAG_USES_CHROMA_LR;
    }
  }

  const bool use_loop_restoration =
      std::find_if(std::begin(lr.type),
                   std::begin(lr.type) + libgav1::kMaxPlanes,
                   [](const auto type) {
                     return type != libgav1::kLoopRestorationTypeNone;
                   }) != (lr.type + libgav1::kMaxPlanes);

  if (!use_loop_restoration)
    return;

  DCHECK_GE(lr.unit_size_log2[0], lr.unit_size_log2[1]);
  DCHECK_LE(lr.unit_size_log2[0] - lr.unit_size_log2[1], 1);
  v4l2_lr.lr_unit_shift = lr.unit_size_log2[0] - 6;
  v4l2_lr.lr_uv_shift = lr.unit_size_log2[0] - lr.unit_size_log2[1];

  // AV1 spec (p.52) uses this formula with hard coded value 2.
  // https://aomediacodec.github.io/av1-spec/#loop-restoration-params-syntax
  v4l2_lr.loop_restoration_size[0] =
      V4L2_AV1_RESTORATION_TILESIZE_MAX >> (2 - v4l2_lr.lr_unit_shift);
  v4l2_lr.loop_restoration_size[1] =
      v4l2_lr.loop_restoration_size[0] >> v4l2_lr.lr_uv_shift;
  v4l2_lr.loop_restoration_size[2] =
      v4l2_lr.loop_restoration_size[0] >> v4l2_lr.lr_uv_shift;
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

  FillLoopFilterDeltaParams(v4l2_lf, frame_header.delta_lf);

  struct v4l2_av1_quantization v4l2_quant = {};
  FillQuantizationParams(v4l2_quant, frame_header.quantizer);

  FillQuantizerIndexDeltaParams(v4l2_quant, sequence_header, frame_header);

  struct v4l2_av1_segmentation v4l2_seg = {};
  FillSegmentationParams(v4l2_seg, frame_header.segmentation);

  const auto color_bitdepth = sequence_header.color_config.bitdepth;
  struct v4l2_av1_cdef v4l2_cdef = {};
  FillCdefParams(v4l2_cdef, frame_header.cdef,
                 base::strict_cast<int8_t>(color_bitdepth));

  struct v4l2_av1_loop_restoration v4l2_lr = {};
  FillLoopRestorationParams(v4l2_lr, frame_header.loop_restoration);

  struct v4l2_av1_tile_info v4l2_ti = {};
  FillTileInfo(v4l2_ti, frame_header.tile_info);

  struct v4l2_ctrl_av1_frame v4l2_frame_params = {};
  if (frame_header.show_frame)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_SHOW_FRAME;
  if (frame_header.showable_frame)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_SHOWABLE_FRAME;
  if (frame_header.error_resilient_mode)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_ERROR_RESILIENT_MODE;
  if (frame_header.enable_cdf_update == false)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_DISABLE_CDF_UPDATE;
  if (frame_header.allow_screen_content_tools)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_ALLOW_SCREEN_CONTENT_TOOLS;
  if (frame_header.force_integer_mv)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_FORCE_INTEGER_MV;
  if (frame_header.allow_intrabc)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_ALLOW_INTRABC;
  if (frame_header.use_superres)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_USE_SUPERRES;
  if (frame_header.allow_high_precision_mv)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_ALLOW_HIGH_PRECISION_MV;
  if (frame_header.is_motion_mode_switchable)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_IS_MOTION_MODE_SWITCHABLE;
  if (frame_header.use_ref_frame_mvs)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_USE_REF_FRAME_MVS;
  if (frame_header.enable_frame_end_update_cdf == false)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_DISABLE_FRAME_END_UPDATE_CDF;
  if (frame_header.tile_info.uniform_spacing)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_UNIFORM_TILE_SPACING;
  if (frame_header.allow_warped_motion)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_ALLOW_WARPED_MOTION;
  if (frame_header.reference_mode_select)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_REFERENCE_SELECT;
  if (frame_header.reduced_tx_set)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_REDUCED_TX_SET;
  if (frame_header.skip_mode_frame[0] > 0)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_SKIP_MODE_ALLOWED;
  if (frame_header.skip_mode_present)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_SKIP_MODE_PRESENT;
  if (frame_header.frame_size_override_flag)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_FRAME_SIZE_OVERRIDE;
  // libgav1 header doesn't have |buffer_removal_time_present_flag|.
  if (frame_header.buffer_removal_time[0] > 0)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_BUFFER_REMOVAL_TIME_PRESENT;
  if (frame_header.frame_refs_short_signaling)
    v4l2_frame_params.flags |= V4L2_AV1_FRAME_FLAG_FRAME_REFS_SHORT_SIGNALING;

  switch (frame_header.frame_type) {
    case libgav1::kFrameKey:
      v4l2_frame_params.frame_type = V4L2_AV1_KEY_FRAME;
      break;
    case libgav1::kFrameInter:
      v4l2_frame_params.frame_type = V4L2_AV1_INTER_FRAME;
      break;
    case libgav1::kFrameIntraOnly:
      v4l2_frame_params.frame_type = V4L2_AV1_INTRA_ONLY_FRAME;
      break;
    case libgav1::kFrameSwitch:
      v4l2_frame_params.frame_type = V4L2_AV1_SWITCH_FRAME;
      break;
    default:
      NOTREACHED() << "Invalid frame type, " << frame_header.frame_type;
  }

  v4l2_frame_params.order_hint = frame_header.order_hint;
  v4l2_frame_params.superres_denom = frame_header.superres_scale_denominator;
  v4l2_frame_params.upscaled_width = frame_header.upscaled_width;

  switch (frame_header.interpolation_filter) {
    case libgav1::kInterpolationFilterEightTap:
      v4l2_frame_params.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP;
      break;
    case libgav1::kInterpolationFilterEightTapSmooth:
      v4l2_frame_params.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH;
      break;
    case libgav1::kInterpolationFilterEightTapSharp:
      v4l2_frame_params.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP_SHARP;
      break;
    case libgav1::kInterpolationFilterBilinear:
      v4l2_frame_params.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_BILINEAR;
      break;
    case libgav1::kInterpolationFilterSwitchable:
      v4l2_frame_params.interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_SWITCHABLE;
      break;
    default:
      NOTREACHED() << "Invalid interpolation filter, "
                   << frame_header.interpolation_filter;
  }

  switch (frame_header.tx_mode) {
    case libgav1::kTxModeOnly4x4:
      v4l2_frame_params.tx_mode = V4L2_AV1_TX_MODE_ONLY_4X4;
      break;
    case libgav1::kTxModeLargest:
      v4l2_frame_params.tx_mode = V4L2_AV1_TX_MODE_LARGEST;
      break;
    case libgav1::kTxModeSelect:
      v4l2_frame_params.tx_mode = V4L2_AV1_TX_MODE_SELECT;
      break;
    default:
      NOTREACHED() << "Invalid tx mode, " << frame_header.tx_mode;
  }

  v4l2_frame_params.frame_width_minus_1 = frame_header.width - 1;
  v4l2_frame_params.frame_height_minus_1 = frame_header.height - 1;
  v4l2_frame_params.render_width_minus_1 = frame_header.render_width - 1;
  v4l2_frame_params.render_height_minus_1 = frame_header.render_height - 1;

  v4l2_frame_params.current_frame_id = frame_header.current_frame_id;
  v4l2_frame_params.primary_ref_frame = frame_header.primary_reference_frame;
  SafeArrayMemcpy(v4l2_frame_params.buffer_removal_time,
                  frame_header.buffer_removal_time);
  v4l2_frame_params.refresh_frame_flags = frame_header.refresh_frame_flags;

  // TODO(b/248602457): Enable code for |order_hints| setup
  // after |ref_order_hint| maintenance is implemented.

  // These params looks duplicated with |ref_frame_idx|, but they are required
  // and used when |frame_refs_short_signaling| is set according to the AV1
  // spec. https://aomediacodec.github.io/av1-spec/#uncompressed-header-syntax
  v4l2_frame_params.last_frame_idx =
      frame_header.reference_frame_index[libgav1::kReferenceFrameLast];
  v4l2_frame_params.gold_frame_idx =
      frame_header.reference_frame_index[libgav1::kReferenceFrameGolden];

  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; ++i) {
    const auto* v4l2_ref_pic =
        static_cast<const V4L2AV1Picture*>(ref_frames[i].get());

    v4l2_frame_params.reference_frame_ts[i] =
        v4l2_ref_pic->dec_surface()->GetReferenceID();
  }

  static_assert(std::size(decltype(v4l2_frame_params.ref_frame_idx){}) ==
                    libgav1::kNumInterReferenceFrameTypes,
                "Invalid size of |ref_frame_idx| array");
  for (size_t i = 0; i < libgav1::kNumInterReferenceFrameTypes; i++)
    v4l2_frame_params.ref_frame_idx[i] =
        base::checked_cast<__u8>(frame_header.reference_frame_index[i]);

  v4l2_frame_params.skip_mode_frame[0] =
      base::checked_cast<__u8>(frame_header.skip_mode_frame[0]);
  v4l2_frame_params.skip_mode_frame[1] =
      base::checked_cast<__u8>(frame_header.skip_mode_frame[1]);

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
