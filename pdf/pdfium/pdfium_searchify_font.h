// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_SEARCHIFY_FONT_H_
#define PDF_PDFIUM_PDFIUM_SEARCHIFY_FONT_H_

#include <cstdint>
#include <vector>

namespace chrome_pdf {

extern const uint8_t kPdfTtf[];
extern const uint32_t kPdfTtfSize;

extern const char kToUnicodeCMap[];

std::vector<uint8_t> CreateCidToGidMap();

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_SEARCHIFY_FONT_H_
