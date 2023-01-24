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

base::FilePath GetTestDataFilePath(const base::FilePath& path) {
  base::FilePath source_root;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root))
    return {};

  return source_root.Append(FILE_PATH_LITERAL("pdf"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(path);
}

testing::AssertionResult MatchesPngFile(
    const SkImage* actual_image,
    const base::FilePath& expected_png_file) {
  SkBitmap actual_bitmap;
  if (!actual_image->asLegacyBitmap(&actual_bitmap))
    return testing::AssertionFailure() << "Reference: " << expected_png_file;

  if (!cc::MatchesPNGFile(actual_bitmap, GetTestDataFilePath(expected_png_file),
                          cc::ExactPixelComparator())) {
    return testing::AssertionFailure() << "Reference: " << expected_png_file;
  }

  return testing::AssertionSuccess();
}

sk_sp<SkSurface> CreateSkiaSurfaceForTesting(const gfx::Size& size,
                                             SkColor color) {
  auto surface = SkSurface::MakeRasterN32Premul(size.width(), size.height());
  surface->getCanvas()->clear(color);
  return surface;
}

sk_sp<SkImage> CreateSkiaImageForTesting(const gfx::Size& size, SkColor color) {
  return CreateSkiaSurfaceForTesting(size, color)->makeImageSnapshot();
}

}  // namespace chrome_pdf
