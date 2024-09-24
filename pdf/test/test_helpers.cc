// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/test_helpers.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

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
