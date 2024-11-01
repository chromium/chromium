// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_reader.h"

#include <vector>

#include "third_party/ink/src/ink/geometry/modeled_shape.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

namespace {

bool IsV2InkPath(FPDF_PAGEOBJECT page_object) {
  // TODO(crbug.com/353942910): Process `page_object`.
  return false;
}

}  // namespace

std::vector<ink::ModeledShape> ReadV2InkPathsFromPageAsModeledShapes(
    FPDF_PAGE page) {
  std::vector<ink::ModeledShape> shapes;
  if (!page) {
    return shapes;
  }

  const int page_object_count = FPDFPage_CountObjects(page);
  for (int i = 0; i < page_object_count; ++i) {
    FPDF_PAGEOBJECT page_object = FPDFPage_GetObject(page, i);
    if (!IsV2InkPath(page_object)) {
      continue;
    }

    // TODO(crbug.com/353942910): Import `page_object` into `shapes`.
  }
  return shapes;
}

}  // namespace chrome_pdf
