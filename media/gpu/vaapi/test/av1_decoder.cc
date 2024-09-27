// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/test/av1_decoder.h"

#include <va/va.h>
#include <va/va_dec_av1.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "media/base/video_decoder.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/scoped_va_config.h"
#include "media/gpu/vaapi/test/scoped_va_context.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "third_party/libgav1/src/src/warp_prediction.h"

namespace media {
namespace vaapi_test {

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
  // DecodeNextFrame(). libgav1::ObuParser returns kStatusUnimplemented on
  // ParseOneFrame().
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

// Returns the preferred VA_RT_FORMAT for the given |color_config|.
// Because we're limited to profile 0, we can have 8 or 10 bits. We do not
// support monochrome configs, but do support 4:2:0 Chroma subsampling.
unsigned int GetFormatForColorConfig(libgav1::ColorConfig color_config) {
  LOG_ASSERT(!color_config.is_monochrome)
      << "Monochrome color config is not supported.";

  if (color_config.subsampling_x == 1u && color_config.subsampling_y == 1u) {
    // uses chroma subsampling
    if (color_config.bitdepth == 8) {
      return VA_RT_FORMAT_YUV420;

    } else if (color_config.bitdepth == 10) {
      return VA_RT_FORMAT_YUV420_10;
    } else {
      // GetFormatForColorConfig() is only called after we know we're dealing
      // with an
      // AV1 stream whose profile is 'main' - this profile only supports bit
      // depths of 8 and 10 and libgav1 should guarantee that
      // |color_config.bitdepth| meets that requirement at parsing time.
      NOTREACHED_IN_MIGRATION()
          << "Unsupported color config with chroma subsampling of bitdepth %d"
          << color_config.bitdepth;
    }
  }
  // If this AV1 stream has profile 'main', then libgav1 ensures that both
  // |color_config.subsampling_x| and |color_config.subsampling_y| are 1.
  NOTREACHED_IN_MIGRATION()
      << "Unsupported color config; only profile 0 with 4:2:0 Chroma "
         "subsampling is supported.";
  // There is no VA_RT_FORMAT_UNSUPPORTED; use a "default" value.
  return 0u;
}

}  // namespace

Av1Decoder::Av1Decoder(std::unique_ptr<IvfParser> ivf_parser,
                       const VaapiDevice& va_device,
                       SharedVASurface::FetchPolicy fetch_policy)
    : VideoDecoder::VideoDecoder(va_device, fetch_policy),
      buffer_pool_(std::make_unique<libgav1::BufferPool>(
          /*on_frame_buffer_size_changed=*/nullptr,
          /*get_frame_buffer=*/nullptr,
          /*release_frame_buffer=*/nullptr,
          /*callback_private_data=*/nullptr)),
      state_(std::make_unique<libgav1::DecoderState>()),
      ref_frames_(kAv1NumRefFrames),
      display_surfaces_(kAv1NumRefFrames),
      ivf_parser_(std::move(ivf_parser)) {}

Av1Decoder::~Av1Decoder() {
  // We destroy the state explicitly to ensure it's destroyed before the
  // |buffer_pool_|. The |buffer_pool_| checks that all the allocated frames
  // are released in its destructor. Explicitly destructing |state_| releases
  // frames in |reference_frame| in |state_|.
  state_.reset();

  // We destroy the VA handles explicitly to ensure the correct order.
  // The configuration must be destroyed after the context so that the
  // configuration reference remains valid in the context, and surfaces can only
  // be destroyed after the context as per
  // https://github.com/intel/libva/blob/8c6126e67c446f4c7808cb51b609077e4b9bd8fe/va/va.h#L1549
  va_context_.reset();
  va_config_.reset();
  ref_frames_.clear();
  display_surfaces_.clear();

  last_decoded_surface_.reset();
}

Av1Decoder::ParsingResult Av1Decoder::ReadNextFrame(
    libgav1::RefCountedBufferPtr& current_frame) {
  if (!obu_parser_ || !obu_parser_->HasData()) {
    if (!ivf_parser_->ParseNextFrame(&ivf_frame_header_, &ivf_frame_data_)) {
      return ParsingResult::kEOStream;
    }

    // The ObuParser has run out of data or did not exist in the first place. It
    // has no "replace the current buffer with a new buffer of a different size"
    // method; we must make a new parser.
    obu_parser_ = base::WrapUnique(new (std::nothrow) libgav1::ObuParser(
        ivf_frame_data_, ivf_frame_header_.frame_size, /*operating_point=*/0,
        buffer_pool_.get(), state_.get()));
    if (current_sequence_header_) {
      obu_parser_->set_sequence_header(*current_sequence_header_);
    }
  }

  libgav1::StatusCode code = obu_parser_->ParseOneFrame(&current_frame);
  if (code != libgav1::kStatusOk) {
    LOG(ERROR) << "Error parsing OBU stream: " << code;
    return ParsingResult::kFailed;
  }
  return ParsingResult::kOk;
}

void Av1Decoder::RefreshReferenceSlots(
    const uint8_t refresh_frame_flags,
    scoped_refptr<SharedVASurface> surface,
    libgav1::RefCountedBufferPtr current_frame,
    scoped_refptr<SharedVASurface> display_surface) {
  const std::bitset<kAv1NumRefFrames> slots(refresh_frame_flags);
  for (size_t i = 0; i < kAv1NumRefFrames; i++) {
    if (slots[i]) {
      ref_frames_[i] = surface;
      display_surfaces_[i] = display_surface;
    }
  }
  state_->UpdateReferenceFrames(current_frame,
                                base::strict_cast<int>(refresh_frame_flags));
}

VideoDecoder::Result Av1Decoder::DecodeNextFrame() {
  // Parse next frame from stream.
  libgav1::RefCountedBufferPtr current_frame;
  ParsingResult parser_res = ReadNextFrame(current_frame);

  if (parser_res != ParsingResult::kOk) {
    LOG_ASSERT(parser_res == ParsingResult::kEOStream)
        << "Failed to parse next frame, got " << static_cast<int>(parser_res);
    return VideoDecoder::kEOStream;
  }

  libgav1::ObuFrameHeader current_frame_header = obu_parser_->frame_header();
  if (current_frame_header.show_existing_frame) {
    last_decoded_frame_visible_ = true;
  } else {
    last_decoded_frame_visible_ = current_frame_header.show_frame;
  }

  if (obu_parser_->sequence_header_changed()) {
    if (current_frame_header.frame_type != libgav1::kFrameKey ||
        !current_frame_header.show_frame ||
        current_frame_header.show_existing_frame ||
        current_frame->temporal_id() != 0) {
      // Section 7.5.
      LOG(FATAL)
          << "The first frame successive to sequence header OBU must be a "
          << "keyframe with show_frame=1, show_existing_frame=0 and "
          << "temporal_id=0";
    }
    current_sequence_header_.emplace(obu_parser_->sequence_header());
    // TODO(clarissagarvey): Support other profiles once Chrome does.
    LOG_ASSERT(current_sequence_header_->profile ==
               libgav1::BitstreamProfile::kProfile0)
        << "Unsupported profile.";
    const VAProfile new_profile = VAProfileAV1Profile0;

    // (Section 6.4.1):
    //
    // - "An operating point specifies which spatial and temporal layers should
    //   be decoded."
    //
    // - "The order of operating points indicates the preferred order for
    //   producing an output: a decoder should select the earliest operating
    //   point in the list that meets its decoding capabilities as expressed by
    //   the level associated with each operating point."
    //
    // For simplicity, we always select operating point 0 and validate that it
    // doesn't have scalability information.
    LOG_ASSERT(current_sequence_header_->operating_point_idc[0] == 0)
        << "Either temporal or spatial layer decoding is not supported.";

    if (!va_config_ || va_config_->profile() != new_profile) {
      va_context_.reset();
      libgav1::ColorConfig color_config =
          current_sequence_header_.value().color_config;
      va_config_ = std::make_unique<ScopedVAConfig>(
          *va_device_, new_profile, GetFormatForColorConfig(color_config));
    }

    for (auto& frame : ref_frames_)
      frame.reset();

    for (auto& display_surface : display_surfaces_)
      display_surface.reset();

    // Update the context size if needed.
    const gfx::Size new_max_frame_size(
        base::strict_cast<int>(current_sequence_header_->max_frame_width),
        base::strict_cast<int>(current_sequence_header_->max_frame_height));
    if (!va_context_ || va_context_->size() != new_max_frame_size) {
      VLOG(1) << "New context size needed";
      VLOG_IF(1, va_context_)
          << "Previous context size: " << va_context_->size().ToString();
      VLOG(1) << "New context size: " << new_max_frame_size.ToString();
      va_context_ = std::make_unique<ScopedVAContext>(*va_device_, *va_config_,
                                                      new_max_frame_size);
    }
  }

  // Clean up reference frames.
  for (size_t i = 0; i < kAv1NumRefFrames; ++i) {
    if (state_->reference_frame[i] && !ref_frames_[i]) {
      LOG(FATAL) << "The state of the reference frames are different "
                    "between |ref_frames_| and |state_|";
    }
    if (!state_->reference_frame[i] && ref_frames_[i]) {
      ref_frames_[i].reset();
      display_surfaces_[i].reset();
    }
  }
  if (current_frame_header.show_existing_frame) {
    last_decoded_surface_ =
        display_surfaces_[current_frame_header.frame_to_show];
    RefreshReferenceSlots(current_frame_header.refresh_frame_flags,
                          last_decoded_surface_, current_frame,
                          last_decoded_surface_);
    return VideoDecoder::kOk;
  }

  LOG_ASSERT(current_sequence_header_)
      << "Sequence header missing for decoding.";

  // The frame_width and frame_height denote the visible part of the frame.
  // This handles resolution changes between sequence header changes: the
  // "resolution change" is really just an update to which part of the frame to
  // show to the user, which comes from the width and height hints provided in
  // |current_frame_header|.
  // Also see
  // https://source.chromium.org/chromium/chromium/src/+/main:media/gpu/av1_decoder.cc;l=454;drc=9c1d4b495c1ebadeda004c9b741e11a6f035b9e7
  const gfx::Size visible_size(
      base::strict_cast<int>(current_frame->frame_width()),
      base::strict_cast<int>(current_frame->frame_height()));

  // Create surfaces for decode.
  VASurfaceAttrib attribute;
  memset(&attribute, 0, sizeof(VASurfaceAttrib));
  attribute.type = VASurfaceAttribUsageHint;
  attribute.flags = VA_SURFACE_ATTRIB_SETTABLE;
  attribute.value.type = VAGenericValueTypeInteger;
  attribute.value.value.i = VA_SURFACE_ATTRIB_USAGE_HINT_DECODER;
  scoped_refptr<SharedVASurface> surface = SharedVASurface::Create(
      *va_device_, va_config_->va_rt_format(), visible_size, attribute);

  // Set up buffer for pic parameters
  VADecPictureParameterBufferAV1 pic_parameters;
  memset(&pic_parameters, 0, sizeof(VADecPictureParameterBufferAV1));
  pic_parameters.profile =
      base::strict_cast<uint8_t>(current_sequence_header_->profile);

  if (current_sequence_header_->enable_order_hint) {
    pic_parameters.order_hint_bits_minus_1 =
        current_sequence_header_->order_hint_bits - 1;
  }

  switch (current_sequence_header_->color_config.bitdepth) {
    case 8:
      pic_parameters.bit_depth_idx = 0;
      break;
    case 10:
      pic_parameters.bit_depth_idx = 1;
      break;
    case 12:
      // This is a valid bitdepth in streams, but we do not support it;
      // GetFormatForColorConfig() only expects bit depths of 8 or 10.
      NOTREACHED_IN_MIGRATION() << "12bpp color is not yet supported.";
      break;
    default:
      // The OBU Parser can only produce bit depths of 8, 10, and 12; we should
      // not hit any other cases. See
      // https://source.chromium.org/chromium/chromium/src/+/main:third_party/libgav1/src/src/obu_parser.cc;l=144-150;drc=7880d0cc1d1976012dbec8a1bb982191ac49b7f4
      NOTREACHED_IN_MIGRATION()
          << "Invalid color bit depth: "
          << current_sequence_header_->color_config.bitdepth;
  }
  pic_parameters.matrix_coefficients = base::checked_cast<uint8_t>(
      current_sequence_header_->color_config.matrix_coefficients);
#define COPY_SEQ_FIELD(a) \
  pic_parameters.seq_info_fields.fields.a = current_sequence_header_->a
#define COPY_SEQ_FIELD2(a, b) pic_parameters.seq_info_fields.fields.a = b
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
  COPY_SEQ_FIELD2(mono_chrome,
                  current_sequence_header_->color_config.is_monochrome);
  COPY_SEQ_FIELD2(subsampling_x,
                  current_sequence_header_->color_config.subsampling_x);
  COPY_SEQ_FIELD2(subsampling_y,
                  current_sequence_header_->color_config.subsampling_y);
  COPY_SEQ_FIELD(film_grain_params_present);
#undef COPY_SEQ_FIELD
  COPY_SEQ_FIELD2(color_range,
                  base::strict_cast<uint32_t>(
                      current_sequence_header_->color_config.color_range));
#undef COPY_SEQ_FIELD2

  scoped_refptr<SharedVASurface> film_grain_surface;
  if (current_frame_header.film_grain_params.apply_grain) {
    pic_parameters.current_frame = surface->id();
    film_grain_surface = SharedVASurface::Create(
        *va_device_, va_config_->va_rt_format(), visible_size, attribute);
    pic_parameters.current_display_picture = film_grain_surface->id();
  } else {
    pic_parameters.current_frame = surface->id();
    pic_parameters.current_display_picture = VA_INVALID_SURFACE;
  }
  pic_parameters.frame_width_minus1 = current_frame_header.width - 1;
  pic_parameters.frame_height_minus1 = current_frame_header.height - 1;

  for (size_t i = 0; i < kAv1NumRefFrames; ++i) {
    pic_parameters.ref_frame_map[i] =
        ref_frames_[i] ? ref_frames_[i]->id() : VA_INVALID_SURFACE;
  }

  // |pic_parameters.ref_frame_idx| doesn't need to be filled in for intra
  // frames (it can be left zero initialized).
  if (!libgav1::IsIntraFrame(current_frame_header.frame_type)) {
    for (size_t i = 0; i < kAv1NumRefFrames; ++i) {
      const int8_t index = current_frame_header.reference_frame_index[i];
      CHECK_GE(index, 0);
      CHECK_LT(static_cast<size_t>(index), kAv1NumRefFrames);
      pic_parameters.ref_frame_idx[i] = base::checked_cast<uint8_t>(index);
    }
  }
  pic_parameters.primary_ref_frame =
      base::checked_cast<uint8_t>(current_frame_header.primary_reference_frame);
  pic_parameters.order_hint = current_frame_header.order_hint;

  LOG_ASSERT(!current_frame_header.use_superres)
      << "Upscaling (use_superres=1) is not supported";

  FillSegmentInfo(pic_parameters.seg_info, current_frame_header.segmentation);
  FillFilmGrainInfo(pic_parameters.film_grain_info,
                    current_frame_header.film_grain_params);

  const bool tile_info_success =
      FillTileInfo(pic_parameters, current_frame_header.tile_info);
  LOG_ASSERT(tile_info_success)
      << "Failed to fill tile info for current frame.";

  auto& info_fields = pic_parameters.pic_info_fields.bits;
  info_fields.uniform_tile_spacing_flag =
      current_frame_header.tile_info.uniform_spacing;
#define COPY_PIC_FIELD(a) info_fields.a = current_frame_header.a
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
  info_fields.frame_type =
      base::strict_cast<uint32_t>(current_frame_header.frame_type);
  info_fields.disable_cdf_update = !current_frame_header.enable_cdf_update;
  info_fields.disable_frame_end_update_cdf =
      !current_frame_header.enable_frame_end_update_cdf;

  pic_parameters.superres_scale_denominator =
      current_frame_header.superres_scale_denominator;
  pic_parameters.interp_filter =
      base::strict_cast<uint8_t>(current_frame_header.interpolation_filter);

  FillLoopFilterInfo(pic_parameters, current_frame_header.loop_filter);
  FillQuantizationInfo(pic_parameters, current_frame_header.quantizer);
  const uint color_bitdepth = current_sequence_header_->color_config.bitdepth;
  FillCdefInfo(pic_parameters, current_frame_header.cdef, color_bitdepth);
  FillModeControlInfo(pic_parameters, current_frame_header);
  FillLoopRestorationInfo(pic_parameters,
                          current_frame_header.loop_restoration);
  FillGlobalMotionInfo(pic_parameters.wm, current_frame_header.global_motion);

  std::vector<VABufferID> buffers;
  VABufferID buffer_id;
  VAStatus res = vaCreateBuffer(
      va_device_->display(), va_context_->id(), VAPictureParameterBufferType,
      sizeof(VADecPictureParameterBufferAV1), 1u, &pic_parameters, &buffer_id);
  VA_LOG_ASSERT(res, "vaCreateBuffer");
  buffers.push_back(buffer_id);

  // Create buffer for "slice" (for AV1, this corresponds to tile) decoding.
  std::vector<VASliceParameterBufferAV1> slice_params;
  // The tiles are row-major; we pass in the number of columns for computation
  // of the row and column for a given flattened index.
  const size_t tile_columns = current_frame_header.tile_info.tile_columns;
  const bool slice_parameters_success = FillAV1SliceParameters(
      obu_parser_->tile_buffers(), tile_columns,
      base::make_span(ivf_frame_data_, ivf_frame_header_.frame_size),
      slice_params);
  LOG_ASSERT(slice_parameters_success)
      << "Failed to fill slice parameters for current frame.";

  // Set up a buffer for the slice parameters for each slice.
  for (auto& slice_param : slice_params) {
    res = vaCreateBuffer(
        va_device_->display(), va_context_->id(), VASliceParameterBufferType,
        sizeof(VASliceParameterBufferAV1), 1u, &slice_param, &buffer_id);
    VA_LOG_ASSERT(res, "vaCreateBuffer");
    buffers.push_back(buffer_id);
  }

  // Set up the slice data buffer.
  res = vaCreateBuffer(va_device_->display(), va_context_->id(),
                       VASliceDataBufferType, ivf_frame_header_.frame_size, 1u,
                       const_cast<uint8_t*>(ivf_frame_data_), &buffer_id);
  VA_LOG_ASSERT(res, "vaCreateBuffer");
  buffers.push_back(buffer_id);

  res = vaBeginPicture(va_device_->display(), va_context_->id(), surface->id());
  VA_LOG_ASSERT(res, "vaBeginPicture");

  res = vaRenderPicture(va_device_->display(), va_context_->id(),
                        buffers.data(), buffers.size());
  VA_LOG_ASSERT(res, "vaRenderPicture");

  res = vaEndPicture(va_device_->display(), va_context_->id());
  VA_LOG_ASSERT(res, "vaEndPicture");

  if (current_frame_header.film_grain_params.apply_grain) {
    last_decoded_surface_ = film_grain_surface;
  } else {
    last_decoded_surface_ = surface;
  }

  RefreshReferenceSlots(current_frame_header.refresh_frame_flags, surface,
                        current_frame, last_decoded_surface_);

  for (auto id : buffers) {
    vaDestroyBuffer(va_device_->display(), id);
  }
  buffers.clear();

  return VideoDecoder::kOk;
}

}  // namespace vaapi_test
}  // namespace media
