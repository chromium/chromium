// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_DRAW_SELECTION_TEST_BASE_H_
#define PDF_PDFIUM_PDFIUM_DRAW_SELECTION_TEST_BASE_H_

#include <stdint.h>

#include <string_view>

#include "base/files/file_path.h"
#include "pdf/pdfium/pdfium_test_base.h"

namespace chrome_pdf {

class PDFiumEngine;

// A subclass of PDFiumTestBase that offers methods for setting and testing
// drawn selections and highlights.
class PDFiumDrawSelectionTestBase : public PDFiumTestBase {
 protected:
  void DrawSelectionAndCompare(PDFiumEngine& engine,
                               int page_index,
                               std::string_view expected_png_filename);

  void DrawSelectionAndCompareWithPlatformExpectations(
      PDFiumEngine& engine,
      int page_index,
      std::string_view expected_png_filename);

  void DrawHighlightsAndCompare(PDFiumEngine& engine,
                                int page_index,
                                std::string_view expected_png_filename);

  void SetSelection(PDFiumEngine& engine,
                    uint32_t start_page_index,
                    uint32_t start_char_index,
                    uint32_t end_page_index,
                    uint32_t end_char_index);

 private:
  void DrawSelectionAndCompareImpl(PDFiumEngine& engine,
                                   int page_index,
                                   base::FilePath::StringViewType sub_directory,
                                   std::string_view expected_png_filename,
                                   bool use_platform_suffix);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_DRAW_SELECTION_TEST_BASE_H_
