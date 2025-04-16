// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_ROTATION_H_
#define PDF_PDFIUM_PDFIUM_ROTATION_H_

#include <optional>

#include "pdf/page_rotation.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

std::optional<PageRotation> GetPageRotation(FPDF_PAGE page);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_ROTATION_H_
