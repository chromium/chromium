// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_reader.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "pdf/pdf_ink_constants.h"
#include "pdf/pdf_ink_text.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/pdfium/pdfium_test_helpers.h"
#include "pdf/test/pdf_ink_test_helpers.h"
#include "pdf/test/test_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/intersects.h"
#include "third_party/ink/src/ink/geometry/partitioned_mesh.h"
#include "third_party/ink/src/ink/geometry/point.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

using PDFiumInkReaderTest = PDFiumTestBase;

TEST_P(PDFiumInkReaderTest, Basic) {
  TestClient client(/*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("ink_v2.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPage(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  std::vector<ReadV2InkPathResult> results =
      ReadV2InkPathsFromPageAsModeledShapes(page);
  ASSERT_EQ(results.size(), 1u);

  // Make sure there is a page object and it is a path.
  EXPECT_TRUE(results[0].page_object);
  EXPECT_EQ(FPDFPageObj_GetType(results[0].page_object), FPDF_PAGEOBJ_PATH);

  // Test `shape` works with ink::Intersects(). All point values are in
  // canonical coordinates, so no transform is necessary.
  const auto no_transform = ink::AffineTransform::Identity();
  const auto& shape = results[0].shape;

  // Points at the corners do not intersect with `shape`.
  EXPECT_FALSE(ink::Intersects(ink::Point{0, 0}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{266, 266}, shape, no_transform));

  // Points close to `shape`, that still do not intersect.
  EXPECT_FALSE(ink::Intersects(ink::Point{132, 212}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{194, 204}, shape, no_transform));

  // Points that do intersect.
  EXPECT_TRUE(ink::Intersects(ink::Point{133, 212}, shape, no_transform));
  EXPECT_TRUE(ink::Intersects(ink::Point{194, 203}, shape, no_transform));
}

TEST_P(PDFiumInkReaderTest, NoPage) {
  std::vector<ReadV2InkPathResult> results =
      ReadV2InkPathsFromPageAsModeledShapes(/*page=*/nullptr);
  EXPECT_TRUE(results.empty());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumInkReaderTest, testing::Bool());

class PDFiumInkReaderStrokeMarkedObjectsTests : public PDFiumInkReaderTest {
 public:
  void ValidateStrokeMarkedObjectsCount(
      const base::FilePath::CharType* pdf_name,
      int expected_count) {
    TestClient client(/*use_skia_renderer=*/GetParam());
    std::unique_ptr<PDFiumEngine> engine = InitializeEngine(&client, pdf_name);
    ASSERT_TRUE(engine);

    std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
    ASSERT_FALSE(saved_pdf_data.empty());

    TestClient saved_client(/*use_skia_renderer=*/GetParam());
    std::unique_ptr<PDFiumEngine> saved_engine =
        InitializeEngineFromData(&saved_client, std::move(saved_pdf_data));
    ASSERT_TRUE(saved_engine);

    ASSERT_TRUE(saved_engine->doc());
    EXPECT_EQ(GetPdfMarkObjCountForTesting(saved_engine->doc(),
                                           kInkAnnotationIdentifierKeyV2),
              expected_count);
  }
};

TEST_P(PDFiumInkReaderStrokeMarkedObjectsTests, MarkedObjectsNoStrokeData) {
  ValidateStrokeMarkedObjectsCount(FILE_PATH_LITERAL("blank.pdf"),
                                   /*expected_count=*/0);
}

TEST_P(PDFiumInkReaderStrokeMarkedObjectsTests, MarkedObjectsHasStrokeData) {
  ValidateStrokeMarkedObjectsCount(FILE_PATH_LITERAL("ink_v2.pdf"),
                                   /*expected_count=*/1);
}

// There are no rendering concerns for counting marked objects, so only one
// variation need be run.
INSTANTIATE_TEST_SUITE_P(All,
                         PDFiumInkReaderStrokeMarkedObjectsTests,
                         testing::Values(false));

TEST_P(PDFiumInkReaderTest, BasicTextAnnotation) {
  TestClient client(/*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("ink_text.pdf"));
  ASSERT_TRUE(engine);

  constexpr int kPageIndex = 0;
  PDFiumPage& pdfium_page = GetPDFiumPage(*engine, kPageIndex);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  std::vector<InkTextBox> results = ReadInkTextAnnotationsFromPage(page);
  ASSERT_EQ(1u, results.size());

  EXPECT_EQ(0, results[0].id);
  EXPECT_EQ("Hello\n!", results[0].attributes.text);

  EXPECT_THAT(results[0].attributes,
              InkTextBoxAttributesEq(
                  gfx::RectF(25.333334f, 125.333336f, 133.33334f, 66.66667f),
                  SK_ColorBLACK, 10.0f, TextTypeface::kSansSerif,
                  TextAlignment::kLeft, 0,
                  /*is_bold=*/true, /*is_italic=*/false, "Hello\n!"));
}

TEST_P(PDFiumInkReaderTest, InvalidTextAnnotation) {
  TestClient client(/*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("ink_text_invalid.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  constexpr int kPageIndex = 0;
  PDFiumPage& pdfium_page = GetPDFiumPage(*engine, kPageIndex);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  // The PDF has multiple different objects with invalid or missing parameters,
  // so the parser should safely skip all of them and return empty results.
  std::vector<InkTextBox> results = ReadInkTextAnnotationsFromPage(page);
  EXPECT_TRUE(results.empty());
}

TEST_P(PDFiumInkReaderTest, MultipleTextboxesOnOnePage) {
  TestClient client(/*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("ink_text_multi_textboxes.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(1, engine->GetNumberOfPages());

  constexpr int kPageIndex = 0;
  PDFiumPage& pdfium_page = GetPDFiumPage(*engine, kPageIndex);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  std::vector<InkTextBox> results = ReadInkTextAnnotationsFromPage(page);
  ASSERT_EQ(2u, results.size());

  // Textbox 0.
  EXPECT_EQ(0, results[0].id);
  EXPECT_EQ("Hello", results[0].attributes.text);
  EXPECT_THAT(results[0].attributes,
              InkTextBoxAttributesEq(
                  gfx::RectF(25.333334f, 125.333336f, 133.33334f, 66.66667f),
                  SK_ColorBLACK, 10.0f, TextTypeface::kSansSerif,
                  TextAlignment::kLeft, 0,
                  /*is_bold=*/true, /*is_italic=*/false, "Hello"));

  // Textbox 1.
  EXPECT_EQ(42, results[1].id);
  EXPECT_EQ("World", results[1].attributes.text);
  EXPECT_THAT(results[1].attributes,
              InkTextBoxAttributesEq(
                  gfx::RectF(25.333334f, 186.66667f, 133.33334f, 66.66667f),
                  SK_ColorBLUE, 15.0f, TextTypeface::kMonospace,
                  TextAlignment::kLeft, 0,
                  /*is_bold=*/false, /*is_italic=*/true, "World"));
}

}  // namespace chrome_pdf
