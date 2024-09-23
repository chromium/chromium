// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/rgba_to_yuva.h"

#include <array>

#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"

namespace skia {

namespace {

SkRect GetSubsampledRect(const SkRect& rect,
                         const std::array<float, 2>& subsampling_factors) {
  return SkRect::MakeXYWH(rect.x() * subsampling_factors[0],
                          rect.y() * subsampling_factors[1],
                          rect.width() * subsampling_factors[0],
                          rect.height() * subsampling_factors[1]);
}

}  // namespace

void BlitRGBAToYUVA(SkImage* src_image,
                    SkSurface* dst_surfaces[SkYUVAInfo::kMaxPlanes],
                    const SkYUVAInfo& dst_yuva_info,
                    const SkRect& dst_region,
                    bool clear_destination) {
  // Rectangle representing the entire destination image:
  const SkRect dst_image_rect = SkRect::Make(dst_yuva_info.dimensions());
  const SkRect src_rect = SkRect::Make(src_image->bounds());
  // Region of destination image that is supposed to be populated:
  const SkRect dst_rect = dst_region.isEmpty() ? dst_image_rect : dst_region;

  DCHECK(dst_image_rect.contains(dst_rect));

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
    SkCanvas* plane_canvas = dst_surfaces[plane]->getCanvas();

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
    std::array<float, 2> subsampling_factors = {
        static_cast<float>(dst_surfaces[plane]->width()) /
            dst_yuva_info.dimensions().width(),
        static_cast<float>(dst_surfaces[plane]->height()) /
            dst_yuva_info.dimensions().height(),
    };

    if (clear_destination && dst_image_rect != dst_rect) {
      // If we were told to clear the destination prior to blitting and we know
      // the blit won't populate the entire destination image, issue the draw
      // call that fills the destination with black and takes into account the
      // color conversion needed.
      SkPaint clear_paint(paint);
      clear_paint.setColor(SK_ColorBLACK);

      plane_canvas->drawPaint(clear_paint);
    }

    const SkRect plane_dst_rect =
        GetSubsampledRect(dst_rect, subsampling_factors);
    plane_canvas->drawImageRect(src_image, src_rect, plane_dst_rect,
                                sampling_options, &paint,
                                SkCanvas::kFast_SrcRectConstraint);
  }
}

}  // namespace skia
