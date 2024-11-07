// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_reader.h"

#include <memory>
#include <vector>

#include "base/cfi_buildflags.h"
#include "pdf/pdf_ink_constants.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/pdfium/pdfium_test_helpers.h"
#include "pdf/test/test_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/intersects.h"
#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/ink/src/ink/geometry/point.h"

namespace chrome_pdf {

using PDFiumInkReaderTest = PDFiumTestBase;

// TODO(crbug.com/377704081): Enable test for CFI.
#if BUILDFLAG(CFI_ICALL_CHECK)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST_P(PDFiumInkReaderTest, MAYBE_Basic) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("ink_v2.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
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
    TestClient client;
    std::unique_ptr<PDFiumEngine> engine = InitializeEngine(&client, pdf_name);
    ASSERT_TRUE(engine);

    std::vector<uint8_t> saved_pdf_data = engine->GetSaveData();
    ASSERT_FALSE(saved_pdf_data.empty());

    TestClient saved_client;
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

}  // namespace chrome_pdf
