// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_transform.h"

#include <memory>

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/pdfium/pdfium_test_helpers.h"
#include "pdf/test/test_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace chrome_pdf {

namespace {

constexpr gfx::PointF kCanonicalTopLeftPoint(0.0f, 0.0f);
constexpr gfx::PointF kCanonicalMiddlePoint(100.0f, 50.0f);

}  // namespace

using PDFiumInkTransformTest = PDFiumTestBase;

TEST_P(PDFiumInkTransformTest, GetCanonicalToPdfTransformForHelloWorld) {
  TestClient client(/*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPage(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  const gfx::Transform transform = GetCanonicalToPdfTransformForPage(page);
  EXPECT_EQ(gfx::PointF(0.0f, 200.0f),
            transform.MapPoint(kCanonicalTopLeftPoint));
  EXPECT_EQ(gfx::PointF(75.0f, 162.5f),
            transform.MapPoint(kCanonicalMiddlePoint));
}

TEST_P(PDFiumInkTransformTest, GetCanonicalToPdfTransformForHelloWorldCropped) {
  TestClient client(/*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPage(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  const gfx::Transform transform = GetCanonicalToPdfTransformForPage(page);
  EXPECT_EQ(gfx::PointF(55.0f, 97.0f),
            transform.MapPoint(kCanonicalTopLeftPoint));
  EXPECT_EQ(gfx::PointF(130.0f, 59.5f),
            transform.MapPoint(kCanonicalMiddlePoint));
}

TEST(PDFiumInkTransformCalculateTest, CalculateTextBoxTransform) {
  gfx::Transform identity;
  gfx::RectF rect(10.0f, 20.0f, 100.0f, 50.0f);

  {
    FS_MATRIX matrix = CalculateTextBoxTransform(rect, 0, identity);
    EXPECT_FLOAT_EQ(matrix.a, 1.3333333f);
    EXPECT_FLOAT_EQ(matrix.b, 0.0f);
    EXPECT_FLOAT_EQ(matrix.c, 0.0f);
    EXPECT_FLOAT_EQ(matrix.d, -1.3333333f);
    EXPECT_FLOAT_EQ(matrix.e, 10.0f);
    EXPECT_FLOAT_EQ(matrix.f, 20.0f);
  }
  {
    FS_MATRIX matrix = CalculateTextBoxTransform(rect, 1, identity);
    EXPECT_FLOAT_EQ(matrix.a, 0.0f);
    EXPECT_FLOAT_EQ(matrix.b, 1.3333333f);
    EXPECT_FLOAT_EQ(matrix.c, 1.3333333f);
    EXPECT_FLOAT_EQ(matrix.d, 0.0f);
    EXPECT_FLOAT_EQ(matrix.e, 110.0f);
    EXPECT_FLOAT_EQ(matrix.f, 20.0f);
  }
  {
    FS_MATRIX matrix = CalculateTextBoxTransform(rect, 2, identity);
    EXPECT_FLOAT_EQ(matrix.a, -1.3333333f);
    EXPECT_FLOAT_EQ(matrix.b, 0.0f);
    EXPECT_FLOAT_EQ(matrix.c, 0.0f);
    EXPECT_FLOAT_EQ(matrix.d, 1.3333333f);
    EXPECT_FLOAT_EQ(matrix.e, 110.0f);
    EXPECT_FLOAT_EQ(matrix.f, 70.0f);
  }
  {
    FS_MATRIX matrix = CalculateTextBoxTransform(rect, 3, identity);
    EXPECT_FLOAT_EQ(matrix.a, 0.0f);
    EXPECT_FLOAT_EQ(matrix.b, -1.3333333f);
    EXPECT_FLOAT_EQ(matrix.c, -1.3333333f);
    EXPECT_FLOAT_EQ(matrix.d, 0.0f);
    EXPECT_FLOAT_EQ(matrix.e, 10.0f);
    EXPECT_FLOAT_EQ(matrix.f, 70.0f);
  }
}

// There are no rendering concerns for doing transforms, so only one variation
// needs be run.
INSTANTIATE_TEST_SUITE_P(All, PDFiumInkTransformTest, testing::Values(false));

}  // namespace chrome_pdf
