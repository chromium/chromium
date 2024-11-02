// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_reader.h"

#include <memory>
#include <vector>

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ink/src/ink/geometry/affine_transform.h"
#include "third_party/ink/src/ink/geometry/intersects.h"
#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/ink/src/ink/geometry/point.h"

namespace chrome_pdf {

using PDFiumInkReaderTest = PDFiumTestBase;

TEST_P(PDFiumInkReaderTest, Basic) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("ink_v2.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPage& pdfium_page = GetPDFiumPageForTest(*engine, 0);
  FPDF_PAGE page = pdfium_page.GetPage();
  ASSERT_TRUE(page);

  std::vector<ink::ModeledShape> shapes =
      ReadV2InkPathsFromPageAsModeledShapes(page);
  ASSERT_EQ(shapes.size(), 1u);

  // Test `shape` works with ink::Intersects(). All point values are in
  // canonical coordinates, so no transform is necessary.
  const auto no_transform = ink::AffineTransform::Identity();
  const auto& shape = shapes[0];

  // Points at the corners do not intersect with `shape`.
  EXPECT_FALSE(ink::Intersects(ink::Point{0, 0}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{266, 266}, shape, no_transform));

  // Points close to `shape`, that still do not intersect.
  EXPECT_FALSE(ink::Intersects(ink::Point{132, 212}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{194, 204}, shape, no_transform));

  // Points that do intersect.
  // TODO(crbug.com/353942910): These should return true once a tessellator is
  // available.
  EXPECT_FALSE(ink::Intersects(ink::Point{133, 212}, shape, no_transform));
  EXPECT_FALSE(ink::Intersects(ink::Point{194, 203}, shape, no_transform));
}

TEST_P(PDFiumInkReaderTest, NoPage) {
  std::vector<ink::ModeledShape> shapes =
      ReadV2InkPathsFromPageAsModeledShapes(/*page=*/nullptr);
  EXPECT_TRUE(shapes.empty());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumInkReaderTest, testing::Bool());

}  // namespace chrome_pdf
