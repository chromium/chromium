// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_
#define PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_

#include <stddef.h>
#include <stdint.h>

#include "build/build_config.h"
#include "pdf/pdf_engine.h"

namespace chrome_pdf {

class PDFiumEngineExports : public PDFEngineExports {
 public:
  PDFiumEngineExports();
  PDFiumEngineExports(const PDFiumEngineExports&) = delete;
  PDFiumEngineExports& operator=(const PDFiumEngineExports&) = delete;
  ~PDFiumEngineExports() override;

// PDFEngineExports:
#if BUILDFLAG(IS_CHROMEOS)
  std::vector<uint8_t> CreateFlattenedPdf(
      base::span<const uint8_t> input_buffer) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_WIN)
  bool RenderPDFPageToDC(base::span<const uint8_t> pdf_buffer,
                         int page_index,
                         const RenderingSettings& settings,
                         HDC dc) override;
  void SetPDFUsePrintMode(int mode) override;
#endif  // BUILDFLAG(IS_WIN)
  bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                             int page_index,
                             const RenderingSettings& settings,
                             void* bitmap_buffer) override;
  std::vector<uint8_t> ConvertPdfPagesToNupPdf(
      std::vector<base::span<const uint8_t>> input_buffers,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area) override;
  std::vector<uint8_t> ConvertPdfDocumentToNupPdf(
      base::span<const uint8_t> input_buffer,
      size_t pages_per_sheet,
      const gfx::Size& page_size,
      const gfx::Rect& printable_area) override;
  bool GetPDFDocInfo(base::span<const uint8_t> pdf_buffer,
                     int* page_count,
                     float* max_page_width) override;
  absl::optional<bool> IsPDFDocTagged(
      base::span<const uint8_t> pdf_buffer) override;
  base::Value GetPDFStructTreeForPage(base::span<const uint8_t> pdf_buffer,
                                      int page_index) override;
  absl::optional<gfx::SizeF> GetPDFPageSizeByIndex(
      base::span<const uint8_t> pdf_buffer,
      int page_index) override;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_
