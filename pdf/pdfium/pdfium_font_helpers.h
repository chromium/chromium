// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_FONT_HELPERS_H_
#define PDF_PDFIUM_PDFIUM_FONT_HELPERS_H_

#include <optional>

#include "third_party/blink/public/platform/web_font_description.h"

namespace chrome_pdf {

// Helper shared between pdfium_font_linux and pdfium_font_win to transform
// pdfium font parameters into skia/blink friendly values. Returns nullopt
// if no suitable mapping can be suggested.
std::optional<blink::WebFontDescription> PdfFontToBlinkFontMapping(
    int weight,
    int italic,
    int charset,
    int pitch_family,
    const char* face);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_FONT_HELPERS_H_
