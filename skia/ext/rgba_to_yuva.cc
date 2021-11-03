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
                    const SkYUVAInfo& dst_yuva_info,
                    const SkRect& dst_region) {
  const SkRect src_rect = SkRect::Make(src_image->bounds());
  const SkRect dst_rect =
      dst_region.isEmpty()
          ? SkRect::MakeSize(SkSize::Make(dst_yuva_info.dimensions()))
          : dst_region;

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
  SkColorMatrix rgb_to_yuv_matrix =
      SkColorMatrix::RGBtoYUV(dst_yuva_info.yuvColorSpace());

  // Blit each plane.
  for (int plane = 0; plane < dst_yuva_info.numPlanes(); ++plane) {
    SkColorMatrix color_matrix = rgb_to_yuv_matrix;
    color_matrix.postConcat(permutation_matrices[plane]);

    SkSamplingOptions sampling_options(SkFilterMode::kLinear);

    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);

    // Blend the input image over black before performing RGB to YUV
    // conversion, to match un-accelerated versions.
    paint.setColorFilter(SkColorFilters::Compose(
        SkColorFilters::Matrix(color_matrix),
        SkColorFilters::Blend(SK_ColorBLACK, SkBlendMode::kDstOver)));

    // Subsampling factors are determined by the ratios of the entire image's
    // width & height to the dimensions of the passed in surfaces (which should
    // also span the entire logical image):
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
