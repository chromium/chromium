// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ocr.h"

#include <stddef.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace chrome_pdf {

SkBitmap GetImageForOcr(FPDF_DOCUMENT doc,
                        FPDF_PAGE page,
                        int page_object_index) {
  SkBitmap bitmap;

  FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, page_object_index);
  if (FPDFPageObj_GetType(page_object) != FPDF_PAGEOBJ_IMAGE) {
    return bitmap;
  }

  // OCR needs the image with the highest available quality. To get it, the
  // image transform matrix is reset to no-scale, the bitmap is extracted,
  // and then the original matrix is restored.
  FS_MATRIX original_matrix;
  if (!FPDFPageObj_GetMatrix(page_object, &original_matrix)) {
    DLOG(ERROR) << "Failed to get original matrix";
    return bitmap;
  }

  // Get the actual image size.
  unsigned int width;
  unsigned int height;
  if (!FPDFImageObj_GetImagePixelSize(page_object, &width, &height)) {
    DLOG(ERROR) << "Failed to get image size";
    return bitmap;
  }

  // Resize the matrix to actual size.
  FS_MATRIX new_matrix = {static_cast<float>(width),  0, 0,
                          static_cast<float>(height), 0, 0};
  if (!FPDFPageObj_SetMatrix(page_object, &new_matrix)) {
    DLOG(ERROR) << "Failed to set new matrix on image";
    return bitmap;
  }

  ScopedFPDFBitmap raw_bitmap(
      FPDFImageObj_GetRenderedBitmap(doc, page, page_object));

  if (!raw_bitmap) {
    DLOG(ERROR) << "Failed to get rendered bitmap";
    return bitmap;
  }

  // Restore the original matrix.
  CHECK(FPDFPageObj_SetMatrix(page_object, &original_matrix));

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
