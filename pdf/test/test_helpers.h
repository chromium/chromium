// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_HELPERS_H_
#define PDF_TEST_TEST_HELPERS_H_

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;
class SkSurface;

namespace gfx {
class Size;
}  // namespace gfx

namespace chrome_pdf {

// Resolves a file path within //pdf/test/data. `path` must be relative. Returns
// the empty path if the source root can't be found.
base::FilePath GetTestDataFilePath(const base::FilePath& path);

// Matches `actual_image` against the PNG at the file path `expected_png_file`.
// The path must be relative to //pdf/test/data.
testing::AssertionResult MatchesPngFile(
    const SkImage* actual_image,
    const base::FilePath& expected_png_file);

// Creates a Skia surface with dimensions `size` and filled with `color`.
sk_sp<SkSurface> CreateSkiaSurfaceForTesting(const gfx::Size& size,
                                             SkColor color);

// Creates a Skia image with dimensions `size` and filled with `color`.
sk_sp<SkImage> CreateSkiaImageForTesting(const gfx::Size& size, SkColor color);

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_HELPERS_H_
