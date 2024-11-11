// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_writer.h"

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/cfi_buildflags.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "pdf/pdf_ink_brush.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_ink_reader.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_helpers.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/intersects.h"
#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input_batch.h"
#include "third_party/ink/src/ink/strokes/stroke.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

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

std::unique_ptr<PdfInkBrush> CreateTestBrush() {
  return std::make_unique<PdfInkBrush>(PdfInkBrush::Type::kPen, SK_ColorRED,
                                       /*size=*/4.0f);
}

}  // namespace

using PDFiumInkWriterTest = PDFiumTestBase;

// TODO(crbug.com/377704081): Enable test for CFI.
#if BUILDFLAG(CFI_ICALL_CHECK)
#define MAYBE_BasicWriteAndRead DISABLED_BasicWriteAndRead
#else
#define MAYBE_BasicWriteAndRead BasicWriteAndRead
#endif
TEST_P(PDFiumInkWriterTest, MAYBE_BasicWriteAndRead) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  auto brush = CreateTestBrush();

  std::optional<ink::StrokeInputBatch> inputs =
      CreateInkInputBatch(kBasicInputs);
  ASSERT_TRUE(inputs.has_value());
  ink::Stroke stroke(brush->ink_brush(), inputs.value());
  std::vector<FPDF_PAGEOBJECT> results =
      WriteStrokeToPage(engine->doc(), page, stroke);
  EXPECT_EQ(1u, results.size());

  ASSERT_TRUE(FPDFPage_GenerateContent(page));

  std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
  ASSERT_TRUE(!saved_pdf_data.empty());

  CheckPdfRendering(saved_pdf_data,
                    /*page_index=*/0, gfx::Size(200, 200),
                    GetInkTestDataFilePath("ink_writer_basic.png"));

  // Load `saved_pdf_data` into `saved_engine` and get a handle to the one and
  // only page.
  TestClient saved_client;
  std::unique_ptr<PDFiumEngine> saved_engine =
      InitializeEngineFromData(&saved_client, std::move(saved_pdf_data));
  ASSERT_TRUE(saved_engine);
  ASSERT_EQ(saved_engine->GetNumberOfPages(), 1);
  PDFiumPage& saved_pdfium_page = GetPDFiumPageForTest(*saved_engine, 0);
  FPDF_PAGE saved_page = saved_pdfium_page.GetPage();
  ASSERT_TRUE(saved_page);

  // Complete the round trip and read the written PDF data back into memory as
  // an ink::ModeledShape. ReadV2InkPathsFromPageAsModeledShapes() is known to
  // be good because its unit tests reads from a real, known to be good Ink PDF.
  std::vector<ReadV2InkPathResult> saved_results =
      ReadV2InkPathsFromPageAsModeledShapes(saved_page);
  ASSERT_EQ(saved_results.size(), 1u);

  // Take the original and saved shapes and compare them. Note that
  // `saved_shape` does not have an outline, so just check they behave the same
  // way with ink::Intersects().
  const auto& shape = stroke.GetShape();
  const auto& saved_shape = saved_results[0].shape;

  // All point values below are in canonical coordinates, so no transform is
  // necessary.
  const auto no_transform = ink::AffineTransform::Identity();

  // Points at the corners do not intersect.
  EXPECT_FALSE(ink::Intersects(ink::Point{0, 0}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{0, 0}, saved_shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{266, 266}, shape, no_transform));
  EXPECT_FALSE(
      ink::Intersects(ink::Point{266, 266}, saved_shape, no_transform));

  // Points close to `shape`, that still do not intersect.
  EXPECT_FALSE(ink::Intersects(ink::Point{139, 51}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{139, 51}, saved_shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{128, 63}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{128, 63}, saved_shape, no_transform));

  // Points that do intersect.
  EXPECT_TRUE(ink::Intersects(ink::Point{139, 53}, shape, no_transform));
  EXPECT_TRUE(ink::Intersects(ink::Point{139, 53}, saved_shape, no_transform));
  EXPECT_TRUE(ink::Intersects(ink::Point{129, 63}, shape, no_transform));
  EXPECT_TRUE(ink::Intersects(ink::Point{129, 63}, saved_shape, no_transform));
}

TEST_P(PDFiumInkWriterTest, EmptyStroke) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  auto brush = CreateTestBrush();
  ink::Stroke unused_stroke(brush->ink_brush());
  std::vector<FPDF_PAGEOBJECT> results =
      WriteStrokeToPage(engine->doc(), page, unused_stroke);
  EXPECT_TRUE(results.empty());
}

TEST_P(PDFiumInkWriterTest, NoDocumentNoPage) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("blank.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  auto brush = CreateTestBrush();
  ink::Stroke unused_stroke(brush->ink_brush());
  std::vector<FPDF_PAGEOBJECT> results =
      WriteStrokeToPage(/*document=*/nullptr, /*page=*/nullptr, unused_stroke);
  EXPECT_TRUE(results.empty());
  results = WriteStrokeToPage(/*document=*/nullptr, page, unused_stroke);
  EXPECT_TRUE(results.empty());
  results = WriteStrokeToPage(engine->doc(), /*page=*/nullptr, unused_stroke);
  EXPECT_TRUE(results.empty());
}

// Don't be concerned about any slight rendering differences in AGG vs. Skia,
// covering one of these is sufficient for checking how data is written out.
INSTANTIATE_TEST_SUITE_P(All, PDFiumInkWriterTest, testing::Values(false));

}  // namespace chrome_pdf
