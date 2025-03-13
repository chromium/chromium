// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_HELPERS_H_
#define PDF_TEST_TEST_HELPERS_H_

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "v8/include/v8-forward.h"

class SkImage;
class SkSurface;

namespace gfx {
class Size;
}  // namespace gfx

namespace chrome_pdf {

// Resolves a file path within //pdf/test/data. `path` must be relative. Returns
// the empty path if the source root can't be found.
base::FilePath GetTestDataFilePath(const base::FilePath& path);

// Gets a file path that includes a platform-identifying suffix in its file
// name. If `filename` has an extension, then the suffix is inserted right
// before that. Since expectation files are typically generated on Linux, no
// suffix is generated for that platform.  A suffix is only generated for
// Windows or macOS platforms.
base::FilePath::StringType GetTestDataPathWithPlatformSuffix(
    std::string_view filename);

// Matches `actual_image` against the PNG at the file path `expected_png_file`.
// The path must be relative to //pdf/test/data.
testing::AssertionResult MatchesPngFile(
    const SkImage* actual_image,
    const base::FilePath& expected_png_file);

// Same as MatchesPngFile() above, but with a fuzzy pixel comparator.
testing::AssertionResult FuzzyMatchesPngFile(
    const SkImage* actual_image,
    const base::FilePath& expected_png_file);

// Takes `pdf_data` and loads it using PDFium. Then renders the page at
// `page_index` to a bitmap of `size_in_points` and checks if it matches
// `expected_png_file`.
void CheckPdfRendering(base::span<const uint8_t> pdf_data,
                       int page_index,
                       const gfx::Size& size_in_points,
                       const base::FilePath& expected_png_file);

// Creates a Skia surface with dimensions `size` and filled with `color`.
sk_sp<SkSurface> CreateSkiaSurfaceForTesting(const gfx::Size& size,
                                             SkColor color);

// Creates a Skia image with dimensions `size` and filled with `color`.
sk_sp<SkImage> CreateSkiaImageForTesting(const gfx::Size& size, SkColor color);

// Retrieves the `v8::Isolate` the test harness created when initializing blink.
v8::Isolate* GetBlinkIsolate();

// Stores the `v8::Isolate` the test harness created when initializing blink.
void SetBlinkIsolate(v8::Isolate* isolate);

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_HELPERS_H_
