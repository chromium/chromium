// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_api_wrappers.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
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

std::u16string GetPageObjectMarkName(FPDF_PAGEOBJECTMARK mark) {
  // FPDFPageObjMark_GetName() naturally handles null `mark` inputs, so no
  // explicit check.

  std::u16string name;
  // NOLINT used below because this is required by the PDFium API interaction.
  unsigned long buflen_bytes = 0;  // NOLINT(runtime/int)
  if (!FPDFPageObjMark_GetName(mark, nullptr, 0, &buflen_bytes)) {
    return name;
  }

  // PDFium should never return an odd number of bytes for 16-bit chars.
  static_assert(sizeof(FPDF_WCHAR) == sizeof(char16_t));
  CHECK_EQ(buflen_bytes % 2, 0u);

  // Number of characters, including the NUL.
  const size_t expected_size = base::checked_cast<size_t>(buflen_bytes / 2);
  PDFiumAPIStringBufferAdapter adapter(&name, expected_size,
                                       /*check_expected_size=*/true);
  unsigned long actual_buflen_bytes = 0;  // NOLINT(runtime/int)
  bool result =
      FPDFPageObjMark_GetName(mark, static_cast<FPDF_WCHAR*>(adapter.GetData()),
                              buflen_bytes, &actual_buflen_bytes);
  CHECK(result);

  // Reuse `expected_size`, as `actual_buflen_bytes` divided by 2 is equal.
  CHECK_EQ(actual_buflen_bytes, buflen_bytes);
  adapter.Close(expected_size);
  return name;
}

}  // namespace chrome_pdf
