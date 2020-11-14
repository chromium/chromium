// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/av1_vaapi_video_decoder_delegate.h"

#include <string.h>
#include <va/va.h>
#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libgav1/src/src/obu_parser.h"
#include "third_party/libgav1/src/src/utils/types.h"
#include "third_party/libgav1/src/src/warp_prediction.h"

namespace media {
namespace {

#define ARRAY_SIZE(ar) (sizeof(ar) / sizeof(ar[0]))

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
        NOTREACHED() << "Invalid global motion transformation type, "
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

bool FillAV1PictureParameter(const AV1Picture& pic,
                             const libgav1::ObuSequenceHeader& seq_header,
                             const AV1ReferenceFrameVector& ref_frames,
                             VADecPictureParameterBufferAV1& pic_param) {
  memset(&pic_param, 0, sizeof(VADecPictureParameterBufferAV1));
  NOTIMPLEMENTED();
  return false;
}

bool FillAV1SliceParameters(
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    const size_t tile_columns,
    base::span<const uint8_t> data,
    std::vector<VASliceParameterBufferAV1>& slice_params) {
  NOTIMPLEMENTED();
  return false;
}
}  // namespace

AV1VaapiVideoDecoderDelegate::AV1VaapiVideoDecoderDelegate(
    DecodeSurfaceHandler<VASurface>* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : VaapiVideoDecoderDelegate(vaapi_dec, std::move(vaapi_wrapper)) {}

AV1VaapiVideoDecoderDelegate::~AV1VaapiVideoDecoderDelegate() = default;

scoped_refptr<AV1Picture> AV1VaapiVideoDecoderDelegate::CreateAV1Picture(
    bool apply_grain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto display_va_surface = vaapi_dec_->CreateSurface();
  if (!display_va_surface)
    return nullptr;

  auto reconstruct_va_surface = display_va_surface;
  if (apply_grain) {
    // TODO(hiroh): When no surface is available here, this returns nullptr and
    // |display_va_surface| is released. Since the surface is back to the pool,
    // VaapiVideoDecoder will detect that there are surfaces available and will
    // start another decode task which means that CreateSurface() might fail
    // again for |reconstruct_va_surface| since only one surface might have gone
    // back to the pool (the one for |display_va_surface|). We should avoid this
    // loop for the sake of efficiency.
    reconstruct_va_surface = vaapi_dec_->CreateSurface();
    if (!reconstruct_va_surface)
      return nullptr;
  }

  return base::MakeRefCounted<VaapiAV1Picture>(
      std::move(display_va_surface), std::move(reconstruct_va_surface));
}

bool AV1VaapiVideoDecoderDelegate::OutputPicture(const AV1Picture& pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* vaapi_pic = static_cast<const VaapiAV1Picture*>(&pic);
  vaapi_dec_->SurfaceReady(vaapi_pic->display_va_surface(),
                           vaapi_pic->bitstream_id(), vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

bool AV1VaapiVideoDecoderDelegate::SubmitDecode(
    const AV1Picture& pic,
    const libgav1::ObuSequenceHeader& seq_header,
    const AV1ReferenceFrameVector& ref_frames,
    const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
    base::span<const uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // libgav1 ensures that tile_columns is >= 0 and <= MAX_TILE_COLS.
  DCHECK_LE(0, pic.frame_header.tile_info.tile_columns);
  DCHECK_LE(pic.frame_header.tile_info.tile_columns, libgav1::kMaxTileColumns);
  const size_t tile_columns =
      base::checked_cast<size_t>(pic.frame_header.tile_info.tile_columns);

  VADecPictureParameterBufferAV1 pic_param;
  std::vector<VASliceParameterBufferAV1> slice_params;
  if (!FillAV1PictureParameter(pic, seq_header, ref_frames, pic_param) ||
      !FillAV1SliceParameters(tile_buffers, tile_columns, data, slice_params)) {
    return false;
  }

  // TODO(hiroh): Batch VABuffer submissions like Vp9VaapiVideoDecoderDelegate.
  // Submit the picture parameters.
  if (!vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType, &pic_param))
    return false;

  // Submit the entire buffer and the per-tile information.
  // TODO(hiroh): Don't submit the entire coded data to the buffer. Instead,
  // only pass the data starting from the tile list OBU to reduce the size of
  // the VA buffer.
  if (!vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType, data.size(),
                                    data.data())) {
    return false;
  }
  for (const VASliceParameterBufferAV1& tile_param : slice_params) {
    if (!vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType,
                                      &tile_param)) {
      return false;
    }
  }

  const auto* vaapi_pic = static_cast<const VaapiAV1Picture*>(&pic);
  return vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
      vaapi_pic->display_va_surface()->id());
}
}  // namespace media
