// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_test_helpers.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "pdf/test/test_helpers.h"
#include "printing/units.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

namespace {

int GetPageObjectMarksCount(FPDF_PAGEOBJECT page_obj,
                            std::u16string_view mark_name) {
  int matches_count = 0;
  int marks_count = FPDFPageObj_CountMarks(page_obj);
  for (int mark_index = 0; mark_index < marks_count; ++mark_index) {
    FPDF_PAGEOBJECTMARK mark_obj = FPDFPageObj_GetMark(page_obj, mark_index);
    CHECK(mark_obj);

    if (mark_name == GetPageObjectMarkName(mark_obj)) {
      ++matches_count;
    }
  }

  return matches_count;
}

}  // namespace

int GetPdfMarkObjCountForTesting(FPDF_DOCUMENT document,
                                 std::string_view mark_name) {
  const std::u16string pdf_mark_name = base::UTF8ToUTF16(mark_name);
  int matches_count = 0;
  // `doc_page_count` will be zero if document is null.
  int doc_page_count = FPDF_GetPageCount(document);

  for (int page_index = 0; page_index < doc_page_count; ++page_index) {
    ScopedFPDFPage page(FPDF_LoadPage(document, page_index));

    int page_obj_count = FPDFPage_CountObjects(page.get());
    for (int page_obj_index = 0; page_obj_index < page_obj_count;
         ++page_obj_index) {
      FPDF_PAGEOBJECT page_obj = FPDFPage_GetObject(page.get(), page_obj_index);
      CHECK(page_obj);

      matches_count += GetPageObjectMarksCount(page_obj, pdf_mark_name);
    }
  }

  return matches_count;
}

// Renders `page` to a bitmap of `size_in_points` and checks if it matches
// `expected_png_file`.
void CheckPdfRendering(FPDF_PAGE page,
                       const gfx::Size& size_in_points,
                       const base::FilePath& expected_png_file) {
  SkBitmap page_bitmap;
  page_bitmap.allocPixels(SkImageInfo::Make(gfx::SizeToSkISize(size_in_points),
                                            kBGRA_8888_SkColorType,
                                            kPremul_SkAlphaType));

  PDFiumEngineExports::RenderingSettings settings(
      gfx::Size(printing::kPointsPerInch, printing::kPointsPerInch),
      gfx::Rect(size_in_points),
      /*fit_to_bounds=*/false,
      /*stretch_to_bounds=*/false,
      /*keep_aspect_ratio=*/true,
      /*center_in_bounds=*/false,
      /*autorotate=*/false, /*use_color=*/true, /*render_for_printing=*/false);

  ASSERT_TRUE(RenderPageToBitmap(page, settings, page_bitmap.getPixels()));

  EXPECT_TRUE(MatchesPngFile(page_bitmap.asImage().get(), expected_png_file));
}

}  // namespace chrome_pdf
