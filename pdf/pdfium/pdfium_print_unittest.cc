// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_print.h"

#include <memory>

#include "base/hash/md5.h"
#include "base/stl_util.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/private/ppp_pdf.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_pdf {

using PDFiumPrintTest = PDFiumTestBase;
using testing::ElementsAre;

namespace {

// Number of color channels in a BGRA bitmap.
constexpr int kColorChannels = 4;

constexpr PP_Size kUSLetterSize = {612, 792};
constexpr PP_Rect kUSLetterRect = {{0, 0}, kUSLetterSize};
constexpr PP_Rect kPrintableAreaRect = {{18, 18}, {576, 733}};

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

void CheckPdfRendering(const std::vector<uint8_t>& pdf_data,
                       int page_number,
                       const SizeDouble& size_in_points,
                       const char* expected_md5_hash) {
  int width_in_pixels = printing::ConvertUnit(
      size_in_points.width, printing::kPointsPerInch, printing::kDefaultPdfDpi);
  int height_in_pixels =
      printing::ConvertUnit(size_in_points.height, printing::kPointsPerInch,
                            printing::kDefaultPdfDpi);

  const pp::Rect page_rect(width_in_pixels, height_in_pixels);
  std::vector<uint8_t> page_bitmap_data(kColorChannels * page_rect.width() *
                                        page_rect.height());

  PDFEngineExports::RenderingSettings settings(
      printing::kDefaultPdfDpi, printing::kDefaultPdfDpi, page_rect,
      /*fit_to_bounds=*/true,
      /*stretch_to_bounds=*/false,
      /*keep_aspect_ratio=*/true,
      /*center_in_bounds=*/true,
      /*autorotate=*/false, /*use_color=*/true);

  PDFiumEngineExports exports;
  ASSERT_TRUE(exports.RenderPDFPageToBitmap(pdf_data, page_number, settings,
                                            page_bitmap_data.data()));

  base::MD5Digest hash;
  base::MD5Sum(page_bitmap_data.data(), page_bitmap_data.size(), &hash);
  EXPECT_STREQ(expected_md5_hash, base::MD5DigestToBase16(hash).c_str());
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

TEST_F(PDFiumPrintTest, AlterScaling) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPrint print(engine.get());

  PP_PrintSettings_Dev print_settings = {kPrintableAreaRect,
                                         kUSLetterRect,
                                         kUSLetterSize,
                                         72,
                                         PP_PRINTORIENTATION_NORMAL,
                                         PP_PRINTSCALINGOPTION_NONE,
                                         PP_FALSE,
                                         PP_PRINTOUTPUTFORMAT_PDF};
  constexpr PP_PdfPrintSettings_Dev pdf_print_settings = {1, 100};
  const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
  constexpr PP_PrintPageNumberRange_Dev page_range = {0, 0};

  {
    // Default scaling
    static const char md5_hash[] = "40e2e16416015cdde5c6e5735c1d06ac";
    static const char md5_hash_raster[] = "c29b9ed661143ea7f177d7af8a336ef7";

    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(
        &page_range, 1, print_settings, pdf_print_settings,
        /*raster=*/false);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], md5_hash);

    pdf_data = print.PrintPagesAsPdf(&page_range, 1, print_settings,
                                     pdf_print_settings,
                                     /*raster=*/true);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], md5_hash_raster);
  }
  {
    // "Fit to Page" scaling
    print_settings.print_scaling_option =
        PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA;

    static const char md5_hash[] = "41847e1f0c581150a84794482528f790";
    static const char md5_hash_raster[] = "436354693512c8144ae51837ff9f951e";

    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(
        &page_range, 1, print_settings, pdf_print_settings,
        /*raster=*/false);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], md5_hash);

    pdf_data = print.PrintPagesAsPdf(&page_range, 1, print_settings,
                                     pdf_print_settings,
                                     /*raster=*/true);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], md5_hash_raster);
  }
  {
    // "Fit to Paper" scaling
    print_settings.print_scaling_option = PP_PRINTSCALINGOPTION_FIT_TO_PAPER;

    static const char md5_hash[] = "3a4828228bcbae230574c057b7a0669e";
    static const char md5_hash_raster[] = "8834ddfb3ef4483acf8da9d27d43cf1f";

    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(
        &page_range, 1, print_settings, pdf_print_settings,
        /*raster=*/false);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], md5_hash);

    pdf_data = print.PrintPagesAsPdf(&page_range, 1, print_settings,
                                     pdf_print_settings,
                                     /*raster=*/true);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], md5_hash_raster);
  }
}

}  // namespace chrome_pdf
