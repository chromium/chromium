// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/rgba_to_yuva.h"

#include <array>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"

namespace skia {

namespace {

void ApplyColorMatrix(SkPixmap& pm, const std::array<float, 20>& m) {
  CHECK_EQ(pm.colorType(), kRGBA_F32_SkColorType);
  CHECK_NE(pm.alphaType(), kPremul_SkAlphaType);
  for (int y = 0; y < pm.height(); ++y) {
    for (int x = 0; x < pm.width(); ++x) {
      SkColor4f& c = *reinterpret_cast<SkColor4f*>(pm.writable_addr(x, y));
      // Only apply the RGB parts of matrix (the alpha channel will be
      // unchanged).
      c = {
          c.fR * m[0] + c.fG * m[1] + c.fB * m[2] + m[4],
          c.fR * m[5] + c.fG * m[6] + c.fB * m[7] + m[9],
          c.fR * m[10] + c.fG * m[11] + c.fB * m[12] + m[14],
          c.fA,
      };
    }
  }
}

}  // namespace

void BlitRGBAToYUVA(SkImage* src_image,
                    base::span<SkSurface* const> dst_surfaces,
                    const SkYUVAInfo& dst_yuva_info,
                    const SkRect& dst_region,
                    bool clear_destination,
                    const SkRect& src_region) {
  // Rectangle representing the entire destination image:
  const SkRect dst_image_rect = SkRect::Make(dst_yuva_info.dimensions());
  const SkRect src_rect = src_image && src_region.isEmpty()
                              ? SkRect::Make(src_image->bounds())
                              : src_region;
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
  std::array<SkColorMatrix, SkYUVAInfo::kMaxPlanes> permutation_matrices;
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

    auto [ssHoriz, ssVert] = dst_yuva_info.planeSubsamplingFactors(plane);
    const SkRect plane_dst_rect = SkRect::MakeXYWH(
        dst_rect.x() / ssHoriz, dst_rect.y() / ssVert,
        dst_rect.width() / ssHoriz, dst_rect.height() / ssVert);

    if (!src_image) {
      paint.setColor(SK_ColorBLACK);
      plane_canvas->drawRect(plane_dst_rect, paint);
    } else {
      if (clear_destination && dst_image_rect != dst_rect) {
        // If we were told to clear the destination prior to blitting and we
        // know the blit won't populate the entire destination image, issue the
        // draw call that fills the destination with black and takes into
        // account the color conversion needed.
        SkPaint clear_paint(paint);
        clear_paint.setColor(SK_ColorBLACK);

        plane_canvas->drawPaint(clear_paint);
      }

      SkCanvas::SrcRectConstraint constraint =
          SkCanvas::kFast_SrcRectConstraint;
      if (src_rect != SkRect::Make(src_image->bounds())) {
        constraint = SkCanvas::kStrict_SrcRectConstraint;
      }
      plane_canvas->drawImageRect(src_image, src_rect, plane_dst_rect,
                                  sampling_options, &paint, constraint);
    }
  }
}

void ConvertRGBAToOrFromYUVA(SkPixmap src_pm,
                             SkYUVColorSpace src_yuv_cs,
                             SkPixmap dst_pm,
                             SkYUVColorSpace dst_yuv_cs) {
  CHECK(src_pm.dimensions() == dst_pm.dimensions());
  if (dst_pm.alphaType() == kOpaque_SkAlphaType) {
    CHECK(src_pm.alphaType() == kOpaque_SkAlphaType);
  }
  const int h = src_pm.height();
  const int w = src_pm.width();
  const SkAlphaType alpha_type = src_pm.alphaType() == kOpaque_SkAlphaType
                                     ? kOpaque_SkAlphaType
                                     : kUnpremul_SkAlphaType;

  // If we need to do YUV to RGB conversion, we will do that in-place in
  // `src_rgb_row`.
  SkPixmap src_rgb_row;
  SkBitmap src_rgb_row_bm;
  std::array<float, 20> src_matrix;
  if (src_yuv_cs != kIdentity_SkYUVColorSpace) {
    src_rgb_row_bm.allocPixels(SkImageInfo::Make(
        w, 1, kRGBA_F32_SkColorType, alpha_type, src_pm.refColorSpace()));
    src_rgb_row = src_rgb_row_bm.pixmap();
    SkColorMatrix::YUVtoRGB(src_yuv_cs).getRowMajor(src_matrix.data());
  }

  // If we need to do RGB to YUV conversion, we will do that in-place in
  // `dst_rgb_row`.
  SkBitmap dst_rgb_row_bm;
  SkPixmap dst_rgb_row;
  std::array<float, 20> dst_matrix;
  if (dst_yuv_cs != kIdentity_SkYUVColorSpace) {
    dst_rgb_row_bm.allocPixels(SkImageInfo::Make(
        w, 1, kRGBA_F32_SkColorType, alpha_type, dst_pm.refColorSpace()));
    dst_rgb_row = dst_rgb_row_bm.pixmap();
    SkColorMatrix::RGBtoYUV(dst_yuv_cs).getRowMajor(dst_matrix.data());
  }

  // Process one row at a time.
  for (int y = 0; y < h; ++y) {
    // Extract the source and destination rows.
    SkPixmap src_yuv_row;
    SkPixmap dst_yuv_row;
    const auto row_rect = SkIRect::MakeXYWH(0, y, w, 1);
    CHECK(src_pm.extractSubset(&src_yuv_row, row_rect));
    CHECK(dst_pm.extractSubset(&dst_yuv_row, row_rect));

    // Let `src_rgb_row` be the source row in full-range RGB.
    if (src_yuv_cs == kIdentity_SkYUVColorSpace) {
      src_rgb_row = src_yuv_row;
    } else {
      CHECK(src_yuv_row.readPixels(src_rgb_row));
      ApplyColorMatrix(src_rgb_row, src_matrix);
    }

    // Write `dst_yuv_row`.
    if (dst_yuv_cs == kIdentity_SkYUVColorSpace) {
      CHECK(src_rgb_row.readPixels(dst_yuv_row));
    } else {
      src_rgb_row.readPixels(dst_rgb_row);
      ApplyColorMatrix(dst_rgb_row, dst_matrix);
      dst_rgb_row.readPixels(dst_yuv_row);
    }
  }
}

}  // namespace skia
