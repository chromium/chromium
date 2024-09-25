// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_ink_writer.h"

#include "third_party/ink/src/ink/strokes/stroke.h"

namespace chrome_pdf {

bool WriteStrokeToPage(FPDF_PAGE page, const ink::Stroke& stroke) {
  if (!page) {
    return false;
  }

  // TODO(crbug.com/335517469): Implement.
  return true;
}

}  // namespace chrome_pdf
