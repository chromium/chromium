// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/rgba_to_yuva.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"

namespace skia {

void BlitRGBAToYUVA(SkImage* src_image,
                    SkSurface* dst_surfaces[SkYUVAInfo::kMaxPlanes],
                    const SkYUVAInfo& dst_yuva_info) {
  DCHECK(dst_yuva_info.yuvColorSpace() == kRec601_Limited_SkYUVColorSpace ||
         dst_yuva_info.yuvColorSpace() == kRec709_Limited_SkYUVColorSpace);

  const SkRect src_rect = SkRect::Make(src_image->bounds());
  const SkRect dst_rect =
      SkRect::MakeSize(SkSize::Make(dst_yuva_info.dimensions()));

  // TODO(https://crbug.com/skia/12545): Expose color matrices via SkYUVAInfo.
  // This corresponds to hard-coding kRec601_Limited_SkYUVColorSpace:
  constexpr SkColorMatrix rgb_to_yuv_rec601_limited(
      0.257f, 0.504f, 0.098f, 0.000f, 0.063f,    //
      -0.148f, -0.291f, 0.439f, 0.000f, 0.500f,  //
      0.439f, -0.368f, -0.071f, 0.000f, 0.500f,  //
      0.000f, 0.000f, 0.000f, 1.000f, 0.000f);
  // This corresponds to hard-coding kRec709_Limited_SkYUVColorSpace:
  constexpr SkColorMatrix rgb_to_yuv_rec709_limited(
      0.183f, 0.614f, 0.062f, 0.000f, 0.063f,    //
      -0.101f, -0.339f, 0.439f, 0.000f, 0.502f,  //
      0.439f, -0.399f, -0.040f, 0.000f, 0.502f,  //
      0.000f, 0.000f, 0.000f, 1.000f, 0.000f);

  // Permutation matrices to select the appropriate YUVA channels for each
  // output plane.
  constexpr SkColorMatrix xxxY(0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               1, 0, 0, 0, 0);
  constexpr SkColorMatrix UVx1(0, 1, 0, 0, 0,  //
                               0, 0, 1, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 1, 0);

  // Only Y_UV has been tested.
  SkColorMatrix permutation_matrices[SkYUVAInfo::kMaxPlanes];
  switch (dst_yuva_info.planeConfig()) {
    case SkYUVAInfo::PlaneConfig::kY_UV:
      permutation_matrices[0] = xxxY;
      permutation_matrices[1] = UVx1;
      break;
    default:
      DLOG(ERROR) << "Unsupported plane configuration.";
      return;
  }

  SkColorMatrix rgb_to_yuv_matrix;
  switch (dst_yuva_info.yuvColorSpace()) {
    case kRec601_Limited_SkYUVColorSpace:
      rgb_to_yuv_matrix = rgb_to_yuv_rec601_limited;
      break;
    default:
      // TODO(https://crbug.com/skia/12545): Query Skia for matrices.
      DLOG(ERROR) << "Unsupported color matrix, using Rec709.";
    case kRec709_Limited_SkYUVColorSpace:
      rgb_to_yuv_matrix = rgb_to_yuv_rec709_limited;
      break;
  }

  // Blit each plane.
  for (int plane = 0; plane < dst_yuva_info.numPlanes(); ++plane) {
    SkColorMatrix color_matrix = rgb_to_yuv_matrix;
    color_matrix.postConcat(permutation_matrices[plane]);

    SkSamplingOptions sampling_options(SkFilterMode::kLinear);

    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);
    paint.setColorFilter(SkColorFilters::Matrix(color_matrix));

    float subsampling_factors[2] = {
        static_cast<float>(dst_surfaces[plane]->width()) /
            dst_yuva_info.dimensions().width(),
        static_cast<float>(dst_surfaces[plane]->height()) /
            dst_yuva_info.dimensions().height(),
    };
    SkRect plane_dst_rect =
        SkRect::MakeXYWH(dst_rect.x() * subsampling_factors[0],
                         dst_rect.y() * subsampling_factors[1],
                         dst_rect.width() * subsampling_factors[0],
                         dst_rect.height() * subsampling_factors[1]);

    dst_surfaces[plane]->getCanvas()->drawImageRect(
        src_image, src_rect, plane_dst_rect, sampling_options, &paint,
        SkCanvas::kFast_SrcRectConstraint);
  }
}

}  // namespace skia
