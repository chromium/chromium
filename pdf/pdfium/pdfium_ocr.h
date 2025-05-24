// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_OCR_H_
#define PDF_PDFIUM_PDFIUM_OCR_H_

#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size_f.h"

namespace chrome_pdf {

// TODO(crbug.com/c/360803943): Remove `rotate_image_to_upright` when PDF OCR
// support is removed and set the default behavior to
// rotate_image_to_upright = false.
SkBitmap GetImageForOcr(FPDF_DOCUMENT doc,
                        FPDF_PAGE page,
                        FPDF_PAGEOBJECT page_object,
                        uint32_t max_image_dimension,
                        bool rotate_image_to_upright);

// Returns image bound's size in page coordinates. Returns (0,0) if fails.
gfx::SizeF GetImageSize(FPDF_PAGEOBJECT page_object);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_OCR_H_
