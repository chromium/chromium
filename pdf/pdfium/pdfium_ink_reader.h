// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_INK_READER_H_
#define PDF_PDFIUM_PDFIUM_INK_READER_H_

#include <vector>

#include "pdf/buildflags.h"
#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/pdfium/public/fpdfview.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// For the given `page`, iterate through all page objects and import "V2" paths
// created by Ink as ink::ModeledShapes. The shapes do not have outlines and are
// only suitable for use with ink::Intersects().
//
// If a path does not match the characteristics of a "V2" path, or if the path
// cannot be properly tessellated, then it is ignored.
//
// If `page` is null, then the return value is an empty vector.
std::vector<ink::ModeledShape> ReadV2InkPathsFromPageAsModeledShapes(
    FPDF_PAGE page);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_INK_READER_H_
