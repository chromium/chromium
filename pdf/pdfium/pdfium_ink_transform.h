// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_INK_TRANSFORM_H_
#define PDF_PDFIUM_PDFIUM_INK_TRANSFORM_H_

#include "pdf/buildflags.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/transform.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// Wrapper function that returns the GetCanonicalToPdfTransform() result for a
// given `page`. Takes the page's MediaBox and CropBox into account.
gfx::Transform GetCanonicalToPdfTransformForPage(FPDF_PAGE page);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_INK_TRANSFORM_H_
