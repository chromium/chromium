// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_WATERMARK_OVERLAYER_H_
#define PDF_PDFIUM_PDFIUM_WATERMARK_OVERLAYER_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "pdf/pdf_watermark_overlayer.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "ui/gfx/geometry/size_f.h"

namespace chrome_pdf {

class PdfiumWatermarkOverlayer : public PdfWatermarkOverlayer {
 public:
  // Creates a `PdfiumWatermarkOverlayer` for `pdf_buffer`.
  // Note: `pdf_buffer` must remain valid for the lifetime of this object.
  // Returns nullptr if `pdf_buffer` is not a valid PDF or has no pages.
  static std::unique_ptr<PdfiumWatermarkOverlayer> Create(
      base::span<const uint8_t> pdf_buffer);

  PdfiumWatermarkOverlayer(const PdfiumWatermarkOverlayer&) = delete;
  PdfiumWatermarkOverlayer& operator=(const PdfiumWatermarkOverlayer&) = delete;
  ~PdfiumWatermarkOverlayer() override;

  // PdfWatermarkOverlayer:
  std::vector<gfx::SizeF> GetPageSizes() const override;
  std::vector<uint8_t> OverlayPages(
      base::span<const uint8_t> overlay_pdf_buffer) override;

 private:
  class ScopedSdkInitializer {
   public:
    ScopedSdkInitializer();

    ScopedSdkInitializer(const ScopedSdkInitializer&) = delete;
    ScopedSdkInitializer& operator=(const ScopedSdkInitializer&) = delete;

    ~ScopedSdkInitializer();
  };

  PdfiumWatermarkOverlayer();
  bool Initialize(base::span<const uint8_t> pdf_buffer);

  ScopedSdkInitializer sdk_initializer_;
  ScopedFPDFDocument base_doc_;
  int page_count_ = 0;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_WATERMARK_OVERLAYER_H_

