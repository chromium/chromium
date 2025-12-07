// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_rotation.h"

#include "third_party/pdfium/public/fpdf_edit.h"

namespace chrome_pdf {

std::optional<PageRotation> GetPageRotation(FPDF_PAGE page) {
  switch (FPDFPage_GetRotation(page)) {
    case 0:
      return PageRotation::kRotate0;
    case 1:
      return PageRotation::kRotate90;
    case 2:
      return PageRotation::kRotate180;
    case 3:
      return PageRotation::kRotate270;
    default:
      return std::nullopt;
  }
}

}  // namespace chrome_pdf
