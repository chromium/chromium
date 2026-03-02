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

// Read the specified row `y` to `dst_pixmap`. This is slow and should only be
// used in tests or in extreme edge cases.
void ReadYUVRow(const std::vector<SkPixmap>& src_pixmaps,
                const SkYUVAInfo& src_yuva_info,
                const size_t src_bit_depth,
                size_t y,
                SkPixmap& row_pm) {
  CHECK_EQ(row_pm.colorType(), kRGBA_F32_SkColorType);
  CHECK_NE(row_pm.alphaType(), kPremul_SkAlphaType);
  CHECK_EQ(row_pm.height(), 1);
  const size_t w = src_pixmaps[0].width();

  // Let sf be a scale factor to account for discrepancies between the source
  // pixel format bit depth and the specified `src_bit_depth`. Also CHECK that
  // plane pixel formats are consistent.
  float sf = 1.f;
  switch (src_pixmaps[0].colorType()) {
    case kAlpha_8_SkColorType:
    case kA16_unorm_SkColorType:
    case kA16_float_SkColorType:
      NOTREACHED();
      break;
    case kR8_unorm_SkColorType:
      for (const auto& plane_pm : src_pixmaps) {
        CHECK(plane_pm.colorType() == kR8_unorm_SkColorType ||
              plane_pm.colorType() == kR8G8_unorm_SkColorType);
      }
      sf = ((1 << 8) - 1.f) / ((1 << src_bit_depth) - 1.f);
      break;
    case kR16_unorm_SkColorType:
      for (const auto& plane_pm : src_pixmaps) {
        CHECK(plane_pm.colorType() == kR16_unorm_SkColorType ||
              plane_pm.colorType() == kR16G16_unorm_SkColorType);
      }
      sf = ((1 << 16) - 1.f) / ((1 << src_bit_depth) - 1.f);
      break;
    default:
      break;
  }

  // Read the row, pixel-by-pixel. This is written for readability and not
  // speed.
  for (size_t x = 0; x < w; ++x) {
    // Sample the planes.
    std::array<SkColor4f, SkYUVAInfo::kMaxPlanes> planes;
    for (int p = 0; p < src_yuva_info.numPlanes(); ++p) {
      const auto [ssx, ssy] = src_yuva_info.planeSubsamplingFactors(p);
      planes[p] = src_pixmaps[p].getColor4f(x / ssx, y / ssy);

      // Scale to adjust for the input bits per pixel.
      planes[p] = {
          sf * planes[p].fR,
          sf * planes[p].fG,
          sf * planes[p].fB,
          sf * planes[p].fA,
      };
    }
    // Swizzle them to get the YUVA value.
    SkColor4f src_yuva = {0.f, 0.f, 0.f, 1.f};
    switch (src_yuva_info.planeConfig()) {
      case SkYUVAInfo::PlaneConfig::kY_U_V:
        src_yuva = {planes[0].fR, planes[1].fR, planes[2].fR, 1.f};
        break;
      case SkYUVAInfo::PlaneConfig::kY_V_U:
        src_yuva = {planes[0].fR, planes[2].fR, planes[1].fR, 1.f};
        break;
      case SkYUVAInfo::PlaneConfig::kY_UV:
        src_yuva = {planes[0].fR, planes[1].fR, planes[1].fG, 1.f};
        break;
      case SkYUVAInfo::PlaneConfig::kY_VU:
        src_yuva = {planes[0].fR, planes[1].fG, planes[1].fR, 1.f};
        break;
      case SkYUVAInfo::PlaneConfig::kYUV:
        src_yuva = {planes[0].fR, planes[0].fG, planes[0].fB, 1.f};
        break;
      case SkYUVAInfo::PlaneConfig::kUYV:
        src_yuva = {planes[0].fG, planes[0].fR, planes[0].fB, 1.f};
        break;
      case SkYUVAInfo::PlaneConfig::kY_U_V_A:
        src_yuva = {planes[0].fR, planes[1].fR, planes[2].fR, planes[3].fR};
        break;
      case SkYUVAInfo::PlaneConfig::kY_V_U_A:
        src_yuva = {planes[0].fR, planes[2].fR, planes[1].fR, planes[3].fR};
        break;
      case SkYUVAInfo::PlaneConfig::kY_UV_A:
        src_yuva = {planes[0].fR, planes[1].fR, planes[1].fG, planes[2].fR};
        break;
      case SkYUVAInfo::PlaneConfig::kY_VU_A:
        src_yuva = {planes[0].fR, planes[1].fG, planes[1].fR, planes[2].fR};
        break;
      case SkYUVAInfo::PlaneConfig::kYUVA:
        src_yuva = {planes[0].fR, planes[0].fG, planes[0].fB, planes[0].fA};
        break;
      case SkYUVAInfo::PlaneConfig::kUYVA:
        src_yuva = {planes[0].fG, planes[0].fR, planes[0].fB, planes[0].fA};
        break;
      case SkYUVAInfo::PlaneConfig::kUnknown:
        NOTREACHED();
    }
    *reinterpret_cast<SkColor4f*>(row_pm.writable_addr(x, 0)) = src_yuva;
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
  constexpr SkColorMatrix YxxA(1, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 1, 0);
  constexpr SkColorMatrix UxxA(0, 1, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 1, 0);
  constexpr SkColorMatrix VxxA(0, 0, 1, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 1, 0);
  constexpr SkColorMatrix AxxA(0, 0, 0, 1, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 1, 0);
  constexpr SkColorMatrix UVxA(0, 1, 0, 0, 0,  //
                               0, 0, 1, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 1, 0);
  constexpr SkColorMatrix VUxA(0, 0, 1, 0, 0,  //
                               0, 1, 0, 0, 0,  //
                               0, 0, 0, 0, 0,  //
                               0, 0, 0, 1, 0);
  constexpr SkColorMatrix YUVA(1, 0, 0, 0, 0,  //
                               0, 1, 0, 0, 0,  //
                               0, 0, 1, 0, 0,  //
                               0, 0, 0, 1, 0);
  constexpr SkColorMatrix UYVA(0, 1, 0, 0, 0,  //
                               1, 0, 0, 0, 0,  //
                               0, 0, 1, 0, 0,  //
                               0, 0, 0, 1, 0);

  // Only Y_UV has been tested.
  std::array<SkColorMatrix, SkYUVAInfo::kMaxPlanes> permutation_matrices;
  switch (dst_yuva_info.planeConfig()) {
    case SkYUVAInfo::PlaneConfig::kY_U_V:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = UxxA;
      permutation_matrices[2] = VxxA;
      break;
    case SkYUVAInfo::PlaneConfig::kY_V_U:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = VxxA;
      permutation_matrices[2] = UxxA;
      break;
    case SkYUVAInfo::PlaneConfig::kY_UV:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = UVxA;
      break;
    case SkYUVAInfo::PlaneConfig::kY_VU:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = VUxA;
      break;
    case SkYUVAInfo::PlaneConfig::kYUV:
      permutation_matrices[0] = YUVA;
      break;
    case SkYUVAInfo::PlaneConfig::kUYV:
      permutation_matrices[0] = UYVA;
      break;
    case SkYUVAInfo::PlaneConfig::kY_U_V_A:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = UxxA;
      permutation_matrices[2] = VxxA;
      permutation_matrices[3] = AxxA;
      break;
    case SkYUVAInfo::PlaneConfig::kY_V_U_A:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = VxxA;
      permutation_matrices[2] = UxxA;
      permutation_matrices[3] = AxxA;
      break;
    case SkYUVAInfo::PlaneConfig::kY_UV_A:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = UVxA;
      permutation_matrices[2] = AxxA;
      break;
    case SkYUVAInfo::PlaneConfig::kY_VU_A:
      permutation_matrices[0] = YxxA;
      permutation_matrices[1] = VUxA;
      permutation_matrices[2] = AxxA;
      break;
    case SkYUVAInfo::PlaneConfig::kYUVA:
      permutation_matrices[0] = YUVA;
      break;
    case SkYUVAInfo::PlaneConfig::kUYVA:
      permutation_matrices[0] = UYVA;
      break;
    case SkYUVAInfo::PlaneConfig::kUnknown:
      NOTREACHED();
  }
  SkColorMatrix rgb_to_yuv_matrix =
      SkColorMatrix::RGBtoYUV(dst_yuva_info.yuvColorSpace());

  // Blit each plane.
  for (int plane = 0; plane < dst_yuva_info.numPlanes(); ++plane) {
    // If there exists a separate alpha plane, then, when rendering to the
    // individual planes (which have R and RG pixel formats), ensure that
    // those pixel values are unpremultiplied.
    if (dst_yuva_info.hasAlpha() && dst_yuva_info.numPlanes() > 1 &&
        !src_image->isOpaque()) {
      CHECK_EQ(dst_surfaces[plane]->imageInfo().alphaType(),
               kUnpremul_SkAlphaType);
    }

    SkCanvas* plane_canvas = dst_surfaces[plane]->getCanvas();

    SkColorMatrix color_matrix = rgb_to_yuv_matrix;
    color_matrix.postConcat(permutation_matrices[plane]);

    SkSamplingOptions sampling_options(SkFilterMode::kLinear);

    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrc);

    auto filter = SkColorFilters::Matrix(color_matrix);
    if (!dst_yuva_info.hasAlpha()) {
      // Blend the input image over black before performing RGB to YUV
      // conversion, to match un-accelerated versions.
      filter = SkColorFilters::Compose(
          filter, SkColorFilters::Blend(SK_ColorBLACK, SkBlendMode::kDstOver));
    }
    paint.setColorFilter(filter);

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

void ConvertRGBAToYUVA(SkPixmap src_pm,
                       const SkYUVAInfo& dst_yuva_info,
                       const std::vector<SkPixmap>& dst_pixmaps) {
  CHECK(src_pm.dimensions() == dst_pixmaps[0].dimensions());
  CHECK(dst_yuva_info.isValid());
  const auto* dst_color_space = dst_pixmaps[0].colorSpace();

  auto src_image = SkImages::RasterFromPixmap(src_pm, nullptr, nullptr);
  std::array<sk_sp<SkSurface>, SkYUVAInfo::kMaxPlanes> dst_surfaces;
  std::array<SkSurface*, SkYUVAInfo::kMaxPlanes> dst_surface_ptrs;
  for (size_t p = 0; p < dst_pixmaps.size(); ++p) {
    SkPixmap pm = dst_pixmaps[p];
    // Color space conversion is from `src_pm`'s color space to the space of
    // `dst_pixmaps`. This is only coherent if all of `dst_pixmaps` have the
    // same color space.
    CHECK(SkColorSpace::Equals(dst_color_space, pm.colorSpace()));
    // If `dst_pixmaps` has a separate alpha channel, then the alpha type
    // of the individual planes for sampling isn't relevant (their alpha channel
    // will always sample 1). For rendering to the plane's SkPixmap as an
    // SkSurface, force its alpha type to unpremultiplied to ensure that
    // the values written to it are unpremultiplied.
    if (dst_yuva_info.hasAlpha() && dst_yuva_info.numPlanes() > 1 &&
        !src_pm.isOpaque()) {
      pm = SkPixmap(pm.info().makeAlphaType(kUnpremul_SkAlphaType), pm.addr(),
                    pm.rowBytes());
    }
    dst_surfaces[p] = SkSurfaces::WrapPixels(pm);
    dst_surface_ptrs[p] = dst_surfaces[p].get();
  }
  BlitRGBAToYUVA(src_image.get(),
                 base::span<SkSurface* const>(dst_surface_ptrs), dst_yuva_info);
}

void ConvertYUVAToRGBA(const SkYUVAInfo& src_yuva_info,
                       size_t src_bit_depth,
                       const std::vector<SkPixmap>& src_pixmaps,
                       const SkPixmap& dst_pixmap) {
  DCHECK(src_pixmaps[0].dimensions() == dst_pixmap.dimensions());
  const auto src_color_space = src_pixmaps[0].refColorSpace();
  const size_t w = static_cast<size_t>(dst_pixmap.width());
  const size_t h = static_cast<size_t>(dst_pixmap.height());

  // Just use ReadPixels for RGBA inputs.
  if (!src_yuva_info.isValid()) {
    CHECK_EQ(src_pixmaps.size(), 1u);
    CHECK(src_pixmaps[0].readPixels(dst_pixmap));
    return;
  }

  // Make sure we have the expected number of planes and dimensions of planes,
  // and that all planes have the same color space.
  CHECK_EQ(src_pixmaps.size(), static_cast<size_t>(src_yuva_info.numPlanes()));
  std::array<SkISize, SkYUVAInfo::kMaxPlanes> plane_dimensions;
  src_yuva_info.planeDimensions(plane_dimensions.data());
  for (size_t p = 0; p < src_pixmaps.size(); ++p) {
    CHECK(src_pixmaps[p].dimensions() == plane_dimensions[p]);
    CHECK(SkColorSpace::Equals(src_pixmaps[p].colorSpace(),
                               src_color_space.get()));
  }

  // Compute the YUV to RGB matrix, if needed.
  const bool use_src_yuv_to_rgb_matrix =
      src_yuva_info.yuvColorSpace() != kIdentity_SkYUVColorSpace;
  std::array<float, 20> src_yuv_to_rgb_matrix;
  if (use_src_yuv_to_rgb_matrix) {
    SkColorMatrix::YUVtoRGB(src_yuva_info.yuvColorSpace())
        .getRowMajor(src_yuv_to_rgb_matrix.data());
  }

  // Allocate a single unpremultiplied row buffer.
  SkBitmap src_row_bm;
  src_row_bm.allocPixels(SkImageInfo::Make(
      w, 1, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType, src_color_space));
  auto src_row = src_row_bm.pixmap();

  for (size_t y = 0; y < h; ++y) {
    const auto dst_row_rect = SkIRect::MakeXYWH(0, y, w, 1);
    SkPixmap dst_row;
    CHECK(dst_pixmap.extractSubset(&dst_row, dst_row_rect));

    // Read and resample `src_pixmaps` into `src_row`.
    ReadYUVRow(src_pixmaps, src_yuva_info, src_bit_depth, y, src_row);

    // Perform YUV to RGB conversion.
    if (use_src_yuv_to_rgb_matrix) {
      ApplyColorMatrix(src_row, src_yuv_to_rgb_matrix);
    }

    // Convert to `dst_pixmap` color space and pixel format.
    CHECK(src_row.readPixels(dst_row));
  }
}

}  // namespace skia
