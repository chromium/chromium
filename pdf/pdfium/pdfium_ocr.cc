// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ocr.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace chrome_pdf {

namespace {

// Maximum DPI needed for OCR.
constexpr float kMaxNeededDPI = 300.0f;
constexpr float kMaxNeededPixelToPointRatio =
    kMaxNeededDPI / printing::kPointsPerInch;

}  // namespace

gfx::SizeF GetImageSize(FPDF_PAGEOBJECT page_object) {
  float left;
  float bottom;
  float right;
  float top;
  if (!FPDFPageObj_GetBounds(page_object, &left, &bottom, &right, &top)) {
    return gfx::SizeF();
  }

  return gfx::SizeF(right - left, top - bottom);
}

SkBitmap GetImageForOcr(FPDF_DOCUMENT doc,
                        FPDF_PAGE page,
                        FPDF_PAGEOBJECT page_object) {
  SkBitmap bitmap;

  if (FPDFPageObj_GetType(page_object) != FPDF_PAGEOBJ_IMAGE) {
    return bitmap;
  }

  // OCR needs the image with at most `kMaxNeededDPI` dpi resolution. To get it,
  // the image transform matrix is set to an appropriate scale, the bitmap is
  // extracted, and then the original matrix is restored.
  FS_MATRIX original_matrix;
  if (!FPDFPageObj_GetMatrix(page_object, &original_matrix)) {
    DLOG(ERROR) << "Failed to get original matrix";
    return bitmap;
  }

  // Get the actual image size.
  unsigned int pixel_width;
  unsigned int pixel_height;
  if (!FPDFImageObj_GetImagePixelSize(page_object, &pixel_width,
                                      &pixel_height)) {
    DLOG(ERROR) << "Failed to get image size";
    return bitmap;
  }
  if (!pixel_width || !pixel_height) {
    return bitmap;
  }

  // Get minimum of horizontal and vertical pixel to point ratios.
  gfx::SizeF point_size = GetImageSize(page_object);
  if (point_size.IsEmpty()) {
    return bitmap;
  }
  float pixel_to_point_ratio = std::min(pixel_width / point_size.width(),
                                        pixel_height / point_size.height());

  // Reduce size if DPI is above need.
  float effective_width;
  float effective_height;
  if (pixel_to_point_ratio > kMaxNeededPixelToPointRatio) {
    float reduction_ratio = kMaxNeededPixelToPointRatio / pixel_to_point_ratio;
    effective_width = pixel_width * reduction_ratio;
    effective_height = pixel_height * reduction_ratio;
  } else {
    effective_width = pixel_width;
    effective_height = pixel_height;
  }

  // Scale the matrix to get image with highest resolution and keep the
  // rotation. If image is stretched differently in horizontal and vertical
  // directions, the one with no enlargement of the original height and width is
  // selected.
  float width_scale = hypotf(original_matrix.a, original_matrix.c);
  float height_scale = hypotf(original_matrix.b, original_matrix.d);
  if (width_scale == 0 || height_scale == 0) {
    return bitmap;
  }
  float ratio =
      std::min(effective_width / width_scale, effective_height / height_scale);
  const FS_MATRIX new_matrix = {
      original_matrix.a * ratio, original_matrix.b * ratio,
      original_matrix.c * ratio, original_matrix.d * ratio,
      original_matrix.e,         original_matrix.f};

  if (!FPDFPageObj_SetMatrix(page_object, &new_matrix)) {
    DLOG(ERROR) << "Failed to set new matrix on image";
    return bitmap;
  }

  ScopedFPDFBitmap raw_bitmap(
      FPDFImageObj_GetRenderedBitmap(doc, page, page_object));

  // Restore the original matrix.
  CHECK(FPDFPageObj_SetMatrix(page_object, &original_matrix));

  if (!raw_bitmap) {
    DLOG(ERROR) << "Failed to get rendered bitmap";
    return bitmap;
  }

  CHECK_EQ(FPDFBitmap_GetFormat(raw_bitmap.get()), FPDFBitmap_BGRA);
  SkImageInfo info =
      SkImageInfo::Make(FPDFBitmap_GetWidth(raw_bitmap.get()),
                        FPDFBitmap_GetHeight(raw_bitmap.get()),
                        kBGRA_8888_SkColorType, kOpaque_SkAlphaType);
  const size_t row_bytes = FPDFBitmap_GetStride(raw_bitmap.get());
  SkPixmap pixels(info, FPDFBitmap_GetBuffer(raw_bitmap.get()), row_bytes);
  if (!bitmap.tryAllocPixels(info, row_bytes)) {
    DLOG(ERROR) << "Failed to allocate pixel memory";
    return bitmap;
  }
  bitmap.writePixels(pixels);

  return bitmap;
}

}  // namespace chrome_pdf
