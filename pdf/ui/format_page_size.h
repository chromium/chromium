// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_UI_FORMAT_PAGE_SIZE_H_
#define PDF_UI_FORMAT_PAGE_SIZE_H_

#include "base/optional.h"
#include "base/strings/string16.h"

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
// Returns the string "Varies" if `size_points` is `base::nullopt`.
base::string16 FormatPageSize(const base::Optional<gfx::Size>& size_points);

}  // namespace chrome_pdf

#endif  // PDF_UI_FORMAT_PAGE_SIZE_H_
