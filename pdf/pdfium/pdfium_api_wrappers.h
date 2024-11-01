// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_
#define PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"

// This file contains wrapper functions that let callers use modern C++
// constructs to interact with PDFium. This is easier than accessing PDFium C
// APIs directly.

namespace chrome_pdf {

// Same as LoadPdfDataWithPassword(), but without a password.
ScopedFPDFDocument LoadPdfData(base::span<const uint8_t> pdf_data);

// Wrapper around FPDF_LoadMemDocument64(). `password` can be empty, but it
// cannot be a string_view, since it needs to be null-terminated.
ScopedFPDFDocument LoadPdfDataWithPassword(base::span<const uint8_t> pdf_data,
                                           const std::string& password);

// Wrapper around FPDFPageObjMark_GetName().
// Returns the name of `mark`, or an empty string on failure.
std::u16string GetPageObjectMarkName(FPDF_PAGEOBJECTMARK mark);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_
