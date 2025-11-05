// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_ACCESSIBILITY_CONSTANTS_HELPER_H_
#define PDF_PDF_ACCESSIBILITY_CONSTANTS_HELPER_H_

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "pdf/pdf_accessibility_constants.h"

namespace chrome_pdf {

// Given a string containing a PDF tag type, such as "H1", returns the
// corresponding enum value, such as `PdfTagType::kH1`.
PdfTagType PdfTagTypeFromString(const std::string& tag_type);

// Returns the PDF tag type string-to-enum map.
const base::fixed_flat_map<std::string_view, PdfTagType, 35>&
GetPdfTagTypeMap();

}  // namespace chrome_pdf

#endif  // PDF_PDF_ACCESSIBILITY_CONSTANTS_HELPER_H_
