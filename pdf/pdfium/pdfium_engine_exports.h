// Copyright 2018 The Chromium Authors. All rights reserved.
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
  ~PDFiumEngineExports() override;

// PDFEngineExports:
#if defined(OS_WIN)
  bool RenderPDFPageToDC(base::span<const uint8_t> pdf_buffer,
                         int page_number,
                         const RenderingSettings& settings,
                         HDC dc) override;
  void SetPDFEnsureTypefaceCharactersAccessible(
      PDFEnsureTypefaceCharactersAccessible func) override;

  void SetPDFUseGDIPrinting(bool enable) override;
  void SetPDFUsePrintMode(int mode) override;
#endif  // defined(OS_WIN)
  bool RenderPDFPageToBitmap(base::span<const uint8_t> pdf_buffer,
                             int page_number,
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
                     double* max_page_width) override;
  bool GetPDFPageSizeByIndex(base::span<const uint8_t> pdf_buffer,
                             int page_number,
                             double* width,
                             double* height) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PDFiumEngineExports);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ENGINE_EXPORTS_H_
