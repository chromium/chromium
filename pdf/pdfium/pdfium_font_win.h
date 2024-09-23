// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_FONT_WIN_H_
#define PDF_PDFIUM_PDFIUM_FONT_WIN_H_

#include "third_party/pdfium/public/fpdf_sysfontinfo.h"

namespace chrome_pdf {

// Initializes a Windows-specific font mapper that sends font requests for
// system fonts to Blink. This is necessary because font loading does not work
// in the sandbox on Windows.
void InitializeWindowsFontMapper();

// Allow unit testing of the font matcher.
FPDF_SYSFONTINFO* GetSkiaFontMapperForTesting();

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_FONT_WIN_H_
