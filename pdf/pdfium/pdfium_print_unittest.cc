// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_print.h"

#include <memory>

#include "base/stl_util.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_pdf {

using PDFiumPrintTest = PDFiumTestBase;
using testing::ElementsAre;

namespace {

constexpr PP_Size kUSLetterSize = {612, 792};
constexpr PP_Rect kUSLetterRect = {{0, 0}, kUSLetterSize};

struct SizeDouble {
  double width;
  double height;
};

using ExpectedDimensions = std::vector<SizeDouble>;

void CheckPdfDimensions(const std::vector<uint8_t>& pdf_data,
                        const ExpectedDimensions& expected_dimensions) {
  PDFiumEngineExports exports;
  int page_count;
  ASSERT_TRUE(exports.GetPDFDocInfo(pdf_data, &page_count, nullptr));
  ASSERT_GT(page_count, 0);
  ASSERT_EQ(expected_dimensions.size(), static_cast<size_t>(page_count));

  for (int i = 0; i < page_count; ++i) {
    double width;
    double height;
    ASSERT_TRUE(exports.GetPDFPageSizeByIndex(pdf_data, i, &width, &height));
    EXPECT_DOUBLE_EQ(expected_dimensions[i].width, width);
    EXPECT_DOUBLE_EQ(expected_dimensions[i].height, height);
  }
}

}  // namespace

TEST_F(PDFiumPrintTest, GetPageNumbersFromPrintPageNumberRange) {
  std::vector<uint32_t> page_numbers;

  {
    const PP_PrintPageNumberRange_Dev page_ranges[] = {{0, 2}};
    page_numbers = PDFiumPrint::GetPageNumbersFromPrintPageNumberRange(
        &page_ranges[0], base::size(page_ranges));
    EXPECT_THAT(page_numbers, ElementsAre(0, 1, 2));
  }
  {
    const PP_PrintPageNumberRange_Dev page_ranges[] = {{0, 0}, {2, 2}, {4, 5}};
    page_numbers = PDFiumPrint::GetPageNumbersFromPrintPageNumberRange(
        &page_ranges[0], base::size(page_ranges));
    EXPECT_THAT(page_numbers, ElementsAre(0, 2, 4, 5));
  }
}

TEST_F(PDFiumPrintTest, Basic) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPrint print(engine.get());

  constexpr PP_PrintSettings_Dev print_settings = {kUSLetterRect,
                                                   kUSLetterRect,
                                                   kUSLetterSize,
                                                   72,
                                                   PP_PRINTORIENTATION_NORMAL,
                                                   PP_PRINTSCALINGOPTION_NONE,
                                                   PP_FALSE,
                                                   PP_PRINTOUTPUTFORMAT_PDF};
  constexpr PP_PdfPrintSettings_Dev pdf_print_settings = {1, 100};

  {
    // Print 2 pages.
    const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0},
                                                    {612.0, 792.0}};
    const PP_PrintPageNumberRange_Dev page_ranges[] = {{0, 1}};
    std::vector<uint8_t> pdf_data =
        print.PrintPagesAsPdf(&page_ranges[0], base::size(page_ranges),
                              print_settings, pdf_print_settings,
                              /*raster=*/false);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);

    pdf_data = print.PrintPagesAsPdf(&page_ranges[0], base::size(page_ranges),
                                     print_settings, pdf_print_settings,
                                     /*raster=*/true);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
  }
  {
    // Print 1 page.
    const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
    const PP_PrintPageNumberRange_Dev page_ranges[] = {{0, 0}};
    std::vector<uint8_t> pdf_data =
        print.PrintPagesAsPdf(&page_ranges[0], base::size(page_ranges),
                              print_settings, pdf_print_settings,
                              /*raster=*/false);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);

    pdf_data = print.PrintPagesAsPdf(&page_ranges[0], base::size(page_ranges),
                                     print_settings, pdf_print_settings,
                                     /*raster=*/true);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
  }
  {
    // Print the other page.
    const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
    const PP_PrintPageNumberRange_Dev page_ranges[] = {{1, 1}};
    std::vector<uint8_t> pdf_data =
        print.PrintPagesAsPdf(&page_ranges[0], base::size(page_ranges),
                              print_settings, pdf_print_settings,
                              /*raster=*/false);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);

    pdf_data = print.PrintPagesAsPdf(&page_ranges[0], base::size(page_ranges),
                                     print_settings, pdf_print_settings,
                                     /*raster=*/true);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
  }
}

}  // namespace chrome_pdf
