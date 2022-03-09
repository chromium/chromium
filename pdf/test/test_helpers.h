// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_TEST_HELPERS_H_
#define PDF_TEST_TEST_HELPERS_H_

#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkColor.h"

class SkBitmap;

namespace gfx {
class Size;
}  // namespace gfx

namespace chrome_pdf {

// Resolves a file path within //pdf/test/data. `path` must be relative. Returns
// the empty path if the source root can't be found.
base::FilePath GetTestDataFilePath(const base::FilePath& path);

// Creates a Skia-format `Image` of a given size filled with a given color.
SkBitmap CreateSkiaImageForTesting(const gfx::Size& size, SkColor color);

}  // namespace chrome_pdf

#endif  // PDF_TEST_TEST_HELPERS_H_
