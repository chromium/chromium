// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_api_wrappers.h"

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdfview.h"

namespace chrome_pdf {

ScopedFPDFDocument LoadPdfData(base::span<const uint8_t> pdf_data) {
  return LoadPdfDataWithPassword(pdf_data, std::string());
}

ScopedFPDFDocument LoadPdfDataWithPassword(base::span<const uint8_t> pdf_data,
                                           const std::string& password) {
  return ScopedFPDFDocument(FPDF_LoadMemDocument64(
      pdf_data.data(), pdf_data.size(), password.c_str()));
}

}  // namespace chrome_pdf
