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

  // TODO(crbug.com/353942910): Implement this function and update test
  // expectations.
  std::vector<ink::ModeledShape> shapes =
      ReadV2InkPathsFromPageAsModeledShapes(page);
  EXPECT_TRUE(shapes.empty());
}

TEST_P(PDFiumInkReaderTest, NoPage) {
  std::vector<ink::ModeledShape> shapes =
      ReadV2InkPathsFromPageAsModeledShapes(/*page=*/nullptr);
  EXPECT_TRUE(shapes.empty());
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumInkReaderTest, testing::Bool());

}  // namespace chrome_pdf
