// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_
#define PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_

#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "pdf/pdfium/pdfium_engine_exports.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdfview.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

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

// Wrapper around FPDF_RenderPageBitmap().
// Renders `page` using `settings` into `bitmap_buffer`. Returns whether
// rendering succeeded or not.
bool RenderPageToBitmap(FPDF_PAGE page,
                        const PDFiumEngineExports::RenderingSettings& settings,
                        void* bitmap_buffer);

#if BUILDFLAG(IS_WIN)
// Wrapper around FPDF_RenderPageBitmap().
// Similar to RenderPageToBitmap(), but renders into `dc` instead.
bool RenderPageToDC(FPDF_PAGE page,
                    const PDFiumEngineExports::RenderingSettings& settings,
                    HDC dc);
#endif

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_
