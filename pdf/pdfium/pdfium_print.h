// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PRINT_H_
#define PDF_PDFIUM_PDFIUM_PRINT_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace blink {
struct WebPrintParams;
}  // namespace blink

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace chrome_pdf {

class PDFiumEngine;
struct FlattenPdfResult;

class PDFiumPrint {
 public:
  explicit PDFiumPrint(PDFiumEngine* engine);
  PDFiumPrint(const PDFiumPrint&) = delete;
  PDFiumPrint& operator=(const PDFiumPrint&) = delete;
  ~PDFiumPrint();

#if BUILDFLAG(IS_CHROMEOS)
  // Flattens the `doc`.
  // On success, returns the flattened version of `doc` as a vector and the
  // number of pages inside FlattenPdfResult.
  // On failure, returns std::nullopt.
  static std::optional<FlattenPdfResult> CreateFlattenedPdf(
      ScopedFPDFDocument doc);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Performs N-up PDF generation for `doc` based on `pages_per_sheet`,
  // `page_size`, and `printable_area`.
  // On success, returns the N-up version of `doc` as a vector.
  // On failure, returns an empty vector.
  static std::vector<uint8_t> CreateNupPdf(ScopedFPDFDocument doc,
                                           size_t pages_per_sheet,
                                           const gfx::Size& page_size,
                                           const gfx::Rect& printable_area);

  // Check the source doc orientation.  Returns true if the doc is landscape.
  // For now the orientation of the doc is determined by its first page's
  // orientation.  Improvement can be added in the future to better determine
  // the orientation of the source docs that have mixed orientation.
  // TODO(xlou): rotate pages if the source doc has mixed orientation.  So that
  // the orientation of all pages of the doc are uniform.  Pages of square size
  // will not be rotated.
  static bool IsSourcePdfLandscape(FPDF_DOCUMENT doc);

  static void FitContentsToPrintableArea(FPDF_DOCUMENT doc,
                                         const gfx::Size& page_size,
                                         const gfx::Rect& printable_area);

  std::vector<uint8_t> PrintPagesAsPdf(
      const std::vector<int>& page_indices,
      const blink::WebPrintParams& print_params);

 private:
  ScopedFPDFDocument CreatePrintPdf(const std::vector<int>& page_indices,
                                    const blink::WebPrintParams& print_params);

  ScopedFPDFDocument CreateRasterPdf(ScopedFPDFDocument doc, int dpi);

  ScopedFPDFDocument CreateSinglePageRasterPdf(FPDF_PAGE page_to_print,
                                               int dpi);

  const raw_ptr<PDFiumEngine> engine_;
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PRINT_H_
