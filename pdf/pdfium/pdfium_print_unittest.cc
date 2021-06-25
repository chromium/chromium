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
#include "printing/pdf_render_settings.h"
#include "printing/units.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/web/web_print_params.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"

namespace chrome_pdf {

using PDFiumPrintTest = PDFiumTestBase;
using ::testing::ElementsAre;

namespace {

// Number of color channels in a BGRA bitmap.
constexpr int kColorChannels = 4;

constexpr gfx::Size kUSLetterSize = {612, 792};
constexpr gfx::Rect kUSLetterRect = {{0, 0}, kUSLetterSize};
constexpr gfx::Rect kPrintableAreaRect = {{18, 18}, {576, 733}};

using ExpectedDimensions = std::vector<gfx::SizeF>;

blink::WebPrintParams GetDefaultPrintParams() {
  blink::WebPrintParams params;
  params.print_content_area = kUSLetterRect;
  params.printable_area = kUSLetterRect;
  params.paper_size = kUSLetterSize;
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
    absl::optional<gfx::SizeF> page_size =
        exports.GetPDFPageSizeByIndex(pdf_data, i);
    ASSERT_TRUE(page_size.has_value());
    EXPECT_EQ(expected_dimensions[i], page_size.value());
  }
}

void CheckPdfRendering(const std::vector<uint8_t>& pdf_data,
                       int page_number,
                       const gfx::SizeF& size_in_points,
                       const char* expected_md5_hash) {
  int width_in_pixels =
      printing::ConvertUnit(size_in_points.width(), printing::kPointsPerInch,
                            printing::kDefaultPdfDpi);
  int height_in_pixels =
      printing::ConvertUnit(size_in_points.height(), printing::kPointsPerInch,
                            printing::kDefaultPdfDpi);

  const gfx::Rect page_rect(width_in_pixels, height_in_pixels);
  std::vector<uint8_t> page_bitmap_data(kColorChannels * page_rect.width() *
                                        page_rect.height());

  PDFEngineExports::RenderingSettings settings(
      gfx::Size(printing::kDefaultPdfDpi, printing::kDefaultPdfDpi), page_rect,
      /*fit_to_bounds=*/true,
      /*stretch_to_bounds=*/false,
      /*keep_aspect_ratio=*/true,
      /*center_in_bounds=*/true,
      /*autorotate=*/false, /*use_color=*/true, /*render_for_printing=*/true);

  PDFiumEngineExports exports;
  ASSERT_TRUE(exports.RenderPDFPageToBitmap(pdf_data, page_number, settings,
                                            page_bitmap_data.data()));

  base::MD5Digest hash;
  base::MD5Sum(page_bitmap_data.data(), page_bitmap_data.size(), &hash);
  EXPECT_STREQ(expected_md5_hash, base::MD5DigestToBase16(hash).c_str());
}

}  // namespace

TEST_F(PDFiumPrintTest, Basic) {
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

TEST_F(PDFiumPrintTest, AlterScaling) {
  TestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("rectangles.pdf"));
  ASSERT_TRUE(engine);

  PDFiumPrint print(engine.get());

  blink::WebPrintParams print_params = GetDefaultPrintParams();
  print_params.printable_area = kPrintableAreaRect;

  blink::WebPrintParams print_params_raster = print_params;
  print_params_raster.rasterize_pdf = true;

  const ExpectedDimensions kExpectedDimensions = {{612.0, 792.0}};
  const std::vector<int> pages = {0};

  {
    // Default scaling
    static constexpr char kChecksum[] = "40e2e16416015cdde5c6e5735c1d06ac";
    static constexpr char kChecksumRaster[] =
        "535659885de1ba060222cb13df995ca7";

    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], kChecksum);

    pdf_data = print.PrintPagesAsPdf(pages, print_params_raster);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], kChecksumRaster);
  }
  {
    // "Fit to Printable Area" scaling
    print_params.print_scaling_option =
        printing::mojom::PrintScalingOption::kFitToPrintableArea;
    print_params_raster.print_scaling_option =
        printing::mojom::PrintScalingOption::kFitToPrintableArea;

    static constexpr char kChecksum[] = "41847e1f0c581150a84794482528f790";
    static constexpr char kChecksumRaster[] =
        "63e36d3b991bbd3126fbb6f6c95af336";

    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], kChecksum);

    pdf_data = print.PrintPagesAsPdf(pages, print_params_raster);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], kChecksumRaster);
  }
  {
    // "Fit to Paper" scaling
    print_params.print_scaling_option =
        printing::mojom::PrintScalingOption::kFitToPaper;
    print_params_raster.print_scaling_option =
        printing::mojom::PrintScalingOption::kFitToPaper;

    static constexpr char kChecksum[] = "3a4828228bcbae230574c057b7a0669e";
    static constexpr char kChecksumRaster[] =
        "3ca8f6ead6fe5e41b5e2d8817bedecbb";

    std::vector<uint8_t> pdf_data = print.PrintPagesAsPdf(pages, print_params);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], kChecksum);

    pdf_data = print.PrintPagesAsPdf(pages, print_params_raster);
    CheckPdfDimensions(pdf_data, kExpectedDimensions);
    CheckPdfRendering(pdf_data, 0, kExpectedDimensions[0], kChecksumRaster);
  }
}

}  // namespace chrome_pdf
