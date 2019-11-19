// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_PRINT_H_
#define PDF_PDFIUM_PDFIUM_PRINT_H_

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdfview.h"

struct PP_PdfPrintSettings_Dev;
struct PP_PrintSettings_Dev;
struct PP_PrintPageNumberRange_Dev;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace chrome_pdf {

class PDFiumEngine;

class PDFiumPrint {
 public:
  explicit PDFiumPrint(PDFiumEngine* engine);
  ~PDFiumPrint();

#if defined(OS_CHROMEOS)
  // Flattens the |doc|.
  // On success, returns the flattened version of |doc| as a vector.
  // On failure, returns an empty vector.
  static std::vector<uint8_t> CreateFlattenedPdf(ScopedFPDFDocument doc);
#endif  // defined(OS_CHROMEOS)

  static std::vector<uint32_t> GetPageNumbersFromPrintPageNumberRange(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count);

  // Performs N-up PDF generation for |doc| based on |pages_per_sheet|,
  // |page_size|, and |printable_area|.
  // On success, returns the N-up version of |doc| as a vector.
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
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count,
      const PP_PrintSettings_Dev& print_settings,
      const PP_PdfPrintSettings_Dev& pdf_print_settings,
      bool raster);

 private:
  ScopedFPDFDocument CreatePrintPdf(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count,
      const PP_PrintSettings_Dev& print_settings,
      const PP_PdfPrintSettings_Dev& pdf_print_settings);

  ScopedFPDFDocument CreateRasterPdf(
      ScopedFPDFDocument doc,
      const PP_PrintSettings_Dev& print_settings);

  ScopedFPDFDocument CreateSinglePageRasterPdf(
      FPDF_PAGE page_to_print,
      const PP_PrintSettings_Dev& print_settings);

  PDFiumEngine* const engine_;

  DISALLOW_COPY_AND_ASSIGN(PDFiumPrint);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_PRINT_H_
