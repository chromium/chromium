// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_writer.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_helpers.h"
#include "printing/units.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

namespace {

constexpr PdfInkBrush::Params kBasicBrushParams = {SK_ColorRED, 4.0f};

base::FilePath GetReferenceFilePath(std::string_view test_filename) {
  return base::FilePath(FILE_PATH_LITERAL("pdfium_ink"))
      .AppendASCII(test_filename);
}

// Takes `pdf_data` and loads it using PDFium. Then renders the page at
// `page_index` to a bitmap of `size_in_points` and checks if it matches
// `expected_png_file`.
void CheckPdfRendering(base::span<const uint8_t> pdf_data,
                       int page_index,
                       const gfx::Size& size_in_points,
                       const base::FilePath& expected_png_file) {
  const gfx::Rect page_rect(size_in_points);
  SkBitmap page_bitmap;
  page_bitmap.allocPixels(
      SkImageInfo::Make(gfx::SizeToSkISize(page_rect.size()),
                        kBGRA_8888_SkColorType, kPremul_SkAlphaType));

  PDFiumEngineExports::RenderingSettings settings(
      gfx::Size(printing::kPointsPerInch, printing::kPointsPerInch), page_rect,
      /*fit_to_bounds=*/false,
      /*stretch_to_bounds=*/false,
      /*keep_aspect_ratio=*/true,
      /*center_in_bounds=*/false,
      /*autorotate=*/false, /*use_color=*/true, /*render_for_printing=*/false);

  PDFiumEngineExports exports;
  ASSERT_TRUE(exports.RenderPDFPageToBitmap(pdf_data, page_index, settings,
                                            page_bitmap.getPixels()));

  EXPECT_TRUE(MatchesPngFile(page_bitmap.asImage().get(), expected_png_file));
}

}  // namespace

using PDFiumInkWriterTest = PDFiumTestBase;

TEST_P(PDFiumInkWriterTest, Basic) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage* pdfium_page = engine->GetPage(0);
  ASSERT_TRUE(pdfium_page);
  FPDF_PAGE page = pdfium_page->GetPage();
  ASSERT_TRUE(page);

  auto brush =
      std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen, kBasicBrushParams);

  ink::Stroke stroke(brush->GetInkBrush());
  // TODO(crbug.com/335517469): Add some data to `stroke`.
  ASSERT_TRUE(WriteStrokeToPage(page, stroke));

  std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
  ASSERT_TRUE(!saved_pdf_data.empty());

  // TODO(crbug.com/335517469): The drawing should look different.
  CheckPdfRendering(saved_pdf_data,
                    /*page_number=*/0, gfx::Size(200, 200),
                    GetReferenceFilePath("basic.png"));
}

TEST_P(PDFiumInkWriterTest, NoPage) {
  auto brush =
      std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen, kBasicBrushParams);
  ink::Stroke unused_stroke(brush->GetInkBrush());
  ASSERT_FALSE(WriteStrokeToPage(/*page=*/nullptr, unused_stroke));
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumInkWriterTest, testing::Bool());

}  // namespace chrome_pdf
