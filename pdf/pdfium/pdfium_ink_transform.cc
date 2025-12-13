// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_transform.h"

#include "pdf/pdf_ink_transform.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_rotation.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

gfx::Transform GetCanonicalToPdfTransformForPage(FPDF_PAGE page) {
  CHECK(page);

  // Get the intersection between the page's MediaBox and CropBox, to find
  // the translation offset for the shapes' transform.
  const PdfRect bounding_box = GetPageBoundingBox(page).value();
  const gfx::Vector2dF offset(bounding_box.left(), bounding_box.bottom());

  return GetCanonicalToPdfTransform(
      {FPDF_GetPageWidthF(page), FPDF_GetPageHeightF(page)},
      GetPageRotation(page).value_or(PageRotation::kRotate0), offset);
}

}  // namespace chrome_pdf
