// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/av1_vaapi_video_decoder_delegate.h"

#include <string.h>
#include <va/va.h>

#include <algorithm>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "third_party/libgav1/src/src/utils/types.h"
#include "third_party/libgav1/src/src/warp_prediction.h"

namespace media {

using DecodeStatus = AV1Decoder::AV1Accelerator::Status;

namespace {

#define ARRAY_SIZE(ar) (sizeof(ar) / sizeof(ar[0]))
#define STD_ARRAY_SIZE(ar) (std::tuple_size<decltype(ar)>::value)

void FillSegmentInfo(VASegmentationStructAV1& va_seg_info,
                     const libgav1::Segmentation& segmentation) {
  auto& va_seg_info_fields = va_seg_info.segment_info_fields.bits;
  va_seg_info_fields.enabled = segmentation.enabled;
  va_seg_info_fields.update_map = segmentation.update_map;
  va_seg_info_fields.temporal_update = segmentation.temporal_update;
  va_seg_info_fields.update_data = segmentation.update_data;

  static_assert(libgav1::kMaxSegments == 8 && libgav1::kSegmentFeatureMax == 8,
                "Invalid Segment array size");
  static_assert(ARRAY_SIZE(segmentation.feature_data) == 8 &&
                    ARRAY_SIZE(segmentation.feature_data[0]) == 8 &&
                    ARRAY_SIZE(segmentation.feature_enabled) == 8 &&
                    ARRAY_SIZE(segmentation.feature_enabled[0]) == 8,
                "Invalid segmentation array size");
  static_assert(ARRAY_SIZE(va_seg_info.feature_data) == 8 &&
                    ARRAY_SIZE(va_seg_info.feature_data[0]) == 8 &&
                    ARRAY_SIZE(va_seg_info.feature_mask) == 8,
                "Invalid feature array size");
  for (size_t i = 0; i < libgav1::kMaxSegments; ++i) {
    for (size_t j = 0; j < libgav1::kSegmentFeatureMax; ++j)
      va_seg_info.feature_data[i][j] = segmentation.feature_data[i][j];
  }
  for (size_t i = 0; i < libgav1::kMaxSegments; ++i) {
    uint8_t feature_mask = 0;
    for (size_t j = 0; j < libgav1::kSegmentFeatureMax; ++j) {
      if (segmentation.feature_enabled[i][j])
        feature_mask |= 1 << j;
    }
    va_seg_info.feature_mask[i] = feature_mask;
  }
}

void FillFilmGrainInfo(VAFilmGrainStructAV1& va_film_grain_info,
                       const libgav1::FilmGrainParams& film_grain_params) {
  if (!film_grain_params.apply_grain)
    return;

#define COPY_FILM_GRAIN_FIELD(a) \
  va_film_grain_info.film_grain_info_fields.bits.a = film_grain_params.a
  COPY_FILM_GRAIN_FIELD(apply_grain);
  COPY_FILM_GRAIN_FIELD(chroma_scaling_from_luma);
  COPY_FILM_GRAIN_FIELD(grain_scale_shift);
  COPY_FILM_GRAIN_FIELD(overlap_flag);
  COPY_FILM_GRAIN_FIELD(clip_to_restricted_range);
#undef COPY_FILM_GRAIN_FIELD
  va_film_grain_info.film_grain_info_fields.bits.ar_coeff_lag =
      film_grain_params.auto_regression_coeff_lag;
  DCHECK_GE(film_grain_params.chroma_scaling, 8u);
  DCHECK_GE(film_grain_params.auto_regression_shift, 6u);
  va_film_grain_info.film_grain_info_fields.bits.grain_scaling_minus_8 =
      film_grain_params.chroma_scaling - 8;
  va_film_grain_info.film_grain_info_fields.bits.ar_coeff_shift_minus_6 =
      film_grain_params.auto_regression_shift - 6;

  constexpr size_t kFilmGrainPointYSize = 14;
  constexpr size_t kFilmGrainPointUVSize = 10;
  static_assert(
      ARRAY_SIZE(va_film_grain_info.point_y_value) == kFilmGrainPointYSize &&
          ARRAY_SIZE(va_film_grain_info.point_y_scaling) ==
              kFilmGrainPointYSize &&
          ARRAY_SIZE(va_film_grain_info.point_cb_value) ==
              kFilmGrainPointUVSize &&
          ARRAY_SIZE(va_film_grain_info.point_cb_scaling) ==
              kFilmGrainPointUVSize &&
          ARRAY_SIZE(va_film_grain_info.point_cr_value) ==
              kFilmGrainPointUVSize &&
          ARRAY_SIZE(va_film_grain_info.point_cr_scaling) ==
              kFilmGrainPointUVSize &&
          ARRAY_SIZE(film_grain_params.point_y_value) == kFilmGrainPointYSize &&
          ARRAY_SIZE(film_grain_params.point_y_scaling) ==
              kFilmGrainPointYSize &&
          ARRAY_SIZE(film_grain_params.point_u_value) ==
              kFilmGrainPointUVSize &&
          ARRAY_SIZE(film_grain_params.point_u_scaling) ==
              kFilmGrainPointUVSize &&
          ARRAY_SIZE(film_grain_params.point_v_value) ==
              kFilmGrainPointUVSize &&
          ARRAY_SIZE(film_grain_params.point_v_scaling) ==
              kFilmGrainPointUVSize,
      "Invalid array size of film grain values");
  DCHECK_LE(film_grain_params.num_y_points, kFilmGrainPointYSize);
  DCHECK_LE(film_grain_params.num_u_points, kFilmGrainPointUVSize);
  DCHECK_LE(film_grain_params.num_v_points, kFilmGrainPointUVSize);
#define COPY_FILM_GRAIN_FIELD2(a, b) va_film_grain_info.a = film_grain_params.b
#define COPY_FILM_GRAIN_FIELD3(a) COPY_FILM_GRAIN_FIELD2(a, a)
  COPY_FILM_GRAIN_FIELD3(grain_seed);
  COPY_FILM_GRAIN_FIELD3(num_y_points);
  for (uint8_t i = 0; i < film_grain_params.num_y_points; ++i) {
    COPY_FILM_GRAIN_FIELD3(point_y_value[i]);
    COPY_FILM_GRAIN_FIELD3(point_y_scaling[i]);
  }
#undef COPY_FILM_GRAIN_FIELD3
  COPY_FILM_GRAIN_FIELD2(num_cb_points, num_u_points);
  for (uint8_t i = 0; i < film_grain_params.num_u_points; ++i) {
    COPY_FILM_GRAIN_FIELD2(point_cb_value[i], point_u_value[i]);
    COPY_FILM_GRAIN_FIELD2(point_cb_scaling[i], point_u_scaling[i]);
  }
  COPY_FILM_GRAIN_FIELD2(num_cr_points, num_v_points);
  for (uint8_t i = 0; i < film_grain_params.num_v_points; ++i) {
    COPY_FILM_GRAIN_FIELD2(point_cr_value[i], point_v_value[i]);
    COPY_FILM_GRAIN_FIELD2(point_cr_scaling[i], point_v_scaling[i]);
  }

  constexpr size_t kAutoRegressionCoeffYSize = 24;
  constexpr size_t kAutoRegressionCoeffUVSize = 25;
  static_assert(
      ARRAY_SIZE(va_film_grain_info.ar_coeffs_y) == kAutoRegressionCoeffYSize &&
          ARRAY_SIZE(va_film_grain_info.ar_coeffs_cb) ==
              kAutoRegressionCoeffUVSize &&
          ARRAY_SIZE(va_film_grain_info.ar_coeffs_cr) ==
              kAutoRegressionCoeffUVSize &&
          ARRAY_SIZE(film_grain_params.auto_regression_coeff_y) ==
              kAutoRegressionCoeffYSize &&
          ARRAY_SIZE(film_grain_params.auto_regression_coeff_u) ==
              kAutoRegressionCoeffUVSize &&
          ARRAY_SIZE(film_grain_params.auto_regression_coeff_v) ==
              kAutoRegressionCoeffUVSize,
      "Invalid array size of auto-regressive coefficients");
  const size_t num_pos_y = (film_grain_params.auto_regression_coeff_lag * 2) *
                           (film_grain_params.auto_regression_coeff_lag + 1);
  const size_t num_pos_uv = num_pos_y + (film_grain_params.num_y_points > 0);
  if (film_grain_params.num_y_points > 0) {
    DCHECK_LE(num_pos_y, kAutoRegressionCoeffYSize);
    for (size_t i = 0; i < num_pos_y; ++i)
      COPY_FILM_GRAIN_FIELD2(ar_coeffs_y[i], auto_regression_coeff_y[i]);
  }
  if (film_grain_params.chroma_scaling_from_luma ||
      film_grain_params.num_u_points > 0 ||
      film_grain_params.num_v_points > 0) {
    DCHECK_LE(num_pos_uv, kAutoRegressionCoeffUVSize);
    for (size_t i = 0; i < num_pos_uv; ++i) {
      if (film_grain_params.chroma_scaling_from_luma ||
          film_grain_params.num_u_points > 0) {
        COPY_FILM_GRAIN_FIELD2(ar_coeffs_cb[i], auto_regression_coeff_u[i]);
      }
      if (film_grain_params.chroma_scaling_from_luma ||
          film_grain_params.num_v_points > 0) {
        COPY_FILM_GRAIN_FIELD2(ar_coeffs_cr[i], auto_regression_coeff_v[i]);
      }
    }
  }
  if (film_grain_params.num_u_points > 0) {
    COPY_FILM_GRAIN_FIELD2(cb_mult, u_multiplier + 128);
    COPY_FILM_GRAIN_FIELD2(cb_luma_mult, u_luma_multiplier + 128);
    COPY_FILM_GRAIN_FIELD2(cb_offset, u_offset + 256);
  }
  if (film_grain_params.num_v_points > 0) {
    COPY_FILM_GRAIN_FIELD2(cr_mult, v_multiplier + 128);
    COPY_FILM_GRAIN_FIELD2(cr_luma_mult, v_luma_multiplier + 128);
    COPY_FILM_GRAIN_FIELD2(cr_offset, v_offset + 256);
  }
#undef COPY_FILM_GRAIN_FIELD2
}

void FillGlobalMotionInfo(
    VAWarpedMotionParamsAV1 va_warped_motion[7],
    const std::array<libgav1::GlobalMotion, libgav1::kNumReferenceFrameTypes>&
        global_motion) {
  // global_motion[0] (for kReferenceFrameIntra) is not used.
  constexpr size_t kWarpedMotionSize = libgav1::kNumReferenceFrameTypes - 1;
  for (size_t i = 0; i < kWarpedMotionSize; ++i) {
    // Copy |global_motion| because SetupShear updates the affine variables of
    // the |global_motion|.
    auto gm = global_motion[i + 1];
    switch (gm.type) {
      case libgav1::kGlobalMotionTransformationTypeIdentity:
        va_warped_motion[i].wmtype = VAAV1TransformationIdentity;
        break;
      case libgav1::kGlobalMotionTransformationTypeTranslation:
        va_warped_motion[i].wmtype = VAAV1TransformationTranslation;
        break;
      case libgav1::kGlobalMotionTransformationTypeRotZoom:
        va_warped_motion[i].wmtype = VAAV1TransformationRotzoom;
        break;
      case libgav1::kGlobalMotionTransformationTypeAffine:
        va_warped_motion[i].wmtype = VAAV1TransformationAffine;
        break;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Invalid global motion transformation type, "
            << va_warped_motion[i].wmtype;
    }
    static_assert(ARRAY_SIZE(va_warped_motion[i].wmmat) == 8 &&
                      ARRAY_SIZE(gm.params) == 6,
                  "Invalid size of warp motion parameters");
    for (size_t j = 0; j < 6; ++j)
      va_warped_motion[i].wmmat[j] = gm.params[j];
    va_warped_motion[i].wmmat[6] = 0;
    va_warped_motion[i].wmmat[7] = 0;
    va_warped_motion[i].invalid = !libgav1::SetupShear(&gm);
  }
}

bool FillTileInfo(VADecPictureParameterBufferAV1& va_pic_param,
                  const libgav1::TileInfo& tile_info) {
  // Since gav1 decoder doesn't support decoding with tile lists (i.e. large
  // scale tile decoding), libgav1::ObuParser doesn't parse tile list, so that
  // we cannot acquire anchor_frames_num, anchor_frames_list, tile_count_minus_1
  // and output_frame_width/height_in_tiles_minus_1, and thus must set them and
  // large_scale_tile to 0 or false. This is already done by the memset in
  // SubmitDecode(). libgav1::ObuParser returns kStatusUnimplemented on
  // ParseOneFrame(), a fallback to av1 software decoder happens in the large
  // scale tile decoding.
  // TODO(hiroh): Support the large scale tile decoding once libgav1::ObuParser
  // supports it.
  va_pic_param.tile_cols = base::checked_cast<uint8_t>(tile_info.tile_columns);
  va_pic_param.tile_rows = base::checked_cast<uint8_t>(tile_info.tile_rows);

  if (!tile_info.uniform_spacing) {
    constexpr int kVaSizeOfTileWidthAndHeightArray = 63;
    static_assert(
        ARRAY_SIZE(tile_info.tile_column_width_in_superblocks) == 65 &&
            ARRAY_SIZE(tile_info.tile_row_height_in_superblocks) == 65 &&
            ARRAY_SIZE(va_pic_param.width_in_sbs_minus_1) ==
                kVaSizeOfTileWidthAndHeightArray &&
            ARRAY_SIZE(va_pic_param.height_in_sbs_minus_1) ==
                kVaSizeOfTileWidthAndHeightArray,
        "Invalid sizes of tile column widths and row heights");
    const int tile_columns =
        std::min(kVaSizeOfTileWidthAndHeightArray, tile_info.tile_columns);
    for (int i = 0; i < tile_columns; i++) {
      if (!base::CheckSub<int>(tile_info.tile_column_width_in_superblocks[i], 1)
               .AssignIfValid(&va_pic_param.width_in_sbs_minus_1[i])) {
        return false;
      }
    }
    const int tile_rows =
        std::min(kVaSizeOfTileWidthAndHeightArray, tile_info.tile_rows);
    for (int i = 0; i < tile_rows; i++) {
      if (!base::CheckSub<int>(tile_info.tile_row_height_in_superblocks[i], 1)
               .AssignIfValid(&va_pic_param.height_in_sbs_minus_1[i])) {
        return false;
      }
    }
  }

  va_pic_param.context_update_tile_id =
      base::checked_cast<uint16_t>(tile_info.context_update_id);
  return true;
}

void FillLoopFilterInfo(VADecPictureParameterBufferAV1& va_pic_param,
                        const libgav1::LoopFilter& loop_filter) {
  static_assert(STD_ARRAY_SIZE(loop_filter.level) == libgav1::kFrameLfCount &&
                    libgav1::kFrameLfCount == 4 &&
                    ARRAY_SIZE(va_pic_param.filter_level) == 2,
                "Invalid size of loop filter strength array");
  va_pic_param.filter_level[0] =
      base::checked_cast<uint8_t>(loop_filter.level[0]);
  va_pic_param.filter_level[1] =
      base::checked_cast<uint8_t>(loop_filter.level[1]);
  va_pic_param.filter_level_u =
      base::checked_cast<uint8_t>(loop_filter.level[2]);
  va_pic_param.filter_level_v =
      base::checked_cast<uint8_t>(loop_filter.level[3]);

  va_pic_param.loop_filter_info_fields.bits.sharpness_level =
      loop_filter.sharpness;
  va_pic_param.loop_filter_info_fields.bits.mode_ref_delta_enabled =
      loop_filter.delta_enabled;
  va_pic_param.loop_filter_info_fields.bits.mode_ref_delta_update =
      loop_filter.delta_update;

  static_assert(libgav1::kNumReferenceFrameTypes == 8 &&
                    ARRAY_SIZE(va_pic_param.ref_deltas) ==
                        libgav1::kNumReferenceFrameTypes &&
                    STD_ARRAY_SIZE(loop_filter.ref_deltas) ==
                        libgav1::kNumReferenceFrameTypes,
                "Invalid size of ref deltas array");
  static_assert(libgav1::kLoopFilterMaxModeDeltas == 2 &&
                    ARRAY_SIZE(va_pic_param.mode_deltas) ==
                        libgav1::kLoopFilterMaxModeDeltas &&
                    STD_ARRAY_SIZE(loop_filter.mode_deltas) ==
                        libgav1::kLoopFilterMaxModeDeltas,
                "Invalid size of mode deltas array");
  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; i++)
    va_pic_param.ref_deltas[i] = loop_filter.ref_deltas[i];
  for (size_t i = 0; i < libgav1::kLoopFilterMaxModeDeltas; i++)
    va_pic_param.mode_deltas[i] = loop_filter.mode_deltas[i];
}

void FillQuantizationInfo(VADecPictureParameterBufferAV1& va_pic_param,
                          const libgav1::QuantizerParameters& quant_param) {
  va_pic_param.base_qindex = quant_param.base_index;
  static_assert(
      libgav1::kPlaneY == 0 && libgav1::kPlaneU == 1 && libgav1::kPlaneV == 2,
      "Invalid plane index");
  static_assert(libgav1::kMaxPlanes == 3 &&
                    ARRAY_SIZE(quant_param.delta_dc) == libgav1::kMaxPlanes &&
                    ARRAY_SIZE(quant_param.delta_ac) == libgav1::kMaxPlanes,
                "Invalid size of delta dc/ac array");
  va_pic_param.y_dc_delta_q = quant_param.delta_dc[0];
  va_pic_param.u_dc_delta_q = quant_param.delta_dc[1];
  va_pic_param.v_dc_delta_q = quant_param.delta_dc[2];
  // quant_param.delta_ac[0] is useless as it is always 0.
  va_pic_param.u_ac_delta_q = quant_param.delta_ac[1];
  va_pic_param.v_ac_delta_q = quant_param.delta_ac[2];

  va_pic_param.qmatrix_fields.bits.using_qmatrix = quant_param.use_matrix;
  if (!quant_param.use_matrix)
    return;
  static_assert(ARRAY_SIZE(quant_param.matrix_level) == libgav1::kMaxPlanes,
                "Invalid size of matrix levels");
  va_pic_param.qmatrix_fields.bits.qm_y =
      base::checked_cast<uint16_t>(quant_param.matrix_level[0]);
  va_pic_param.qmatrix_fields.bits.qm_u =
      base::checked_cast<uint16_t>(quant_param.matrix_level[1]);
  va_pic_param.qmatrix_fields.bits.qm_v =
      base::checked_cast<uint16_t>(quant_param.matrix_level[2]);
}

void FillCdefInfo(VADecPictureParameterBufferAV1& va_pic_param,
                  const libgav1::Cdef& cdef,
                  uint8_t color_bitdepth) {
  // Damping value parsed in libgav1 is from the spec + (bitdepth - 8).
  // All the strength values parsed in libgav1 are from the spec and left
  // shifted by (bitdepth - 8).
  CHECK_GE(color_bitdepth, 8u);
  const uint8_t coeff_shift = color_bitdepth - 8u;
  va_pic_param.cdef_damping_minus_3 =
      base::checked_cast<uint8_t>(cdef.damping - coeff_shift - 3u);

  va_pic_param.cdef_bits = cdef.bits;
  static_assert(
      libgav1::kMaxCdefStrengths == 8 &&
          ARRAY_SIZE(cdef.y_primary_strength) == libgav1::kMaxCdefStrengths &&
          ARRAY_SIZE(cdef.y_secondary_strength) == libgav1::kMaxCdefStrengths &&
          ARRAY_SIZE(cdef.uv_primary_strength) == libgav1::kMaxCdefStrengths &&
          ARRAY_SIZE(cdef.uv_secondary_strength) ==
              libgav1::kMaxCdefStrengths &&
          ARRAY_SIZE(va_pic_param.cdef_y_strengths) ==
              libgav1::kMaxCdefStrengths &&
          ARRAY_SIZE(va_pic_param.cdef_uv_strengths) ==
              libgav1::kMaxCdefStrengths,
      "Invalid size of cdef strengths");
  const size_t num_cdef_strengths = 1 << cdef.bits;
  DCHECK_LE(num_cdef_strengths,
            static_cast<size_t>(libgav1::kMaxCdefStrengths));
  for (size_t i = 0; i < num_cdef_strengths; ++i) {
    const uint8_t prim_strength = cdef.y_primary_strength[i] >> coeff_shift;
    uint8_t sec_strength = cdef.y_secondary_strength[i] >> coeff_shift;
    DCHECK_LE(sec_strength, 4u);
    if (sec_strength == 4)
      sec_strength--;
    va_pic_param.cdef_y_strengths[i] =
        ((prim_strength & 0xf) << 2) | (sec_strength & 0x03);
  }

  for (size_t i = 0; i < num_cdef_strengths; ++i) {
    const uint8_t prim_strength = cdef.uv_primary_strength[i] >> coeff_shift;
    uint8_t sec_strength = cdef.uv_secondary_strength[i] >> coeff_shift;
    DCHECK_LE(sec_strength, 4u);
    if (sec_strength == 4)
      sec_strength--;
    va_pic_param.cdef_uv_strengths[i] =
        ((prim_strength & 0xf) << 2) | (sec_strength & 0x03);
  }
}

void FillModeControlInfo(VADecPictureParameterBufferAV1& va_pic_param,
                         const libgav1::ObuFrameHeader& frame_header) {
  auto& mode_control = va_pic_param.mode_control_fields.bits;
  mode_control.delta_q_present_flag = frame_header.delta_q.present;
  mode_control.log2_delta_q_res = frame_header.delta_q.scale;
  mode_control.delta_lf_present_flag = frame_header.delta_lf.present;
  mode_control.log2_delta_lf_res = frame_header.delta_lf.scale;
  mode_control.delta_lf_multi = frame_header.delta_lf.multi;
  DCHECK_LE(0u, frame_header.tx_mode);
  DCHECK_LE(frame_header.tx_mode, 2u);
  mode_control.tx_mode = frame_header.tx_mode;

  mode_control.reference_select = frame_header.reference_mode_select;
  mode_control.reduced_tx_set_used = frame_header.reduced_tx_set;
  mode_control.skip_mode_present = frame_header.skip_mode_present;
}

void FillLoopRestorationInfo(VADecPictureParameterBufferAV1& va_pic_param,
                             const libgav1::LoopRestoration& loop_restoration) {
  auto to_frame_restoration_type =
      [](libgav1::LoopRestorationType lr_type) -> uint16_t {
    // Spec. 6.10.15
    switch (lr_type) {
      case libgav1::LoopRestorationType::kLoopRestorationTypeNone:
        return 0;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSwitchable:
        return 3;
      case libgav1::LoopRestorationType::kLoopRestorationTypeWiener:
        return 1;
      case libgav1::LoopRestorationType::kLoopRestorationTypeSgrProj:
        return 2;
      default:
        NOTREACHED_IN_MIGRATION()
            << "Invalid restoration type" << base::strict_cast<int>(lr_type);
        return 0;
    }
  };
  static_assert(
      libgav1::kMaxPlanes == 3 &&
          ARRAY_SIZE(loop_restoration.type) == libgav1::kMaxPlanes &&
          ARRAY_SIZE(loop_restoration.unit_size_log2) == libgav1::kMaxPlanes,
      "Invalid size of loop restoration values");
  auto& va_loop_restoration = va_pic_param.loop_restoration_fields.bits;
  va_loop_restoration.yframe_restoration_type =
      to_frame_restoration_type(loop_restoration.type[0]);
  va_loop_restoration.cbframe_restoration_type =
      to_frame_restoration_type(loop_restoration.type[1]);
  va_loop_restoration.crframe_restoration_type =
      to_frame_restoration_type(loop_restoration.type[2]);

  const size_t num_planes = libgav1::kMaxPlanes;
  const bool use_loop_restoration =
      std::find_if(std::begin(loop_restoration.type),
                   std::begin(loop_restoration.type) + num_planes,
                   [](const auto type) {
                     return type != libgav1::kLoopRestorationTypeNone;
                   }) != (loop_restoration.type + num_planes);
  if (!use_loop_restoration)
    return;
  static_assert(libgav1::kPlaneY == 0u && libgav1::kPlaneU == 1u,
                "Invalid plane index");
  DCHECK_GE(loop_restoration.unit_size_log2[0], 6);
  DCHECK_GE(loop_restoration.unit_size_log2[0],
            loop_restoration.unit_size_log2[1]);
  DCHECK_LE(
      loop_restoration.unit_size_log2[0] - loop_restoration.unit_size_log2[1],
      1);
  va_loop_restoration.lr_unit_shift = loop_restoration.unit_size_log2[0] - 6;
  va_loop_restoration.lr_uv_shift =
      loop_restoration.unit_size_log2[0] - loop_restoration.unit_size_log2[1];
}

bool FillAV1PictureParameter(const AV1Picture& pic,
                             const libgav1::ObuSequenceHeader& sequence_header,
                             const AV1ReferenceFrameVector& ref_frames,
                             VADecPictureParameterBufferAV1& va_pic_param) {
  memset(&va_pic_param, 0, sizeof(VADecPictureParameterBufferAV1));
  DCHECK_LE(base::strict_cast<uint8_t>(sequence_header.profile), 2u)
      << "Unknown profile: " << base::strict_cast<int>(sequence_header.profile);
  va_pic_param.profile = base::strict_cast<uint8_t>(sequence_header.profile);

  if (sequence_header.enable_order_hint) {
    DCHECK_GT(sequence_header.order_hint_bits, 0);
    DCHECK_LE(sequence_header.order_hint_bits, 8);
    va_pic_param.order_hint_bits_minus_1 = sequence_header.order_hint_bits - 1;
  }

  switch (sequence_header.color_config.bitdepth) {
    case 8:
      va_pic_param.bit_depth_idx = 0;
      break;
    case 10:
      va_pic_param.bit_depth_idx = 1;
      break;
    case 12:
      va_pic_param.bit_depth_idx = 2;
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown bit depth: "
          << base::strict_cast<int>(sequence_header.color_config.bitdepth);
  }
  switch (sequence_header.color_config.matrix_coefficients) {
    case libgav1::kMatrixCoefficientsIdentity:
    case libgav1::kMatrixCoefficientsBt709:
    case libgav1::kMatrixCoefficientsUnspecified:
    case libgav1::kMatrixCoefficientsFcc:
    case libgav1::kMatrixCoefficientsBt470BG:
    case libgav1::kMatrixCoefficientsBt601:
    case libgav1::kMatrixCoefficientsSmpte240:
    case libgav1::kMatrixCoefficientsSmpteYcgco:
    case libgav1::kMatrixCoefficientsBt2020Ncl:
    case libgav1::kMatrixCoefficientsBt2020Cl:
    case libgav1::kMatrixCoefficientsSmpte2085:
    case libgav1::kMatrixCoefficientsChromatNcl:
    case libgav1::kMatrixCoefficientsChromatCl:
    case libgav1::kMatrixCoefficientsIctcp:
      va_pic_param.matrix_coefficients = base::checked_cast<uint8_t>(
          sequence_header.color_config.matrix_coefficients);
      break;
    default:
      DLOG(ERROR) << "Invalid matrix coefficients: "
                  << static_cast<int>(
                         sequence_header.color_config.matrix_coefficients);
      return false;
  }

  DCHECK(!sequence_header.color_config.is_monochrome);
#define COPY_SEQ_FIELD(a) \
  va_pic_param.seq_info_fields.fields.a = sequence_header.a
#define COPY_SEQ_FIELD2(a, b) va_pic_param.seq_info_fields.fields.a = b
  COPY_SEQ_FIELD(still_picture);
  COPY_SEQ_FIELD(use_128x128_superblock);
  COPY_SEQ_FIELD(enable_filter_intra);
  COPY_SEQ_FIELD(enable_intra_edge_filter);
  COPY_SEQ_FIELD(enable_interintra_compound);
  COPY_SEQ_FIELD(enable_masked_compound);
  COPY_SEQ_FIELD(enable_dual_filter);
  COPY_SEQ_FIELD(enable_order_hint);
  COPY_SEQ_FIELD(enable_jnt_comp);
  COPY_SEQ_FIELD(enable_cdef);
  COPY_SEQ_FIELD2(mono_chrome, sequence_header.color_config.is_monochrome);
  COPY_SEQ_FIELD2(subsampling_x, sequence_header.color_config.subsampling_x);
  COPY_SEQ_FIELD2(subsampling_y, sequence_header.color_config.subsampling_y);
  COPY_SEQ_FIELD(film_grain_params_present);
#undef COPY_SEQ_FIELD
  switch (sequence_header.color_config.color_range) {
    case libgav1::kColorRangeStudio:
    case libgav1::kColorRangeFull:
      COPY_SEQ_FIELD2(color_range,
                      base::strict_cast<uint32_t>(
                          sequence_header.color_config.color_range));
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown color range: "
          << static_cast<int>(sequence_header.color_config.color_range);
  }
#undef COPY_SEQ_FILED2

  const libgav1::ObuFrameHeader& frame_header = pic.frame_header;
  const auto* vaapi_pic = static_cast<const VaapiAV1Picture*>(&pic);
  DCHECK_NE(vaapi_pic->display_va_surface_id(), VA_INVALID_SURFACE);
  DCHECK_NE(vaapi_pic->reconstruct_va_surface_id(), VA_INVALID_SURFACE);
  if (frame_header.film_grain_params.apply_grain) {
    DCHECK_NE(vaapi_pic->display_va_surface_id(),
              vaapi_pic->reconstruct_va_surface_id())
        << "When using film grain synthesis, the display and reconstruct "
           "surfaces"
        << " should be different.";
    va_pic_param.current_frame = vaapi_pic->reconstruct_va_surface_id();
    va_pic_param.current_display_picture = vaapi_pic->display_va_surface_id();
  } else {
    DCHECK_EQ(vaapi_pic->display_va_surface_id(),
              vaapi_pic->reconstruct_va_surface_id())
        << "When not using film grain synthesis, the display and reconstruct"
        << " surfaces should be the same.";
    va_pic_param.current_frame = vaapi_pic->display_va_surface_id();
    va_pic_param.current_display_picture = VA_INVALID_SURFACE;
  }

  if (!base::CheckSub<int32_t>(frame_header.width, 1)
           .AssignIfValid(&va_pic_param.frame_width_minus1) ||
      !base::CheckSub<int32_t>(frame_header.height, 1)
           .AssignIfValid(&va_pic_param.frame_height_minus1)) {
    DLOG(ERROR) << "Invalid frame width and height"
                << ", width=" << frame_header.width
                << ", height=" << frame_header.height;
    return false;
  }

  static_assert(libgav1::kNumReferenceFrameTypes == 8 &&
                    ARRAY_SIZE(va_pic_param.ref_frame_map) ==
                        libgav1::kNumReferenceFrameTypes,
                "Invalid size of reference frames");
  static_assert(libgav1::kNumInterReferenceFrameTypes == 7 &&
                    ARRAY_SIZE(frame_header.reference_frame_index) ==
                        libgav1::kNumInterReferenceFrameTypes &&
                    ARRAY_SIZE(va_pic_param.ref_frame_idx) ==
                        libgav1::kNumInterReferenceFrameTypes,
                "Invalid size of reference frame indices");
  for (size_t i = 0; i < libgav1::kNumReferenceFrameTypes; ++i) {
    const auto* ref_pic =
        static_cast<const VaapiAV1Picture*>(ref_frames[i].get());
    va_pic_param.ref_frame_map[i] =
        ref_pic ? ref_pic->reconstruct_va_surface_id() : VA_INVALID_SURFACE;
  }

  // |va_pic_param.ref_frame_idx| doesn't need to be filled in for intra frames
  // (it can be left zero initialized).
  if (!libgav1::IsIntraFrame(frame_header.frame_type)) {
    for (size_t i = 0; i < libgav1::kNumInterReferenceFrameTypes; ++i) {
      const int8_t index = frame_header.reference_frame_index[i];
      CHECK_GE(index, 0);
      CHECK_LT(index, libgav1::kNumReferenceFrameTypes);
      // AV1Decoder::CheckAndCleanUpReferenceFrames() ensures that
      // |ref_frames[index]| is valid for all the reference frames needed by the
      // current frame.
      DCHECK_NE(va_pic_param.ref_frame_map[index], VA_INVALID_SURFACE);
      va_pic_param.ref_frame_idx[i] = base::checked_cast<uint8_t>(index);
    }
  }

  va_pic_param.primary_ref_frame =
      base::checked_cast<uint8_t>(frame_header.primary_reference_frame);
  va_pic_param.order_hint = frame_header.order_hint;

  FillSegmentInfo(va_pic_param.seg_info, frame_header.segmentation);
  FillFilmGrainInfo(va_pic_param.film_grain_info,
                    frame_header.film_grain_params);

  if (!FillTileInfo(va_pic_param, frame_header.tile_info))
    return false;

  if (frame_header.use_superres) {
    DVLOG(2) << "Upscaling (use_superres=1) is not supported";
    return false;
  }
  auto& va_pic_info_fields = va_pic_param.pic_info_fields.bits;
  va_pic_info_fields.uniform_tile_spacing_flag =
      frame_header.tile_info.uniform_spacing;
#define COPY_PIC_FIELD(a) va_pic_info_fields.a = frame_header.a
  COPY_PIC_FIELD(show_frame);
  COPY_PIC_FIELD(showable_frame);
  COPY_PIC_FIELD(error_resilient_mode);
  COPY_PIC_FIELD(allow_screen_content_tools);
  COPY_PIC_FIELD(force_integer_mv);
  COPY_PIC_FIELD(allow_intrabc);
  COPY_PIC_FIELD(use_superres);
  COPY_PIC_FIELD(allow_high_precision_mv);
  COPY_PIC_FIELD(is_motion_mode_switchable);
  COPY_PIC_FIELD(use_ref_frame_mvs);
  COPY_PIC_FIELD(allow_warped_motion);
#undef COPY_PIC_FIELD
  switch (frame_header.frame_type) {
    case libgav1::FrameType::kFrameKey:
    case libgav1::FrameType::kFrameInter:
    case libgav1::FrameType::kFrameIntraOnly:
    case libgav1::FrameType::kFrameSwitch:
      va_pic_info_fields.frame_type =
          base::strict_cast<uint32_t>(frame_header.frame_type);
      break;
    default:
      NOTREACHED_IN_MIGRATION()
          << "Unknown frame type: "
          << base::strict_cast<int>(frame_header.frame_type);
  }
  va_pic_info_fields.disable_cdf_update = !frame_header.enable_cdf_update;
  va_pic_info_fields.disable_frame_end_update_cdf =
      !frame_header.enable_frame_end_update_cdf;

  static_assert(libgav1::kSuperResScaleNumerator == 8,
                "Invalid libgav1::kSuperResScaleNumerator value");
  CHECK_EQ(frame_header.superres_scale_denominator,
           libgav1::kSuperResScaleNumerator);
  va_pic_param.superres_scale_denominator =
      frame_header.superres_scale_denominator;
  DCHECK_LE(base::strict_cast<uint8_t>(frame_header.interpolation_filter), 4u)
      << "Unknown interpolation filter: "
      << base::strict_cast<int>(frame_header.interpolation_filter);
  va_pic_param.interp_filter =
      base::strict_cast<uint8_t>(frame_header.interpolation_filter);

  FillQuantizationInfo(va_pic_param, frame_header.quantizer);
  FillLoopFilterInfo(va_pic_param, frame_header.loop_filter);
  FillModeControlInfo(va_pic_param, frame_header);
  FillLoopRestorationInfo(va_pic_param, frame_header.loop_restoration);
  FillGlobalMotionInfo(va_pic_param.wm, frame_header.global_motion);
  FillCdefInfo(
      va_pic_param, frame_header.cdef,
      base::checked_cast<uint8_t>(sequence_header.color_config.bitdepth));
  return true;
}

bool FillAV1SliceParameters(
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    const size_t tile_columns,
    base::span<const uint8_t> data,
    std::vector<VASliceParameterBufferAV1>& va_slice_params) {
  CHECK_GT(tile_columns, 0u);
  const uint16_t num_tiles = base::checked_cast<uint16_t>(tile_buffers.size());
  va_slice_params.resize(num_tiles);
  for (uint16_t tile = 0; tile < num_tiles; ++tile) {
    VASliceParameterBufferAV1& va_tile_param = va_slice_params[tile];
    memset(&va_tile_param, 0, sizeof(VASliceParameterBufferAV1));
    va_tile_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
    va_tile_param.tile_row = tile / base::checked_cast<uint16_t>(tile_columns);
    va_tile_param.tile_column =
        tile % base::checked_cast<uint16_t>(tile_columns);
    if (!base::CheckedNumeric<size_t>(tile_buffers[tile].size)
             .AssignIfValid(&va_tile_param.slice_data_size)) {
      return false;
    }
    CHECK(tile_buffers[tile].data >= data.data());
    va_tile_param.slice_data_offset =
        base::checked_cast<uint32_t>(tile_buffers[tile].data - data.data());
    base::CheckedNumeric<uint32_t> safe_va_slice_data_end(
        va_tile_param.slice_data_offset);
    safe_va_slice_data_end += va_tile_param.slice_data_size;
    size_t va_slice_data_end;
    if (!safe_va_slice_data_end.AssignIfValid(&va_slice_data_end) ||
        va_slice_data_end > data.size()) {
      DLOG(ERROR) << "Invalid tile offset and size"
                  << ", offset=" << va_tile_param.slice_data_offset
                  << ", size=" << va_tile_param.slice_data_size
                  << ", entire data size=" << data.size();
      return false;
    }
  }
  return true;
}
}  // namespace

AV1VaapiVideoDecoderDelegate::AV1VaapiVideoDecoderDelegate(
    VaapiDecodeSurfaceHandler* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    ProtectedSessionUpdateCB on_protected_session_update_cb,
    CdmContext* cdm_context,
    EncryptionScheme encryption_scheme)
    : VaapiVideoDecoderDelegate(vaapi_dec,
                                std::move(vaapi_wrapper),
                                std::move(on_protected_session_update_cb),
                                cdm_context,
                                encryption_scheme) {}

AV1VaapiVideoDecoderDelegate::~AV1VaapiVideoDecoderDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!picture_params_);
  DCHECK(!crypto_params_);
  DCHECK(!protected_params_);
}

scoped_refptr<AV1Picture> AV1VaapiVideoDecoderDelegate::CreateAV1Picture(
    bool apply_grain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto display_va_surface_handle = vaapi_dec_->CreateSurface();
  if (!display_va_surface_handle) {
    return nullptr;
  }

  // TODO(339518553): Allow not-film grain nullptr |reconstruct_va_surface|.
  auto reconstruct_va_surface = std::make_unique<VASurfaceHandle>(
      display_va_surface_handle->id(), base::DoNothing());
  if (apply_grain) {
    // TODO(hiroh): When no surface is available here, this returns nullptr and
    // |display_va_surface| is released. Since the surface is back to the pool,
    // VaapiVideoDecoder will detect that there are surfaces available and will
    // start another decode task which means that CreateSurface() might fail
    // again for |reconstruct_va_surface| since only one surface might have gone
    // back to the pool (the one for |display_va_surface_handle|). We should
    // avoid this loop for the sake of efficiency.
    reconstruct_va_surface = vaapi_dec_->CreateSurface();
    if (!reconstruct_va_surface)
      return nullptr;
  }

  return base::MakeRefCounted<VaapiAV1Picture>(
      std::move(display_va_surface_handle), std::move(reconstruct_va_surface));
}

bool AV1VaapiVideoDecoderDelegate::OutputPicture(const AV1Picture& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* vaapi_pic = static_cast<const VaapiAV1Picture*>(&pic);
  vaapi_dec_->SurfaceReady(vaapi_pic->display_va_surface_id(),
                           vaapi_pic->bitstream_id(), vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

DecodeStatus AV1VaapiVideoDecoderDelegate::SubmitDecode(
    const AV1Picture& pic,
    const libgav1::ObuSequenceHeader& seq_header,
    const AV1ReferenceFrameVector& ref_frames,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const DecryptConfig* decrypt_config = pic.decrypt_config();
  if (decrypt_config && !SetDecryptConfig(decrypt_config->Clone()))
    return DecodeStatus::kFail;

  bool uses_crypto = false;
  std::vector<VAEncryptionSegmentInfo> encryption_segment_info;
  VAEncryptionParameters crypto_param{};
  if (IsEncryptedSession()) {
    const ProtectedSessionState state = SetupDecryptDecode(
        /*full_sample=*/false, data.size_bytes(), &crypto_param,
        &encryption_segment_info,
        decrypt_config ? decrypt_config->subsamples()
                       : std::vector<SubsampleEntry>());
    if (state == ProtectedSessionState::kFailed) {
      LOG(ERROR)
          << "SubmitDecode fails because we couldn't setup the protected "
             "session";
      return DecodeStatus::kFail;
    } else if (state != ProtectedSessionState::kCreated) {
      return DecodeStatus::kTryAgain;
    }
    uses_crypto = true;
    if (!crypto_params_) {
      crypto_params_ = vaapi_wrapper_->CreateVABuffer(
          VAEncryptionParameterBufferType, sizeof(crypto_param));
      if (!crypto_params_)
        return DecodeStatus::kFail;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // libgav1 ensures that tile_columns is >= 0 and <= MAX_TILE_COLS.
  DCHECK_LE(0, pic.frame_header.tile_info.tile_columns);
  DCHECK_LE(pic.frame_header.tile_info.tile_columns, libgav1::kMaxTileColumns);
  const size_t tile_columns =
      base::checked_cast<size_t>(pic.frame_header.tile_info.tile_columns);

  VADecPictureParameterBufferAV1 pic_param;
  std::vector<VASliceParameterBufferAV1> slice_params;
  if (!FillAV1PictureParameter(pic, seq_header, ref_frames, pic_param) ||
      !FillAV1SliceParameters(tile_buffers, tile_columns, data, slice_params)) {
    return DecodeStatus::kFail;
  }

  if (!picture_params_) {
    picture_params_ = vaapi_wrapper_->CreateVABuffer(
        VAPictureParameterBufferType, sizeof(pic_param));
    if (!picture_params_)
      return DecodeStatus::kFail;
  }

  // TODO(b/235138734): Once the driver is fixed, re-use the
  // VASliceParameterBufferAV1 buffers across frames instead of creating new
  // ones every time. Alternatively, consider recreating these buffers only if
  // |slice_params| changes from frame to frame.
  std::vector<std::unique_ptr<ScopedVABuffer>> slice_params_va_buffers;
  for (size_t i = 0; i < slice_params.size(); i++) {
    slice_params_va_buffers.push_back(vaapi_wrapper_->CreateVABuffer(
        VASliceParameterBufferType, sizeof(VASliceParameterBufferAV1)));
    if (!slice_params_va_buffers.back())
      return DecodeStatus::kFail;
  }

  // TODO(hiroh): Don't submit the entire coded data to the buffer. Instead,
  // only pass the data starting from the tile list OBU to reduce the size of
  // the VA buffer. When this is changed, the encrypted subsample ranges must
  // also be adjusted.
  // Create VASliceData buffer |encoded_data| every frame so that decoding can
  // be more asynchronous than reusing the buffer.
  std::unique_ptr<ScopedVABuffer> encoded_data;

  std::vector<std::pair<VABufferID, VaapiWrapper::VABufferDescriptor>> buffers =
      {{picture_params_->id(),
        {picture_params_->type(), picture_params_->size(), &pic_param}}};
  buffers.reserve(3 + slice_params.size());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsTranscrypted()) {
    CHECK(decrypt_config);
    CHECK_EQ(decrypt_config->subsamples().size(), 2u);
    if (!protected_params_) {
      protected_params_ = vaapi_wrapper_->CreateVABuffer(
          VAProtectedSliceDataBufferType, decrypt_config->key_id().length());
      if (!protected_params_)
        return DecodeStatus::kFail;
    }
    DCHECK_EQ(decrypt_config->key_id().length(), protected_params_->size());
    buffers.push_back({protected_params_->id(),
                       {protected_params_->type(), protected_params_->size(),
                        decrypt_config->key_id().data()}});
    encoded_data = vaapi_wrapper_->CreateVABuffer(
        VASliceDataBufferType,
        base::strict_cast<size_t>(
            decrypt_config->subsamples()[0].cypher_bytes));
    if (!encoded_data)
      return DecodeStatus::kFail;
    buffers.push_back(
        {encoded_data->id(),
         {encoded_data->type(), encoded_data->size(),
          data.data() + decrypt_config->subsamples()[0].clear_bytes}});
  } else {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    encoded_data = vaapi_wrapper_->CreateVABuffer(VASliceDataBufferType,
                                                  data.size_bytes());
    if (!encoded_data)
      return DecodeStatus::kFail;
    buffers.push_back(
        {encoded_data->id(),
         {encoded_data->type(), encoded_data->size(), data.data()}});
#if BUILDFLAG(IS_CHROMEOS_ASH)
  }
  if (uses_crypto) {
    buffers.push_back(
        {crypto_params_->id(),
         {crypto_params_->type(), crypto_params_->size(), &crypto_param}});
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  for (size_t i = 0; i < slice_params.size(); ++i) {
    buffers.push_back({slice_params_va_buffers[i]->id(),
                       {slice_params_va_buffers[i]->type(),
                        slice_params_va_buffers[i]->size(), &slice_params[i]}});
  }

  const auto* vaapi_pic = static_cast<const VaapiAV1Picture*>(&pic);
  const bool success = vaapi_wrapper_->MapAndCopyAndExecute(
      vaapi_pic->reconstruct_va_surface_id(), buffers);
  if (!success && NeedsProtectedSessionRecovery())
    return DecodeStatus::kTryAgain;

  if (success && IsEncryptedSession())
    ProtectedDecodedSucceeded();

  return success ? DecodeStatus::kOk : DecodeStatus::kFail;
}

void AV1VaapiVideoDecoderDelegate::OnVAContextDestructionSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Destroy the member ScopedVABuffers below since they refer to a VAContextID
  // that will be destroyed soon.
  picture_params_.reset();
  crypto_params_.reset();
  protected_params_.reset();
}
}  // namespace media
