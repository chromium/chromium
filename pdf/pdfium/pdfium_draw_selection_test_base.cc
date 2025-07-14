// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_draw_selection_test_base.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

void PDFiumDrawSelectionTestBase::DrawSelectionAndCompare(
    PDFiumEngine& engine,
    int page_index,
    std::string_view expected_png_filename) {
  return DrawAndCompareImpl(engine, page_index,
                            FILE_PATH_LITERAL("text_selection"),
                            expected_png_filename,
                            /*use_platform_suffix=*/false,
                            /*draw_caret=*/false);
}

void PDFiumDrawSelectionTestBase::
    DrawSelectionAndCompareWithPlatformExpectations(
        PDFiumEngine& engine,
        int page_index,
        std::string_view expected_png_filename) {
  return DrawAndCompareImpl(engine, page_index,
                            FILE_PATH_LITERAL("text_selection"),
                            expected_png_filename,
                            /*use_platform_suffix=*/true,
                            /*draw_caret=*/false);
}

void PDFiumDrawSelectionTestBase::DrawHighlightsAndCompare(
    PDFiumEngine& engine,
    int page_index,
    std::string_view expected_png_filename) {
  return DrawAndCompareImpl(engine, page_index,
                            FILE_PATH_LITERAL("text_fragments"),
                            expected_png_filename,
                            /*use_platform_suffix=*/false,
                            /*draw_caret=*/false);
}

void PDFiumDrawSelectionTestBase::DrawCaretAndCompareWithPlatformExpectations(
    PDFiumEngine& engine,
    int page_index,
    std::string_view expected_png_filename) {
  return DrawAndCompareImpl(engine, page_index, FILE_PATH_LITERAL("caret"),
                            expected_png_filename,
                            /*use_platform_suffix=*/true,
                            /*draw_caret=*/true);
}

void PDFiumDrawSelectionTestBase::SetSelection(PDFiumEngine& engine,
                                               uint32_t start_page_index,
                                               uint32_t start_char_index,
                                               uint32_t end_page_index,
                                               uint32_t end_char_index) {
  engine.SetSelection({start_page_index, start_char_index},
                      {end_page_index, end_char_index});
}

void PDFiumDrawSelectionTestBase::DrawAndCompareImpl(
    PDFiumEngine& engine,
    int page_index,
    base::FilePath::StringViewType sub_directory,
    std::string_view expected_png_filename,
    bool use_platform_suffix,
    bool draw_caret) {
  const gfx::Size plugin_size = engine.plugin_size();

  gfx::Rect visible_page_rect = engine.GetPageContentsRect(page_index);
  visible_page_rect.Intersect(gfx::Rect(plugin_size));
  ASSERT_FALSE(visible_page_rect.IsEmpty());

  SkBitmap plugin_bitmap;
  plugin_bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(plugin_size)));
  SkCanvas canvas(plugin_bitmap);
  canvas.clear(SK_ColorWHITE);

  const size_t progressive_index =
      engine.StartPaint(page_index, visible_page_rect);
  CHECK_EQ(0u, progressive_index);
  if (draw_caret) {
    engine.DrawCaret(progressive_index, plugin_bitmap);
  }
  engine.DrawSelections(progressive_index, plugin_bitmap);
  // Effectively the same as how PDFiumEngine::FinishPaint() cleans up
  // `progressive_paints_`.
  engine.progressive_paints_.clear();

  base::FilePath expectation_path = GetReferenceFilePath(
      sub_directory, expected_png_filename, use_platform_suffix);

  SkBitmap page_bitmap;
  plugin_bitmap.extractSubset(&page_bitmap,
                              gfx::RectToSkIRect(visible_page_rect));
  EXPECT_TRUE(MatchesPngFile(page_bitmap.asImage().get(), expectation_path));
}

}  // namespace chrome_pdf
