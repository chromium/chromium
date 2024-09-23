// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_print.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_helpers.h"
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

using PDFiumPrintTest = PDFiumTestBase;
using ::testing::ElementsAre;

namespace {

// blink::WebPrintParams takes values in CSS pixels, not points.
constexpr gfx::SizeF kUSLetterSize = {816, 1056};
constexpr gfx::RectF kUSLetterRect = {{0, 0}, kUSLetterSize};
constexpr gfx::RectF kPrintableAreaRect = {{24, 24}, {768, 977.33333}};

using ExpectedDimensions = std::vector<gfx::SizeF>;

std::string GenerateRendererSpecificFileName(const std::string& file_name,
                                             bool use_skia_renderer) {
  return base::StringPrintf("%s%s.png", file_name.c_str(),
                            use_skia_renderer ? "_skia" : "");
}

base::FilePath GetReferenceFilePath(std::string_view test_filename) {
  return base::FilePath(FILE_PATH_LITERAL("pdfium_print"))
      .AppendASCII(test_filename);
}

blink::WebPrintParams GetDefaultPrintParams() {
  blink::WebPrintParams params;
  params.default_page_description.size = kUSLetterSize;
  params.printable_area_in_css_pixels = kUSLetterRect;
  params.print_scaling_option = printing::mojom::PrintScalingOption::kNone;
  return params;
}

void CheckPdfDimensions(const std::vector<uint8_t>& pdf_data,
                        const ExpectedDimensions& expected_dimensions) {
  PDFiumEngineExports exports;
  int page_count;
  ASSERT_TRUE(exports.GetPDFDocInfo(pdf_data, &page_count, nullptr));
  ASSERT_GT(page_count, 0);
  ASSERT_EQ(expected_dimensions.size(), static_cast<size_t>(page_count));

  for (int i = 0; i < page_count; ++i) {
    std::optional<gfx::SizeF> page_size =
        exports.GetPDFPageSizeByIndex(pdf_data, i);
    ASSERT_TRUE(page_size.has_value());
    EXPECT_EQ(expected_dimensions[i], page_size.value());
  }
}

void CheckPdfRendering(const std::vector<uint8_t>& pdf_data,
                       int page_number,
                       const gfx::SizeF& size_in_points,
                       std::string_view expected_png_filename) {
  int width_in_pixels =
      printing::ConvertUnit(size_in_points.width(), printing::kPointsPerInch,
                            printing::kDefaultPdfDpi);
  int height_in_pixels =
      printing::ConvertUnit(size_in_points.height(), printing::kPointsPerInch,
                            printing::kDefaultPdfDpi);

  const gfx::Rect page_rect(width_in_pixels, height_in_pixels);
  SkBitmap page_bitmap;
  page_bitmap.allocPixels(
      SkImageInfo::Make(gfx::SizeToSkISize(page_rect.size()),
                        kBGRA_8888_SkColorType, kPremul_SkAlphaType));

  PDFiumEngineExports::RenderingSettings settings(
      gfx::Size(printing::kDefaultPdfDpi, printing::kDefaultPdfDpi), page_rect,
      /*fit_to_bounds=*/true,
      /*stretch_to_bounds=*/false,
      /*keep_aspect_ratio=*/true,
      /*center_in_bounds=*/true,
      /*autorotate=*/false, /*use_color=*/true, /*render_for_printing=*/true);

  PDFiumEngineExports exports;
  ASSERT_TRUE(exports.RenderPDFPageToBitmap(pdf_data, page_number, settings,
                                            page_bitmap.getPixels()));

  EXPECT_TRUE(MatchesPngFile(page_bitmap.asImage().get(),
                             GetReferenceFilePath(expected_png_filename)));
}

}  // namespace

TEST_P(PDFiumPrintTest, Basic) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPrint print(engine.get());

  blink::WebPrintParams print_params = GetDefaultPrintParams();
  blink::WebPrintParams print_params_raster = print_params;
  print_params_raster.rasterize_pdf = true;

  {
    // Print 2 pages.
    const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0},
                                                    {612.0, 792.0}};
    const std::vector<int> pages = {0, 1};
    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);

    pdf_data = print.PrintPagesAsPdf(pages, print_params_raster);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
  }
  {
    // Print 1 page.
    const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
    const std::vector<int> pages = {0};
    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);

    pdf_data = print.PrintPagesAsPdf(pages, print_params_raster);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
  }
  {
    // Print the other page.
    const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
    const std::vector<int> pages = {1};
    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);

    pdf_data = print.PrintPagesAsPdf(pages, print_params_raster);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
  }
}

TEST_P(PDFiumPrintTest, AlterScalingDefault) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPrint print(engine.get());

  const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
  const std::vector<int> pages = {0};

  blink::WebPrintParams print_params = GetDefaultPrintParams();
  print_params.printable_area_in_css_pixels = kPrintableAreaRect;
  std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
  CheckPdfDimensions(pdf_data, kExpectedDimensions);
  CheckPdfRendering(
      pdf_data, 0, kExpectedDimensions[0],
      GenerateRendererSpecificFileName("alter_scaling_default",
                                       /*use_skia_renderer=*/GetParam()));
  print_params.rasterize_pdf = true;
  pdf_data = print.PrintPagesAsPdf(pages, print_params);
  CheckPdfDimensions(pdf_data, kExpectedDimensions);
  CheckPdfRendering(
      pdf_data, 0, kExpectedDimensions[0],
      GenerateRendererSpecificFileName("alter_scaling_default_raster",
                                       /*use_skia_renderer=*/GetParam()));
}

TEST_P(PDFiumPrintTest, AlterScalingFitPaper) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPrint print(engine.get());

  const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
  const std::vector<int> pages = {0};

  blink::WebPrintParams print_params = GetDefaultPrintParams();
  print_params.printable_area_in_css_pixels = kPrintableAreaRect;
  print_params.print_scaling_option =
      printing::mojom::PrintScalingOption::kFitToPaper;
  std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
  CheckPdfDimensions(pdf_data, kExpectedDimensions);
  CheckPdfRendering(
      pdf_data, 0, kExpectedDimensions[0],
      GenerateRendererSpecificFileName("alter_scaling_fit-paper",
                                       /*use_skia_renderer=*/GetParam()));
  print_params.rasterize_pdf = true;
  pdf_data = print.PrintPagesAsPdf(pages, print_params);
  CheckPdfDimensions(pdf_data, kExpectedDimensions);
  CheckPdfRendering(
      pdf_data, 0, kExpectedDimensions[0],
      GenerateRendererSpecificFileName("alter_scaling_fit-paper_raster",
                                       /*use_skia_renderer=*/GetParam()));
}

TEST_P(PDFiumPrintTest, AlterScalingFitPrintable) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPrint print(engine.get());

  const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
  const std::vector<int> pages = {0};

  blink::WebPrintParams print_params = GetDefaultPrintParams();
  print_params.printable_area_in_css_pixels = kPrintableAreaRect;
  print_params.print_scaling_option =
      printing::mojom::PrintScalingOption::kFitToPrintableArea;
  std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
  CheckPdfDimensions(pdf_data, kExpectedDimensions);
  CheckPdfRendering(
      pdf_data, 0, kExpectedDimensions[0],
      GenerateRendererSpecificFileName("alter_scaling_fit-printable",
                                       /*use_skia_renderer=*/GetParam()));
  print_params.rasterize_pdf = true;
  pdf_data = print.PrintPagesAsPdf(pages, print_params);
  CheckPdfDimensions(pdf_data, kExpectedDimensions);
  CheckPdfRendering(
      pdf_data, 0, kExpectedDimensions[0],
      GenerateRendererSpecificFileName("alter_scaling_fit-printable_raster",
                                       /*use_skia_renderer=*/GetParam()));
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumPrintTest, testing::Bool());

}  // namespace chrome_pdf
