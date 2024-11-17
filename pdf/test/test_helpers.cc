// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_helpers.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace chrome_pdf {

namespace {

testing::AssertionResult MatchesPngFileImpl(
    const SkImage* actual_image,
    const base::FilePath& expected_png_file,
    const cc::PixelComparator& comparitor) {
  SkBitmap actual_bitmap;
  if (!actual_image->asLegacyBitmap(&actual_bitmap)) {
    return testing::AssertionFailure() << "Reference: " << expected_png_file;
  }

  if (!cc::MatchesPNGFile(actual_bitmap, GetTestDataFilePath(expected_png_file),
                          comparitor)) {
    return testing::AssertionFailure() << "Reference: " << expected_png_file;
  }

  return testing::AssertionSuccess();
}

}  // namespace

base::FilePath GetTestDataFilePath(const base::FilePath& path) {
  return base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
      .Append(FILE_PATH_LITERAL("pdf"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(path);
}

testing::AssertionResult MatchesPngFile(
    const SkImage* actual_image,
    const base::FilePath& expected_png_file) {
  return MatchesPngFileImpl(actual_image, expected_png_file,
                            cc::ExactPixelComparator());
}

testing::AssertionResult FuzzyMatchesPngFile(
    const SkImage* actual_image,
    const base::FilePath& expected_png_file) {
  // Effectively a "FuzzyPixelOffByTwoComparator".
  cc::FuzzyPixelComparator comparator;
  comparator.SetErrorPixelsPercentageLimit(100.0f);
  comparator.SetAbsErrorLimit(2);
  return MatchesPngFileImpl(actual_image, expected_png_file, comparator);
}

void CheckPdfRendering(base::span<const uint8_t> pdf_data,
                       int page_index,
                       const gfx::Size& size_in_points,
                       const base::FilePath& expected_png_file) {
  const gfx::Rect page_rect(size_in_points);
  SkBitmap page_bitmap;
  page_bitmap.allocPixels(
      SkImageInfo::Make(gfx::SizeToSkISize(page_rect.size()),
                        kBGRA_8888_SkColorType, kPremul_SkAlphaType));

  PDFiumEngineExports::RenderingSettings settings(
      gfx::Size(printing::kPointsPerInch, printing::kPointsPerInch), page_rect,
      /*fit_to_bounds=*/false,
      /*stretch_to_bounds=*/false,
      /*keep_aspect_ratio=*/true,
      /*center_in_bounds=*/false,
      /*autorotate=*/false, /*use_color=*/true, /*render_for_printing=*/false);

  PDFiumEngineExports exports;
  ASSERT_TRUE(exports.RenderPDFPageToBitmap(pdf_data, page_index, settings,
                                            page_bitmap.getPixels()));

  EXPECT_TRUE(MatchesPngFile(page_bitmap.asImage().get(), expected_png_file));
}

sk_sp<SkSurface> CreateSkiaSurfaceForTesting(const gfx::Size& size,
                                             SkColor color) {
  auto surface = SkSurfaces::Raster(
      SkImageInfo::MakeN32Premul(size.width(), size.height()));
  surface->getCanvas()->clear(color);
  return surface;
}

sk_sp<SkImage> CreateSkiaImageForTesting(const gfx::Size& size, SkColor color) {
  return CreateSkiaSurfaceForTesting(size, color)->makeImageSnapshot();
}

static v8::Isolate* g_isolate = nullptr;

v8::Isolate* GetBlinkIsolate() {
  return g_isolate;
}

void SetBlinkIsolate(v8::Isolate* isolate) {
  g_isolate = isolate;
}

}  // namespace chrome_pdf
