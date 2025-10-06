// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_DRAW_SELECTION_TEST_BASE_H_
#define PDF_PDFIUM_PDFIUM_DRAW_SELECTION_TEST_BASE_H_

#include <stdint.h>

#include <string_view>

#include "base/files/file_path.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Size;
}

namespace chrome_pdf {

class PDFiumEngine;

// A subclass of PDFiumTestBase that offers methods for testing drawn
// selections, highlights, and carets.
class PDFiumDrawSelectionTestBase : public PDFiumTestBase {
 protected:
  // Compares the visible page contents in the plugin with the expected PNG.
  // The plugin size of `engine` must have a non-empty intersect with the page
  // contents rect of page `page_index`, i.e. the page must be visible,
  // otherwise the test will fail.
  void DrawSelectionAndCompare(PDFiumEngine& engine,
                               int page_index,
                               std::string_view expected_png_filename);

  // Same as `DrawSelectionAndCompare()`, but has different expected PNGs for
  // specific platforms.
  void DrawSelectionAndCompareWithPlatformExpectations(
      PDFiumEngine& engine,
      int page_index,
      std::string_view expected_png_filename);

  // Same as `DrawSelectionAndCompare()`, but for text highlights.
  void DrawHighlightsAndCompare(PDFiumEngine& engine,
                                int page_index,
                                std::string_view expected_png_filename);

  // Draws the caret, but also attempts to draw the selection, even though they
  // are mutually exclusive. Then compares the visible page contents in the
  // plugin with the expected PNG. This allows tests to verify that only the
  // caret is visible. The plugin size of `engine` must have a non-empty
  // intersect with the page contents rect of page `page_index`, i.e. the page
  // must be visible, otherwise the test will fail.
  void DrawCaretAndCompare(PDFiumEngine& engine,
                           int page_index,
                           std::string_view expected_png_filename);

  // Same as `DrawCaretAndCompare()`, but has different expected PNGs for
  // specific platforms.
  void DrawCaretAndCompareWithPlatformExpectations(
      PDFiumEngine& engine,
      int page_index,
      std::string_view expected_png_filename);

  // Draws selections and highlights. Then verifies the visible page contents in
  // the plugin is completely white and of size `expected_visible_page_size`.
  void DrawAndExpectBlank(PDFiumEngine& engine,
                          int page_index,
                          const gfx::Size& expected_visible_page_size);

  // Draws the caret, but also attempts to draw the selection, even though they
  // are mutually exclusive. Then verifies the page is white in the same manner
  // as `DrawAndExpectBlank()`.
  void DrawCaretAndExpectBlank(PDFiumEngine& engine,
                               int page_index,
                               const gfx::Size& expected_visible_page_size);

  void SetSelection(PDFiumEngine& engine,
                    uint32_t start_page_index,
                    uint32_t start_char_index,
                    uint32_t end_page_index,
                    uint32_t end_char_index);

 private:
  // Returns the page contents rect that is visible within the plugin.
  gfx::Rect GetVisiblePageContentsRect(PDFiumEngine& engine, int page_index);

  // Draws selections, highlights, and optionally carets if `draw_caret` is
  // true. Returns a bitmap of the portion of the page that is visible in the
  // plugin.
  SkBitmap Draw(PDFiumEngine& engine,
                int page_index,
                const gfx::Rect& visible_page_rect,
                bool draw_caret);

  void DrawAndCompareImpl(PDFiumEngine& engine,
                          int page_index,
                          base::FilePath::StringViewType sub_directory,
                          std::string_view expected_png_filename,
                          bool use_platform_suffix,
                          bool draw_caret);

  // Tests the rendering is blank after drawing selections, highlights, and
  // optionally carets if `draw_caret` is true.
  void TestDrawBlank(PDFiumEngine& engine,
                     int page_index,
                     const gfx::Size& expected_visible_page_size,
                     bool draw_caret);
};

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_DRAW_SELECTION_TEST_BASE_H_
