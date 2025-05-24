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
  return DrawSelectionAndCompareImpl(engine, page_index,
                                     FILE_PATH_LITERAL("text_selection"),
                                     expected_png_filename,
                                     /*use_platform_suffix=*/false);
}

void PDFiumDrawSelectionTestBase::
    DrawSelectionAndCompareWithPlatformExpectations(
        PDFiumEngine& engine,
        int page_index,
        std::string_view expected_png_filename) {
  return DrawSelectionAndCompareImpl(engine, page_index,
                                     FILE_PATH_LITERAL("text_selection"),
                                     expected_png_filename,
                                     /*use_platform_suffix=*/true);
}

void PDFiumDrawSelectionTestBase::DrawHighlightsAndCompare(
    PDFiumEngine& engine,
    int page_index,
    std::string_view expected_png_filename) {
  return DrawSelectionAndCompareImpl(engine, page_index,
                                     FILE_PATH_LITERAL("text_fragments"),
                                     expected_png_filename,
                                     /*use_platform_suffix=*/false);
}

void PDFiumDrawSelectionTestBase::SetSelection(PDFiumEngine& engine,
                                               uint32_t start_page_index,
                                               uint32_t start_char_index,
                                               uint32_t end_page_index,
                                               uint32_t end_char_index) {
  engine.SetSelection({start_page_index, start_char_index},
                      {end_page_index, end_char_index});
}

void PDFiumDrawSelectionTestBase::DrawSelectionAndCompareImpl(
    PDFiumEngine& engine,
    int page_index,
    base::FilePath::StringViewType sub_directory,
    std::string_view expected_png_filename,
    bool use_platform_suffix) {
  // Since the GetPageContentsRect() return value may have a non-zero origin,
  // create a rect based solely on its size to draw the selections relative to
  // the origin of the contents rect.
  const auto rect = gfx::Rect(engine.GetPageContentsRect(page_index).size());
  ASSERT_TRUE(!rect.IsEmpty());

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(gfx::SizeToSkISize(rect.size())));
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorWHITE);

  const size_t progressive_index = engine.StartPaint(page_index, rect);
  CHECK_EQ(0u, progressive_index);
  engine.DrawSelections(progressive_index, bitmap);
  // Effectively the same as how PDFiumEngine::FinishPaint() cleans up
  // `progressive_paints_`.
  engine.progressive_paints_.clear();

  base::FilePath expectation_path = GetReferenceFilePath(
      sub_directory, expected_png_filename, use_platform_suffix);

  EXPECT_TRUE(MatchesPngFile(bitmap.asImage().get(), expectation_path));
}

}  // namespace chrome_pdf
