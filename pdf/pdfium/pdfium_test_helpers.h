// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_TEST_HELPERS_H_
#define PDF_PDFIUM_PDFIUM_TEST_HELPERS_H_

#include <string_view>

#include "third_party/pdfium/public/fpdfview.h"

namespace base {
class FilePath;
}

namespace gfx {
class Size;
}

namespace chrome_pdf {

// Scans PDF and returns the count of content mark objects it contains which
// match the specified name.  Will return 0 if `document` is null.
int GetPdfMarkObjCountForTesting(FPDF_DOCUMENT document,
                                 std::string_view mark_name);

// Renders `page` to a bitmap of `size_in_points` and checks if it matches
// `expected_png_file`.
void CheckPdfRendering(FPDF_PAGE page,
                       const gfx::Size& size_in_points,
                       const base::FilePath& expected_png_file);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_TEST_HELPERS_H_
