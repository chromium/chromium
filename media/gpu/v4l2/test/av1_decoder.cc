// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/av1_decoder.h"

#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/base/video_types.h"
#include "media/gpu/macros.h"
#include "media/gpu/v4l2/test/upstream_pix_fmt.h"
#include "media/parsers/ivf_parser.h"
#include "third_party/libgav1/src/src/warp_prediction.h"

namespace media {

namespace v4l2_test {

namespace {
constexpr uint32_t kDriverCodecFourcc = V4L2_PIX_FMT_AV1_FRAME;

constexpr uint32_t kNumberOfBuffersInCaptureQueue = 10;

static_assert(kNumberOfBuffersInCaptureQueue <= 16,
              "Too many CAPTURE buffers are used. The number of CAPTURE "
              "buffers is currently assumed to be no larger than 16.");

// TODO(stevecho): Remove this provision when av1-ctrls.h includes linux/bits.h.
#ifndef BIT
#define BIT(nr) (1U << (nr))
#endif

inline void conditionally_set_flags(__u8* flags,
                                    const bool condition,
                                    const __u8 mask) {
  *flags |= (condition ? mask : 0);
}

inline void conditionally_set_u32_flags(__u32* flags,
                                        const bool condition,
                                        const __u32 mask) {
  *flags |= (condition ? mask : 0);
}

// The resolution encoded in the bitstream is required for queue creation. Note
// that parsing ivf file and parsing the first frame using libgav1 parser happen
// again later in the code. This is intentionally duplicated.
const gfx::Size GetResolutionFromBitstream(
    const base::MemoryMappedFile& stream) {
  media::IvfParser ivf_parser{};
  media::IvfFileHeader ivf_file_header{};

  if (!ivf_parser.Initialize(stream.data(), stream.length(), &ivf_file_header))
    LOG(FATAL) << "Couldn't initialize IVF parser.";

  IvfFrameHeader ivf_frame_header{};
  const uint8_t* ivf_frame_data = nullptr;

  if (!ivf_parser.ParseNextFrame(&ivf_frame_header, &ivf_frame_data))
    LOG(FATAL) << "Failed to parse the first frame with IVF parser.";

  VLOG(2) << "Ivf file header: " << ivf_file_header.width << " x "
          << ivf_file_header.height;

  libgav1::InternalFrameBufferList buffer_list;
  libgav1::BufferPool buffer_pool(libgav1::OnInternalFrameBufferSizeChanged,
                                  libgav1::GetInternalFrameBuffer,
                                  libgav1::ReleaseInternalFrameBuffer,
                                  &buffer_list);
  libgav1::DecoderState decoder_state;
  libgav1::ObuParser av1_parser(ivf_frame_data, ivf_frame_header.frame_size, 0,
                                &buffer_pool, &decoder_state);
  libgav1::RefCountedBufferPtr first_frame;

  if (!av1_parser.HasData())
    LOG(FATAL) << "Libgav1 parser doesn't have any data to parse.";

  if (av1_parser.ParseOneFrame(&first_frame) != libgav1::kStatusOk)
    LOG(FATAL) << "Failed to parse the first frame using libgav1 parser.";

  LOG(INFO) << "Frame header: " << av1_parser.frame_header().width << " x "
            << av1_parser.frame_header().height;

  return gfx::Size(av1_parser.frame_header().width,
                   av1_parser.frame_header().height);
}

// Section 5.5. Sequence header OBU syntax in the AV1 spec.
// https://aomediacodec.github.io/av1-spec/av1-spec.pdf
void FillSequenceParams(
    struct v4l2_ctrl_av1_sequence* v4l2_seq_params,
    const std::optional<libgav1::ObuSequenceHeader>& seq_header) {
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->still_picture,
                              V4L2_AV1_SEQUENCE_FLAG_STILL_PICTURE);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->use_128x128_superblock,
                              V4L2_AV1_SEQUENCE_FLAG_USE_128X128_SUPERBLOCK);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_filter_intra,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_FILTER_INTRA);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_intra_edge_filter,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTRA_EDGE_FILTER);
  conditionally_set_u32_flags(
      &v4l2_seq_params->flags, seq_header->enable_interintra_compound,
      V4L2_AV1_SEQUENCE_FLAG_ENABLE_INTERINTRA_COMPOUND);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_masked_compound,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_MASKED_COMPOUND);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_warped_motion,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_WARPED_MOTION);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_dual_filter,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_DUAL_FILTER);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_order_hint,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_ORDER_HINT);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_jnt_comp,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_JNT_COMP);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_ref_frame_mvs,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_REF_FRAME_MVS);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_superres,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_SUPERRES);
  conditionally_set_u32_flags(&v4l2_seq_params->flags, seq_header->enable_cdef,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_CDEF);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->enable_restoration,
                              V4L2_AV1_SEQUENCE_FLAG_ENABLE_RESTORATION);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->color_config.is_monochrome,
                              V4L2_AV1_SEQUENCE_FLAG_MONO_CHROME);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->color_config.color_range,
                              V4L2_AV1_SEQUENCE_FLAG_COLOR_RANGE);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->color_config.subsampling_x,
                              V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_X);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->color_config.subsampling_y,
                              V4L2_AV1_SEQUENCE_FLAG_SUBSAMPLING_Y);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->film_grain_params_present,
                              V4L2_AV1_SEQUENCE_FLAG_FILM_GRAIN_PARAMS_PRESENT);
  conditionally_set_u32_flags(&v4l2_seq_params->flags,
                              seq_header->color_config.separate_uv_delta_q,
                              V4L2_AV1_SEQUENCE_FLAG_SEPARATE_UV_DELTA_Q);

  v4l2_seq_params->seq_profile = seq_header->profile;
  v4l2_seq_params->order_hint_bits = seq_header->order_hint_bits;
  v4l2_seq_params->bit_depth = seq_header->color_config.bitdepth;
  v4l2_seq_params->max_frame_width_minus_1 = seq_header->max_frame_width - 1;
  v4l2_seq_params->max_frame_height_minus_1 = seq_header->max_frame_height - 1;
}

// Section 5.9.11. Loop filter params syntax.
// Note that |update_ref_delta| and |update_mode_delta| flags in the spec
// are not needed for V4L2 AV1 API.
void FillLoopFilterParams(struct v4l2_av1_loop_filter* v4l2_lf,
                          const libgav1::LoopFilter& lf) {
  conditionally_set_flags(&v4l2_lf->flags, lf.delta_enabled,
                          V4L2_AV1_LOOP_FILTER_FLAG_DELTA_ENABLED);
  conditionally_set_flags(&v4l2_lf->flags, lf.delta_update,
                          V4L2_AV1_LOOP_FILTER_FLAG_DELTA_UPDATE);

  static_assert(std::size(decltype(v4l2_lf->level){}) == libgav1::kFrameLfCount,
                "Invalid size of loop filter level (strength) array");
  for (size_t i = 0; i < libgav1::kFrameLfCount; i++)
    v4l2_lf->level[i] = base::checked_cast<__u8>(lf.level[i]);

  v4l2_lf->sharpness = lf.sharpness;

  static_assert(std::size(decltype(v4l2_lf->ref_deltas){}) ==
                    libgav1::kNumReferenceFrameTypes,
                "Invalid size of ref deltas array");
  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; i++)
    v4l2_lf->ref_deltas[i] = lf.ref_deltas[i];

  static_assert(std::size(decltype(v4l2_lf->mode_deltas){}) ==
                    libgav1::kLoopFilterMaxModeDeltas,
                "Invalid size of mode deltas array");
  for (size_t i = 0; i < libgav1::kLoopFilterMaxModeDeltas; i++)
    v4l2_lf->mode_deltas[i] = lf.mode_deltas[i];
}

// Section 5.9.18. Loop filter delta parameters syntax.
// Note that |delta_lf_res| in |v4l2_av1_loop_filter| corresponds to
// |delta_lf.scale| in the frame header defined in libgav1.
void FillLoopFilterDeltaParams(struct v4l2_av1_loop_filter* v4l2_lf,
                               const libgav1::Delta& delta_lf) {
  conditionally_set_flags(&v4l2_lf->flags, delta_lf.present,
                          V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_PRESENT);
  conditionally_set_flags(&v4l2_lf->flags, delta_lf.multi,
                          V4L2_AV1_LOOP_FILTER_FLAG_DELTA_LF_MULTI);

  v4l2_lf->delta_lf_res = delta_lf.scale;
}

// Section 5.9.12. Quantization params syntax
void FillQuantizationParams(struct v4l2_av1_quantization* v4l2_quant,
                            const libgav1::QuantizerParameters& quant) {
  conditionally_set_flags(&v4l2_quant->flags, quant.use_matrix,
                          V4L2_AV1_QUANTIZATION_FLAG_USING_QMATRIX);

  v4l2_quant->base_q_idx = quant.base_index;

  // Note that quant.delta_ac[0] is useless as it is always 0 according to
  // libgav1.
  v4l2_quant->delta_q_y_dc = quant.delta_dc[0];

  v4l2_quant->delta_q_u_dc = quant.delta_dc[1];
  v4l2_quant->delta_q_u_ac = quant.delta_ac[1];

  v4l2_quant->delta_q_v_dc = quant.delta_dc[2];
  v4l2_quant->delta_q_v_ac = quant.delta_ac[2];

  if (!quant.use_matrix)
    return;

  v4l2_quant->qm_y = base::checked_cast<uint8_t>(quant.matrix_level[0]);
  v4l2_quant->qm_u = base::checked_cast<uint8_t>(quant.matrix_level[1]);
  v4l2_quant->qm_v = base::checked_cast<uint8_t>(quant.matrix_level[2]);
}

// Section 5.9.17. Quantizer index delta parameters syntax
void FillQuantizerIndexDeltaParams(
    struct v4l2_av1_quantization* v4l2_quant,
    const std::optional<libgav1::ObuSequenceHeader>& seq_header,
    const libgav1::ObuFrameHeader& frm_header) {
  // |diff_uv_delta| in the spec doesn't exist in libgav1,
  // because libgav1 infers it using the following logic.
  const bool diff_uv_delta = (frm_header.quantizer.base_index != 0) &&
                             (!seq_header->color_config.is_monochrome) &&
                             (seq_header->color_config.separate_uv_delta_q);
  conditionally_set_flags(&v4l2_quant->flags, diff_uv_delta,
                          V4L2_AV1_QUANTIZATION_FLAG_DIFF_UV_DELTA);

  conditionally_set_flags(&v4l2_quant->flags, frm_header.delta_q.present,
                          V4L2_AV1_QUANTIZATION_FLAG_DELTA_Q_PRESENT);

  // |scale| is used to store |delta_q_res| value. This is because libgav1 uses
  // the same struct |Delta| both for quantizer index delta parameters and loop
  // filter delta parameters.
  v4l2_quant->delta_q_res = frm_header.delta_q.scale;
}

// Section 5.9.14. Segmentation params syntax
void FillSegmentationParams(struct v4l2_av1_segmentation* v4l2_seg,
                            const libgav1::Segmentation& seg) {
  conditionally_set_flags(&v4l2_seg->flags, seg.enabled,
                          V4L2_AV1_SEGMENTATION_FLAG_ENABLED);
  conditionally_set_flags(&v4l2_seg->flags, seg.update_map,
                          V4L2_AV1_SEGMENTATION_FLAG_UPDATE_MAP);
  conditionally_set_flags(&v4l2_seg->flags, seg.temporal_update,
                          V4L2_AV1_SEGMENTATION_FLAG_TEMPORAL_UPDATE);
  conditionally_set_flags(&v4l2_seg->flags, seg.update_data,
                          V4L2_AV1_SEGMENTATION_FLAG_UPDATE_DATA);
  conditionally_set_flags(&v4l2_seg->flags, seg.segment_id_pre_skip,
                          V4L2_AV1_SEGMENTATION_FLAG_SEG_ID_PRE_SKIP);

  static_assert(
      std::size(decltype(v4l2_seg->feature_enabled){}) == libgav1::kMaxSegments,
      "Invalid size of |feature_enabled| array in |v4l2_av1_segmentation| "
      "struct");

  static_assert(
      std::size(decltype(v4l2_seg->feature_data){}) == libgav1::kMaxSegments &&
          std::extent<decltype(v4l2_seg->feature_data), 0>::value ==
              libgav1::kSegmentFeatureMax,
      "Invalid size of |feature_data| array in |v4l2_av1_segmentation| struct");

  for (size_t i = 0; i < libgav1::kMaxSegments; ++i) {
    for (size_t j = 0; j < libgav1::kSegmentFeatureMax; ++j) {
      v4l2_seg->feature_enabled[i] |= (seg.feature_enabled[i][j] << j);
      v4l2_seg->feature_data[i][j] = seg.feature_data[i][j];
    }
  }

  v4l2_seg->last_active_seg_id = seg.last_active_segment_id;
}

// Section 5.9.19. CDEF params syntax
void FillCdefParams(struct v4l2_av1_cdef* v4l2_cdef,
                    const libgav1::Cdef& cdef,
                    uint8_t color_bitdepth) {
  // Damping value parsed in libgav1 is from the spec + (|color_bitdepth| - 8).
  CHECK_GE(color_bitdepth, 8u);
  const uint8_t coeff_shift = color_bitdepth - 8u;

  v4l2_cdef->damping_minus_3 =
      base::checked_cast<uint8_t>(cdef.damping - coeff_shift - 3u);

  v4l2_cdef->bits = cdef.bits;

  static_assert(std::size(decltype(v4l2_cdef->y_pri_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef y_pri_strength strength");

  static_assert(std::size(decltype(v4l2_cdef->y_sec_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef y_sec_strength strength");

  static_assert(std::size(decltype(v4l2_cdef->uv_pri_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef uv_pri_strength strength");

  static_assert(std::size(decltype(v4l2_cdef->uv_sec_strength){}) ==
                    libgav1::kMaxCdefStrengths,
                "Invalid size of cdef uv_sec_strength strength");

  SafeArrayMemcpy(v4l2_cdef->y_pri_strength, cdef.y_primary_strength);
  SafeArrayMemcpy(v4l2_cdef->y_sec_strength, cdef.y_secondary_strength);
  SafeArrayMemcpy(v4l2_cdef->uv_pri_strength, cdef.uv_primary_strength);
  SafeArrayMemcpy(v4l2_cdef->uv_sec_strength, cdef.uv_secondary_strength);

  // All the strength values parsed in libgav1 are from the AV1 spec and left
  // shifted by (|color_bitdepth| - 8). So these values need to be right shifted
  // by (|color_bitdepth| - 8) before passing to a driver.
  for (size_t i = 0; i < libgav1::kMaxCdefStrengths; i++) {
    v4l2_cdef->y_pri_strength[i] >>= coeff_shift;
    v4l2_cdef->y_sec_strength[i] >>= coeff_shift;
    v4l2_cdef->uv_pri_strength[i] >>= coeff_shift;
    v4l2_cdef->uv_sec_strength[i] >>= coeff_shift;
  }
}

// 5.9.20. Loop restoration params syntax
void FillLoopRestorationParams(v4l2_av1_loop_restoration* v4l2_lr,
                               const libgav1::LoopRestoration& lr) {
  for (size_t i = 0; i < V4L2_AV1_NUM_PLANES_MAX; i++) {
    switch (lr.type[i]) {
      case libgav1::LoopRestorationType::kLoopRestorationTypeNone:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_NONE;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeWiener:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_WIENER;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSgrProj:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_SGRPROJ;
        break;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSwitchable:
        v4l2_lr->frame_restoration_type[i] = V4L2_AV1_FRAME_RESTORE_SWITCHABLE;
        break;
      default:
        NOTREACHED_IN_MIGRATION() << "Invalid loop restoration type";
    }

    if (v4l2_lr->frame_restoration_type[i] != V4L2_AV1_FRAME_RESTORE_NONE) {
      conditionally_set_flags(&v4l2_lr->flags, true,
                              V4L2_AV1_LOOP_RESTORATION_FLAG_USES_LR);

      conditionally_set_flags(&v4l2_lr->flags, i > 0,
                              V4L2_AV1_LOOP_RESTORATION_FLAG_USES_CHROMA_LR);
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
  v4l2_lr->lr_unit_shift = lr.unit_size_log2[0] - 6;
  v4l2_lr->lr_uv_shift = lr.unit_size_log2[0] - lr.unit_size_log2[1];

  constexpr uint32_t kAv1RestorationTileSizeMax = 256;

  // AV1 spec (p.52) uses this formula with hard coded value 2.
  v4l2_lr->loop_restoration_size[0] =
      kAv1RestorationTileSizeMax >> (2 - v4l2_lr->lr_unit_shift);
  v4l2_lr->loop_restoration_size[1] =
      v4l2_lr->loop_restoration_size[0] >> v4l2_lr->lr_uv_shift;
  v4l2_lr->loop_restoration_size[2] =
      v4l2_lr->loop_restoration_size[0] >> v4l2_lr->lr_uv_shift;
}

// Section 5.9.15. Tile info syntax
void FillTileInfo(v4l2_av1_tile_info* v4l2_ti, const libgav1::TileInfo& ti) {
  conditionally_set_flags(&v4l2_ti->flags, ti.uniform_spacing,
                          V4L2_AV1_TILE_INFO_FLAG_UNIFORM_TILE_SPACING);
  static_assert(std::size(decltype(v4l2_ti->mi_col_starts){}) ==
                    (libgav1::kMaxTileColumns + 1),
                "Size of |mi_col_starts| array in |v4l2_av1_tile_info| struct "
                "does not match libgav1 expectation");

  for (size_t i = 0; i < libgav1::kMaxTileColumns + 1; i++)
    v4l2_ti->mi_col_starts[i] =
        base::checked_cast<uint32_t>(ti.tile_column_start[i]);

  static_assert(std::size(decltype(v4l2_ti->mi_row_starts){}) ==
                    (libgav1::kMaxTileRows + 1),
                "Size of |mi_row_starts| array in |v4l2_av1_tile_info| struct "
                "does not match libgav1 expectation");
  for (size_t i = 0; i < libgav1::kMaxTileRows + 1; i++)
    v4l2_ti->mi_row_starts[i] =
        base::checked_cast<uint32_t>(ti.tile_row_start[i]);

  if (!ti.uniform_spacing) {
    // Confirmed that |kMaxTileColumns| is enough size for
    // |width_in_sbs_minus_1| and |kMaxTileRows| is enough size for
    // |height_in_sbs_minus_1|
    // https://b.corp.google.com/issues/187828854#comment19
    static_assert(
        std::size(decltype(v4l2_ti->width_in_sbs_minus_1){}) ==
            libgav1::kMaxTileColumns,
        "Size of |width_in_sbs_minus_1| array in |v4l2_av1_tile_info| struct "
        "does not match libgav1 expectation");
    for (size_t i = 0; i < libgav1::kMaxTileColumns; i++) {
      if (ti.tile_column_width_in_superblocks[i] >= 1) {
        v4l2_ti->width_in_sbs_minus_1[i] = base::checked_cast<uint32_t>(
            ti.tile_column_width_in_superblocks[i] - 1);
      }
    }

    static_assert(
        std::size(decltype(v4l2_ti->height_in_sbs_minus_1){}) ==
            libgav1::kMaxTileRows,
        "Size of |height_in_sbs_minus_1| array in |v4l2_av1_tile_info| struct "
        "does not match libgav1 expectation");
    for (size_t i = 0; i < libgav1::kMaxTileRows; i++) {
      if (ti.tile_row_height_in_superblocks[i] >= 1) {
        v4l2_ti->height_in_sbs_minus_1[i] = base::checked_cast<uint32_t>(
            ti.tile_row_height_in_superblocks[i] - 1);
      }
    }
  }

  v4l2_ti->tile_size_bytes = ti.tile_size_bytes;
  v4l2_ti->context_update_tile_id = ti.context_update_id;
  v4l2_ti->tile_cols = ti.tile_columns;
  v4l2_ti->tile_rows = ti.tile_rows;
}

// Section 5.9.24. Global motion params syntax
void FillGlobalMotionParams(
    v4l2_av1_global_motion* v4l2_gm,
    const std::array<libgav1::GlobalMotion, libgav1::kNumReferenceFrameTypes>&
        gm_array) {
  // gm_array[0] (for kReferenceFrameIntra) is not used because global motion is
  // not relevant for intra frames
  for (size_t i = 1; i < libgav1::kNumReferenceFrameTypes; ++i) {
    auto gm = gm_array[i];
    switch (gm.type) {
      case libgav1::kGlobalMotionTransformationTypeIdentity:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_IDENTITY;
        break;
      case libgav1::kGlobalMotionTransformationTypeTranslation:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_TRANSLATION;
        conditionally_set_flags(&v4l2_gm->flags[i], true,
                                V4L2_AV1_GLOBAL_MOTION_FLAG_IS_TRANSLATION);
        break;
      case libgav1::kGlobalMotionTransformationTypeRotZoom:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_ROTZOOM;
        conditionally_set_flags(&v4l2_gm->flags[i], true,
                                V4L2_AV1_GLOBAL_MOTION_FLAG_IS_ROT_ZOOM);
        break;
      case libgav1::kGlobalMotionTransformationTypeAffine:
        v4l2_gm->type[i] = V4L2_AV1_WARP_MODEL_AFFINE;
        conditionally_set_flags(&v4l2_gm->flags[i], true,
                                V4L2_AV1_WARP_MODEL_AFFINE);
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Invalid global motion transformation type, "
            << v4l2_gm->type[i];
    }

    conditionally_set_flags(
        &v4l2_gm->flags[i],
        gm.type != libgav1::kGlobalMotionTransformationTypeIdentity,
        V4L2_AV1_GLOBAL_MOTION_FLAG_IS_GLOBAL);

    constexpr auto kNumGlobalMotionParams = std::size(decltype(gm.params){});

    for (size_t j = 0; j < kNumGlobalMotionParams; ++j) {
      static_assert(
          std::is_same<decltype(v4l2_gm->params[0][0]), int32_t&>::value,
          "|v4l2_av1_global_motion::params|'s data type must be int32_t "
          "starting from AV1 uAPI v4");

      v4l2_gm->params[i][j] = gm.params[j];
    }

    conditionally_set_flags(&v4l2_gm->invalid, !libgav1::SetupShear(&gm),
                            V4L2_AV1_GLOBAL_MOTION_IS_INVALID(i));
  }
}

// Section 5.11. Tile Group OBU syntax
void FillTileGroupParams(
    std::vector<struct v4l2_ctrl_av1_tile_group_entry>*
        tile_group_entry_vectors,
    const base::span<const uint8_t> frame_obu_data,
    const libgav1::TileInfo& tile_info,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers) {
  // TODO(stevecho): This could happen in rare cases (for example, if there is a
  // Metadata OBU after the TileGroup OBU). We currently do not have a reason to
  // handle those cases. This is also the case in libgav1 at the moment.
  CHECK(!tile_buffers.empty());
  const size_t tile_columns = tile_info.tile_columns;

  CHECK_GT(tile_columns, 0u);
  const uint16_t num_tiles = base::checked_cast<uint16_t>(tile_buffers.size());

  for (uint16_t tile_index = 0; tile_index < num_tiles; ++tile_index) {
    struct v4l2_ctrl_av1_tile_group_entry tile_group_entry_params = {};

    CHECK(tile_buffers[tile_index].data >= frame_obu_data.data());
    tile_group_entry_params.tile_offset = base::checked_cast<uint32_t>(
        tile_buffers[tile_index].data - frame_obu_data.data());

    tile_group_entry_params.tile_size = tile_buffers[tile_index].size;

    // The tiles are row-major. We use the number of columns |tile_columns|
    // to compute computation of the row and column for a given tile.
    tile_group_entry_params.tile_row =
        tile_index / base::checked_cast<uint16_t>(tile_columns);
    tile_group_entry_params.tile_col =
        tile_index % base::checked_cast<uint16_t>(tile_columns);

    tile_group_entry_vectors->push_back(tile_group_entry_params);
  }
}

}  // namespace

Av1Decoder::Av1Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       std::unique_ptr<V4L2IoctlShim> v4l2_ioctl,
                       gfx::Size display_resolution)
    : VideoDecoder::VideoDecoder(std::move(v4l2_ioctl), display_resolution),
      ivf_parser_(std::move(ivf_parser)),
      buffer_pool_(std::make_unique<libgav1::BufferPool>(
          /*on_frame_buffer_size_changed=*/nullptr,
          /*get_frame_buffer=*/nullptr,
          /*release_frame_buffer=*/nullptr,
          /*callback_private_data=*/nullptr)),
      state_(std::make_unique<libgav1::DecoderState>()) {}

Av1Decoder::~Av1Decoder() {
  // We destroy the state explicitly to ensure it's destroyed before the
  // |buffer_pool_|. The |buffer_pool_| checks that all the allocated frames
  // are released in its destructor.
  state_.reset();
  DCHECK(buffer_pool_);
}

// static
std::unique_ptr<Av1Decoder> Av1Decoder::Create(
    const base::MemoryMappedFile& stream) {
  VLOG(2) << "Attempting to create decoder with codec "
          << media::FourccToString(kDriverCodecFourcc);

  // Set up video parser.
  auto ivf_parser = std::make_unique<media::IvfParser>();
  media::IvfFileHeader file_header{};

  if (!ivf_parser->Initialize(stream.data(), stream.length(), &file_header)) {
    LOG(ERROR) << "Couldn't initialize IVF parser";
    return nullptr;
  }

  const auto driver_codec_fourcc =
      media::v4l2_test::FileFourccToDriverFourcc(file_header.fourcc);

  if (driver_codec_fourcc != kDriverCodecFourcc) {
    VLOG(2) << "File fourcc (" << media::FourccToString(driver_codec_fourcc)
            << ") does not match expected fourcc("
            << media::FourccToString(kDriverCodecFourcc) << ").";
    return nullptr;
  }

  auto v4l2_ioctl = std::make_unique<V4L2IoctlShim>(kDriverCodecFourcc);

  const gfx::Size bitstream_coded_size = GetResolutionFromBitstream(stream);

  return base::WrapUnique(new Av1Decoder(
      std::move(ivf_parser), std::move(v4l2_ioctl), bitstream_coded_size));
}

Av1Decoder::ParsingResult Av1Decoder::ReadNextFrame(
    libgav1::RefCountedBufferPtr& current_frame) {
  if (!obu_parser_ || !obu_parser_->HasData()) {
    if (!ivf_parser_->ParseNextFrame(&ivf_frame_header_, &ivf_frame_data_))
      return ParsingResult::kEOStream;

    // The ObuParser has run out of data or did not exist in the first place. It
    // has no "replace the current buffer with a new buffer of a different size"
    // method; we must make a new parser.
    // (std::nothrow) is required for the base class Allocable of
    // libgav1::ObuParser
    obu_parser_ = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
        ivf_frame_data_, ivf_frame_header_.frame_size, /*operating_point=*/0,
        buffer_pool_.get(), state_.get()));
    if (current_sequence_header_)
      obu_parser_->set_sequence_header(*current_sequence_header_);
  }

  const libgav1::StatusCode code = obu_parser_->ParseOneFrame(&current_frame);
  if (code != libgav1::kStatusOk) {
    LOG(ERROR) << "Error parsing OBU stream: " << libgav1::GetErrorString(code);
    return ParsingResult::kFailed;
  }
  return ParsingResult::kOk;
}

void Av1Decoder::CopyFrameData(const libgav1::ObuFrameHeader& frame_hdr,
                               std::unique_ptr<V4L2Queue>& queue) {
  CHECK_EQ(queue->num_buffers(), 1u)
      << "Only 1 buffer is expected to be used for OUTPUT queue for now.";

  CHECK_EQ(queue->num_planes(), 1u)
      << "Number of planes is expected to be 1 for OUTPUT queue.";

  scoped_refptr<MmappedBuffer> buffer = queue->GetBuffer(0);

  buffer->mmapped_planes()[0].CopyIn(ivf_frame_data_,
                                     ivf_frame_header_.frame_size);
}

// 5.9.2. Uncompressed header syntax
void Av1Decoder::SetupFrameParams(
    struct v4l2_ctrl_av1_frame* v4l2_frame_params,
    const std::optional<libgav1::ObuSequenceHeader>& seq_header,
    const libgav1::ObuFrameHeader& frm_header) {
  FillLoopFilterParams(&v4l2_frame_params->loop_filter, frm_header.loop_filter);

  FillLoopFilterDeltaParams(&v4l2_frame_params->loop_filter,
                            frm_header.delta_lf);

  FillQuantizationParams(&v4l2_frame_params->quantization,
                         frm_header.quantizer);

  FillQuantizerIndexDeltaParams(&v4l2_frame_params->quantization, seq_header,
                                frm_header);

  FillSegmentationParams(&v4l2_frame_params->segmentation,
                         frm_header.segmentation);

  const auto color_bitdepth = seq_header->color_config.bitdepth;
  FillCdefParams(&v4l2_frame_params->cdef, frm_header.cdef,
                 base::strict_cast<int8_t>(color_bitdepth));

  FillLoopRestorationParams(&v4l2_frame_params->loop_restoration,
                            frm_header.loop_restoration);

  FillTileInfo(&v4l2_frame_params->tile_info, frm_header.tile_info);

  FillGlobalMotionParams(&v4l2_frame_params->global_motion,
                         frm_header.global_motion);

  conditionally_set_u32_flags(&v4l2_frame_params->flags, frm_header.show_frame,
                              V4L2_AV1_FRAME_FLAG_SHOW_FRAME);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.showable_frame,
                              V4L2_AV1_FRAME_FLAG_SHOWABLE_FRAME);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.error_resilient_mode,
                              V4L2_AV1_FRAME_FLAG_ERROR_RESILIENT_MODE);
  // libgav1 header has |enable_cdf_update| instead of |disable_cdf_update|.
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              !frm_header.enable_cdf_update,
                              V4L2_AV1_FRAME_FLAG_DISABLE_CDF_UPDATE);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.allow_screen_content_tools,
                              V4L2_AV1_FRAME_FLAG_ALLOW_SCREEN_CONTENT_TOOLS);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.force_integer_mv,
                              V4L2_AV1_FRAME_FLAG_FORCE_INTEGER_MV);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.allow_intrabc,
                              V4L2_AV1_FRAME_FLAG_ALLOW_INTRABC);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.use_superres,
                              V4L2_AV1_FRAME_FLAG_USE_SUPERRES);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.allow_high_precision_mv,
                              V4L2_AV1_FRAME_FLAG_ALLOW_HIGH_PRECISION_MV);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.is_motion_mode_switchable,
                              V4L2_AV1_FRAME_FLAG_IS_MOTION_MODE_SWITCHABLE);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.use_ref_frame_mvs,
                              V4L2_AV1_FRAME_FLAG_USE_REF_FRAME_MVS);
  // libgav1 header has |enable_frame_end_update_cdf| instead.
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              !frm_header.enable_frame_end_update_cdf,
                              V4L2_AV1_FRAME_FLAG_DISABLE_FRAME_END_UPDATE_CDF);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.allow_warped_motion,
                              V4L2_AV1_FRAME_FLAG_ALLOW_WARPED_MOTION);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.reference_mode_select,
                              V4L2_AV1_FRAME_FLAG_REFERENCE_SELECT);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.reduced_tx_set,
                              V4L2_AV1_FRAME_FLAG_REDUCED_TX_SET);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.skip_mode_frame[0] > 0,
                              V4L2_AV1_FRAME_FLAG_SKIP_MODE_ALLOWED);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.skip_mode_present,
                              V4L2_AV1_FRAME_FLAG_SKIP_MODE_PRESENT);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.frame_size_override_flag,
                              V4L2_AV1_FRAME_FLAG_FRAME_SIZE_OVERRIDE);
  // libgav1 header doesn't have |buffer_removal_time_present_flag|.
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.buffer_removal_time[0] > 0,
                              V4L2_AV1_FRAME_FLAG_BUFFER_REMOVAL_TIME_PRESENT);
  conditionally_set_u32_flags(&v4l2_frame_params->flags,
                              frm_header.frame_refs_short_signaling,
                              V4L2_AV1_FRAME_FLAG_FRAME_REFS_SHORT_SIGNALING);

  switch (frm_header.frame_type) {
    case libgav1::kFrameKey:
      v4l2_frame_params->frame_type = V4L2_AV1_KEY_FRAME;
      break;
    case libgav1::kFrameInter:
      v4l2_frame_params->frame_type = V4L2_AV1_INTER_FRAME;
      break;
    case libgav1::kFrameIntraOnly:
      v4l2_frame_params->frame_type = V4L2_AV1_INTRA_ONLY_FRAME;
      break;
    case libgav1::kFrameSwitch:
      v4l2_frame_params->frame_type = V4L2_AV1_SWITCH_FRAME;
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Invalid frame type, " << frm_header.frame_type;
  }

  v4l2_frame_params->order_hint = frm_header.order_hint;
  v4l2_frame_params->superres_denom = frm_header.superres_scale_denominator;
  v4l2_frame_params->upscaled_width = frm_header.upscaled_width;

  switch (frm_header.interpolation_filter) {
    case libgav1::kInterpolationFilterEightTap:
      v4l2_frame_params->interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP;
      break;
    case libgav1::kInterpolationFilterEightTapSmooth:
      v4l2_frame_params->interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH;
      break;
    case libgav1::kInterpolationFilterEightTapSharp:
      v4l2_frame_params->interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_EIGHTTAP_SHARP;
      break;
    case libgav1::kInterpolationFilterBilinear:
      v4l2_frame_params->interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_BILINEAR;
      break;
    case libgav1::kInterpolationFilterSwitchable:
      v4l2_frame_params->interpolation_filter =
          V4L2_AV1_INTERPOLATION_FILTER_SWITCHABLE;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid interpolation filter, "
                                << frm_header.interpolation_filter;
  }

  switch (frm_header.tx_mode) {
    case libgav1::kTxModeOnly4x4:
      v4l2_frame_params->tx_mode = V4L2_AV1_TX_MODE_ONLY_4X4;
      break;
    case libgav1::kTxModeLargest:
      v4l2_frame_params->tx_mode = V4L2_AV1_TX_MODE_LARGEST;
      break;
    case libgav1::kTxModeSelect:
      v4l2_frame_params->tx_mode = V4L2_AV1_TX_MODE_SELECT;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid tx mode, " << frm_header.tx_mode;
  }

  v4l2_frame_params->frame_width_minus_1 = frm_header.width - 1;
  v4l2_frame_params->frame_height_minus_1 = frm_header.height - 1;
  v4l2_frame_params->render_width_minus_1 = frm_header.render_width - 1;
  v4l2_frame_params->render_height_minus_1 = frm_header.render_height - 1;

  v4l2_frame_params->current_frame_id = frm_header.current_frame_id;
  v4l2_frame_params->primary_ref_frame = frm_header.primary_reference_frame;
  SafeArrayMemcpy(v4l2_frame_params->buffer_removal_time,
                  frm_header.buffer_removal_time);
  v4l2_frame_params->refresh_frame_flags = frm_header.refresh_frame_flags;

  if (frm_header.frame_type == libgav1::kFrameKey && frm_header.show_frame) {
    for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; i++)
      ref_order_hint_[i] = 0;
  }

  // The first slot in |order_hints| is reserved for intra frame, so it is not
  // used and will always be 0.
  static_assert(std::size(decltype(v4l2_frame_params->order_hints){}) ==
                    libgav1::kNumInterReferenceFrameTypes + 1,
                "Invalid size of |order_hints| array");
  if (!libgav1::IsIntraFrame(frm_header.frame_type)) {
    for (size_t i = 0; i < libgav1::kNumInterReferenceFrameTypes; i++) {
      v4l2_frame_params->order_hints[i + 1] =
          ref_order_hint_[frm_header.reference_frame_index[i]];
    }
  }

  // TODO(b/230891887): use uint64_t when v4l2_timeval_to_ns() function is used.
  constexpr uint32_t kInvalidSurface = std::numeric_limits<uint32_t>::max();

  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; ++i) {
    constexpr size_t kTimestampToNanoSecs = 1000;

    // |reference_frame_ts| is needed to use previously decoded frames
    // from reference frames list.
    const auto reference_frame_ts =
        ref_frames_[i] ? ref_frames_[i]->frame_number() * kTimestampToNanoSecs
                       : kInvalidSurface;

    v4l2_frame_params->reference_frame_ts[i] = reference_frame_ts;
  }

  static_assert(std::size(decltype(v4l2_frame_params->ref_frame_idx){}) ==
                    libgav1::kNumInterReferenceFrameTypes,
                "Invalid size of |ref_frame_idx| array");
  for (size_t i = 0; i < libgav1::kNumInterReferenceFrameTypes; i++) {
    static_assert(std::is_same<decltype(v4l2_frame_params->ref_frame_idx[0]),
                               int8_t&>::value,
                  "|v4l2_ctrl_av1_frame::ref_frame_idx|'s data type must be "
                  "int8_t starting from AV1 uAPI v4");

    v4l2_frame_params->ref_frame_idx[i] = frm_header.reference_frame_index[i];
  }

  v4l2_frame_params->skip_mode_frame[0] =
      base::checked_cast<__u8>(frm_header.skip_mode_frame[0]);
  v4l2_frame_params->skip_mode_frame[1] =
      base::checked_cast<__u8>(frm_header.skip_mode_frame[1]);
}

std::set<int> Av1Decoder::RefreshReferenceSlots(
    const libgav1::ObuFrameHeader& frame_hdr,
    const libgav1::RefCountedBufferPtr current_frame,
    const scoped_refptr<MmappedBuffer> buffer,
    const uint32_t last_queued_buffer_id) {
  state_->UpdateReferenceFrames(
      current_frame, base::strict_cast<int>(frame_hdr.refresh_frame_flags));

  static_assert(
      kAv1NumRefFrames == sizeof(frame_hdr.refresh_frame_flags) * CHAR_BIT,
      "|refresh_frame_flags| size must be equal to |kAv1NumRefFrames|");

  const std::bitset<kAv1NumRefFrames> refresh_frame_slots(
      frame_hdr.refresh_frame_flags);

  std::set<int> reusable_buffer_ids;

  constexpr uint8_t kRefreshFrameFlagsAll = 0xFF;
  // If |show_existing_frame| = 1 and the frame to show is a key frame, the
  // reference frame loading process as specified in section 7.21 of the AV1
  // spec is invoked.
  const bool is_show_existing_key_frame =
      (frame_hdr.show_existing_frame &&
       (state_->reference_frame[frame_hdr.frame_to_show]->frame_type() ==
        libgav1::kFrameKey));
  if (frame_hdr.refresh_frame_flags == kRefreshFrameFlagsAll ||
      is_show_existing_key_frame) {
    // After decoding a key frame, all CAPTURE buffers can be reused except the
    // CAPTURE buffer corresponding to the key frame.
    for (size_t i = 0; i < kNumberOfBuffersInCaptureQueue; i++)
      reusable_buffer_ids.insert(i);

    reusable_buffer_ids.erase(buffer->buffer_id());

    // Note that the CAPTURE buffer for previous frame can be used as well,
    // but it is already queued again at this point.
    reusable_buffer_ids.erase(last_queued_buffer_id);

    // Updates to assign current key frame as a reference frame for all
    // reference frame slots in the reference frames list.
    ref_frames_.fill(buffer);

    if (is_show_existing_key_frame) {
      for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; i++)
        ref_order_hint_[i] = ref_order_hint_[frame_hdr.frame_to_show];
    }

    return reusable_buffer_ids;
  }

  constexpr uint8_t kRefreshFrameFlagsNone = 0;
  if (frame_hdr.refresh_frame_flags == kRefreshFrameFlagsNone) {
    // Indicates to reuse currently decoded CAPTURE buffer.
    reusable_buffer_ids.insert(buffer->buffer_id());

    return reusable_buffer_ids;
  }

  // More than one slot in |refresh_frame_flags| can be set.
  for (size_t i = 0; i < kAv1NumRefFrames; i++) {
    if (!refresh_frame_slots[i])
      continue;

    // It is not required to check whether existing reference frame slot is
    // already pointing to a reference frame. This is because reference
    // frame slots are empty only after the first key frame decoding.
    const uint16_t reusable_candidate_buffer_id = ref_frames_[i]->buffer_id();
    reusable_buffer_ids.insert(reusable_candidate_buffer_id);

    // Checks to make sure |reusable_candidate_buffer_id| is not used in
    // different reference frame slots in the reference frames list. If
    // |reusable_candidate_buffer_id| is already being used, then it is no
    // longer qualified as a reusable buffer. Thus, it is removed from
    // |reusable_buffer_ids|.
    for (size_t j = 0; j < kAv1NumRefFrames; j++) {
      const bool is_refresh_slot_not_used = (refresh_frame_slots[j] == false);
      const bool is_candidate_used =
          (ref_frames_[j]->buffer_id() == reusable_candidate_buffer_id);

      if (is_refresh_slot_not_used && is_candidate_used) {
        reusable_buffer_ids.erase(reusable_candidate_buffer_id);
        break;
      }
    }
    ref_frames_[i] = buffer;
    ref_order_hint_[i] = frame_hdr.order_hint;
  }

  return reusable_buffer_ids;
}

void Av1Decoder::QueueReusableBuffersInCaptureQueue(
    const std::set<int> reusable_buffer_ids,
    const bool is_inter_frame) {
  for (const auto reusable_buffer_id : reusable_buffer_ids) {
    if (!v4l2_ioctl_->QBuf(CAPTURE_queue_, reusable_buffer_id))
      LOG(ERROR) << "VIDIOC_QBUF failed for CAPTURE queue.";

    if (is_inter_frame)
      CAPTURE_queue_->set_last_queued_buffer_id(reusable_buffer_id);
  }
}

VideoDecoder::Result Av1Decoder::DecodeNextFrame(const int frame_number,
                                                 std::vector<uint8_t>& y_plane,
                                                 std::vector<uint8_t>& u_plane,
                                                 std::vector<uint8_t>& v_plane,
                                                 gfx::Size& size,
                                                 BitDepth& bit_depth) {
  libgav1::RefCountedBufferPtr current_frame;
  const ParsingResult parser_res = ReadNextFrame(current_frame);

  if (parser_res != ParsingResult::kOk) {
    LOG_ASSERT(parser_res == ParsingResult::kEOStream)
        << "Failed to parse next frame.";
    return VideoDecoder::kEOStream;
  }

  const bool is_OUTPUT_queue_new = !OUTPUT_queue_;
  if (!OUTPUT_queue_) {
    CreateOUTPUTQueue(kDriverCodecFourcc);
  }

  libgav1::ObuFrameHeader current_frame_header = obu_parser_->frame_header();

  if (obu_parser_->sequence_header_changed())
    current_sequence_header_.emplace(obu_parser_->sequence_header());

  LOG_ASSERT(current_sequence_header_)
      << "Sequence header missing for decoding.";

  if (current_frame_header.show_existing_frame) {
    last_decoded_frame_visible_ = true;
  } else {
    last_decoded_frame_visible_ = current_frame_header.show_frame;
  }
  VLOG_IF(2, !last_decoded_frame_visible_) << "not displayed frame";

  for (size_t i = 0; i < kAv1NumRefFrames; ++i) {
    if (state_->reference_frame[i] != nullptr && ref_frames_[i] == nullptr) {
      LOG(FATAL) << "The state of the reference frames are different "
                    "between |ref_frames_| and |state_|";
    }
    if (state_->reference_frame[i] == nullptr && ref_frames_[i] != nullptr)
      ref_frames_[i].reset();
  }

  if (current_frame_header.show_existing_frame) {
    scoped_refptr<MmappedBuffer> repeated_frame_buffer =
        ref_frames_[current_frame_header.frame_to_show];

    bit_depth =
        ConvertToYUV(y_plane, u_plane, v_plane, OUTPUT_queue_->resolution(),
                     repeated_frame_buffer->mmapped_planes(),
                     CAPTURE_queue_->resolution(), CAPTURE_queue_->fourcc());

    // Repeated frames normally don't need to update reference frames. But in
    // this special case when the repeated frame is pointing to a key frame, all
    // the reference frames have to be updated to the key frame pointed by the
    // repeated frame.
    if (state_->reference_frame[current_frame_header.frame_to_show]
            ->frame_type() == libgav1::kFrameKey) {
      const std::set<int> reusable_buffer_ids =
          RefreshReferenceSlots(current_frame_header, current_frame,
                                ref_frames_[current_frame_header.frame_to_show],
                                CAPTURE_queue_->last_queued_buffer_id());

      QueueReusableBuffersInCaptureQueue(
          reusable_buffer_ids,
          !libgav1::IsIntraFrame(current_frame_header.frame_type));
    }

    return VideoDecoder::kOk;
  }

  CopyFrameData(current_frame_header, OUTPUT_queue_);

  LOG_ASSERT(OUTPUT_queue_->num_buffers() == 1)
      << "Too many buffers in OUTPUT queue. It is currently designed to "
         "support only 1 request at a time.";

  OUTPUT_queue_->GetBuffer(0)->set_frame_number(frame_number);

  if (!v4l2_ioctl_->QBuf(OUTPUT_queue_, 0))
    LOG(FATAL) << "VIDIOC_QBUF failed for OUTPUT queue.";

  std::vector<struct v4l2_ext_control> ext_ctrl_vectors;

  struct v4l2_ctrl_av1_sequence v4l2_seq_params = {};

  FillSequenceParams(&v4l2_seq_params, current_sequence_header_);

  ext_ctrl_vectors.push_back({.id = V4L2_CID_STATELESS_AV1_SEQUENCE,
                              .size = sizeof(v4l2_seq_params),
                              .ptr = &v4l2_seq_params});

  struct v4l2_ctrl_av1_frame v4l2_frame_params = {};

  SetupFrameParams(&v4l2_frame_params, current_sequence_header_,
                   current_frame_header);

  ext_ctrl_vectors.push_back({.id = V4L2_CID_STATELESS_AV1_FRAME,
                              .size = sizeof(v4l2_frame_params),
                              .ptr = &v4l2_frame_params});

  std::vector<struct v4l2_ctrl_av1_tile_group_entry> tile_group_entry_vectors;

  FillTileGroupParams(
      &tile_group_entry_vectors,
      base::make_span(ivf_frame_data_, ivf_frame_header_.frame_size),
      current_frame_header.tile_info, obu_parser_->tile_buffers());

  ext_ctrl_vectors.push_back({.id = V4L2_CID_STATELESS_AV1_TILE_GROUP_ENTRY,
                              .size = base::checked_cast<__u32>(
                                  tile_group_entry_vectors.size() *
                                  sizeof(v4l2_ctrl_av1_tile_group_entry)),
                              .ptr = &tile_group_entry_vectors[0]});

  struct v4l2_ext_controls ext_ctrls = {.count = base::checked_cast<__u32>(ext_ctrl_vectors.size()),
                                        .controls = &ext_ctrl_vectors[0]};

  // Before the CAPTURE queue is set up the first frame must be parsed by the
  // driver. This is done so that when VIDIOC_G_FMT is called the frame
  // dimensions and format will be ready. Specifying V4L2_CTRL_WHICH_CUR_VAL
  // when VIDIOC_S_EXT_CTRLS processes the request immediately so that the frame
  // is parsed by the driver and the state is readied.
  v4l2_ioctl_->SetExtCtrls(OUTPUT_queue_, &ext_ctrls, is_OUTPUT_queue_new);
  v4l2_ioctl_->MediaRequestIocQueue(OUTPUT_queue_);

  if (!CAPTURE_queue_) {
    CreateCAPTUREQueue(kNumberOfBuffersInCaptureQueue);
  }

  v4l2_ioctl_->WaitForRequestCompletion(OUTPUT_queue_);

  uint32_t buffer_id;
  v4l2_ioctl_->DQBuf(CAPTURE_queue_, &buffer_id);

  scoped_refptr<MmappedBuffer> buffer = CAPTURE_queue_->GetBuffer(buffer_id);
  bit_depth =
      ConvertToYUV(y_plane, u_plane, v_plane, OUTPUT_queue_->resolution(),
                   buffer->mmapped_planes(), CAPTURE_queue_->resolution(),
                   CAPTURE_queue_->fourcc());

  const std::set<int> reusable_buffer_ids = RefreshReferenceSlots(
      current_frame_header, current_frame, CAPTURE_queue_->GetBuffer(buffer_id),
      CAPTURE_queue_->last_queued_buffer_id());

  QueueReusableBuffersInCaptureQueue(
      reusable_buffer_ids,
      !libgav1::IsIntraFrame(current_frame_header.frame_type));

  v4l2_ioctl_->DQBuf(OUTPUT_queue_, &buffer_id);

  v4l2_ioctl_->MediaRequestIocReinit(OUTPUT_queue_);

  return VideoDecoder::kOk;
}

}  // namespace v4l2_test
}  // namespace media
