// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_INK_WRITER_H_
#define PDF_PDFIUM_PDFIUM_INK_WRITER_H_

#include "pdf/buildflags.h"
#include "third_party/pdfium/public/fpdfview.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace ink {
class Stroke;
}

namespace chrome_pdf {

// Writes `stroke` into `page` in `document`.
//
// Returns whether the operation succeeded or not. If the operation fails, then
// both `document` and `page` are left unchanged.
//
// - If either `document` or `page` is null, then return false.
// - If `document` is not associated with `page`, then the behavior is
//   undefined.
// - If the provided `stroke` is empty, then return false.
bool WriteStrokeToPage(FPDF_DOCUMENT document,
                       FPDF_PAGE page,
                       const ink::Stroke& stroke);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_INK_WRITER_H_
