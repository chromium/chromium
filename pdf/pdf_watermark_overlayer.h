// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_WATERMARK_OVERLAYER_H_
#define PDF_PDF_WATERMARK_OVERLAYER_H_

#include <vector>

#include "base/containers/span.h"
#include "ui/gfx/geometry/size_f.h"

namespace chrome_pdf {

// Stateful class that loads a base PDF document once into memory, queries its
// page dimensions, and overlays watermark PDF pages onto it.
class PdfWatermarkOverlayer {
 public:
  virtual ~PdfWatermarkOverlayer() = default;

  // Returns page dimensions in points for all pages in the loaded base PDF.
  // Returns an empty vector if parsing fails or if any page size is invalid.
  virtual std::vector<gfx::SizeF> GetPageSizes() const = 0;

  // Overlays `overlay_pdf_buffer` onto the loaded base PDF page-by-page.
  // `overlay_pdf_buffer` must contain the same number of pages as the base PDF.
  // Returns the serialized watermarked PDF bytes, or an empty vector if page
  // counts mismatch or if processing fails.
  virtual std::vector<uint8_t> OverlayPages(
      base::span<const uint8_t> overlay_pdf_buffer) = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_WATERMARK_OVERLAYER_H_

