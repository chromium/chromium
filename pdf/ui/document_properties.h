// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_UI_DOCUMENT_PROPERTIES_H_
#define PDF_UI_DOCUMENT_PROPERTIES_H_

#include <optional>
#include <string>

#include "pdf/document_metadata.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace chrome_pdf {

// Formats `size_points` of a page to a localized string suitable for display to
// the user. The returned string contains the dimensions and orientation of the
// page. The dimension units are set by the user's locale. Example return
// values:
// ->  210 x 297 mm (portrait)
// ->  11.00 x 8.50 in (landscape)
//
// Returns the string "Varies" if `size_points` is `std::nullopt`.
std::u16string FormatPageSize(const std::optional<gfx::Size>& size_points);

// Formats `version` to a string suitable for display to a user. Version numbers
// do not require localization.
std::string FormatPdfVersion(PdfVersion version);

}  // namespace chrome_pdf

#endif  // PDF_UI_DOCUMENT_PROPERTIES_H_
