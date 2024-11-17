// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_INK_WRITER_H_
#define PDF_PDFIUM_PDFIUM_INK_WRITER_H_

#include <vector>

#include "pdf/buildflags.h"
#include "third_party/pdfium/public/fpdfview.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace ink {
class Stroke;
}

namespace chrome_pdf {

// Writes `stroke` into `page` in `document` using the "V2" Ink format.
//
// Returns handles to the page objects if the operation is successful.
// The returned vector never contains nullptr entries.
//
// - If either `document` or `page` is null, then return an empty vector.
// - If the operation fails, then both `document` and `page` are left unchanged.
// - If `document` is not associated with `page`, then the behavior is
//   undefined.
// - If the provided `stroke` is empty, then return an empty vector.
std::vector<FPDF_PAGEOBJECT> WriteStrokeToPage(FPDF_DOCUMENT document,
                                               FPDF_PAGE page,
                                               const ink::Stroke& stroke);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_INK_WRITER_H_
