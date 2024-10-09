// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_writer.h"

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_helpers.h"
#include "printing/units.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

namespace {

constexpr PdfInkBrush::Params kBasicBrushParams = {SK_ColorRED, 4.0f};

constexpr auto kBasicInputs = std::to_array<PdfInkInputData>({
    {{126.122f, 52.852f}, base::Seconds(0.0f)},
    {{127.102f, 52.2398f}, base::Seconds(0.031467f)},
    {{130.041f, 50.7704f}, base::Seconds(0.07934f)},
    {{132.49f, 50.2806f}, base::Seconds(0.11225f)},
    {{133.714f, 49.7908f}, base::Seconds(0.143326f)},
    {{134.204f, 49.7908f}, base::Seconds(0.187606f)},
    {{135.184f, 49.7908f}, base::Seconds(0.20368f)},
    {{136.408f, 50.5255f}, base::Seconds(0.232364f)},
    {{137.143f, 52.2398f}, base::Seconds(0.261512f)},
    {{137.878f, 54.4439f}, base::Seconds(0.290249f)},
    {{137.878f, 55.9133f}, base::Seconds(0.316557f)},
    {{137.878f, 57.3827f}, base::Seconds(0.341756f)},
    {{137.143f, 58.852f}, base::Seconds(0.37093f)},
    {{136.408f, 59.8316f}, base::Seconds(0.39636f)},
    {{135.184f, 60.3214f}, base::Seconds(0.421022f)},
    {{134.694f, 60.3214f}, base::Seconds(0.450936f)},
    {{133.714f, 60.8112f}, base::Seconds(0.475798f)},
    {{132.245f, 60.8112f}, base::Seconds(0.501089f)},
    {{130.531f, 61.0561f}, base::Seconds(0.525835f)},
    {{130.041f, 61.301f}, base::Seconds(0.551003f)},
    {{129.306f, 61.301f}, base::Seconds(0.575968f)},
    {{128.816f, 61.301f}, base::Seconds(0.618475f)},
    {{128.327f, 61.0561f}, base::Seconds(0.634891f)},
    {{127.347f, 60.0765f}, base::Seconds(0.668079f)},
    {{126.612f, 59.0969f}, base::Seconds(0.692914f)},
    {{126.122f, 58.3622f}, base::Seconds(0.718358f)},
    {{125.878f, 57.1378f}, base::Seconds(0.743602f)},
    {{125.388f, 55.9133f}, base::Seconds(0.768555f)},
    {{125.143f, 54.6888f}, base::Seconds(0.794048f)},
    {{125.143f, 54.199f}, base::Seconds(0.819457f)},
    {{125.143f, 53.7092f}, base::Seconds(0.851297f)},
    {{125.388f, 53.4643f}, base::Seconds(0.901739f)},
    {{125.633f, 53.2194f}, base::Seconds(0.951174f)},
    {{125.878f, 53.2194f}, base::Seconds(0.985401f)},
});

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

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  auto brush =
      std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen, kBasicBrushParams);

  std::optional<ink::StrokeInputBatch> inputs =
      CreateInkInputBatch(kBasicInputs);
  ASSERT_TRUE(inputs.has_value());
  ink::Stroke stroke(brush->ink_brush(), inputs.value());
  ASSERT_TRUE(WriteStrokeToPage(engine->doc(), page, stroke));

  ASSERT_TRUE(FPDFPage_GenerateContent(page));

  std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
  ASSERT_TRUE(!saved_pdf_data.empty());

  CheckPdfRendering(saved_pdf_data,
                    /*page_number=*/0, gfx::Size(200, 200),
                    GetReferenceFilePath("basic.png"));
}

TEST_P(PDFiumInkWriterTest, EmptyStroke) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  auto brush =
      std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen, kBasicBrushParams);
  ink::Stroke unused_stroke(brush->ink_brush());
  ASSERT_FALSE(WriteStrokeToPage(engine->doc(), page, unused_stroke));
}

TEST_P(PDFiumInkWriterTest, NoDocumentNoPage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  auto brush =
      std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen, kBasicBrushParams);
  ink::Stroke unused_stroke(brush->ink_brush());
  ASSERT_FALSE(
      WriteStrokeToPage(/*document=*/nullptr, /*page=*/nullptr, unused_stroke));
  ASSERT_FALSE(WriteStrokeToPage(/*document=*/nullptr, page, unused_stroke));
  ASSERT_FALSE(
      WriteStrokeToPage(engine->doc(), /*page=*/nullptr, unused_stroke));
}

// Don't be concerned about any slight rendering differences in AGG vs. Skia,
// covering one of these is sufficient for checking how data is written out.
INSTANTIATE_TEST_SUITE_P(All, PDFiumInkWriterTest, testing::Values(false));

}  // namespace chrome_pdf
