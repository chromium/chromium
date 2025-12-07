// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_
#define PDF_PDFIUM_PDFIUM_API_WRAPPERS_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "pdf/pdf_rect.h"
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

// Converts between the two rect types. The returned rect is really the same
// instance as the input rect.
const FS_RECTF& FsRectFFromPdfRect(const PdfRect& rect);
FS_RECTF& FsRectFFromPdfRect(PdfRect& rect);

// Same as LoadPdfDataWithPassword(), but without a password.
ScopedFPDFDocument LoadPdfData(base::span<const uint8_t> pdf_data);

// Wrapper around FPDF_LoadMemDocument64(). `password` can be empty, but it
// cannot be a string_view, since it needs to be null-terminated.
ScopedFPDFDocument LoadPdfDataWithPassword(base::span<const uint8_t> pdf_data,
                                           const std::string& password);

// Wrapper around FPDFAnnot_GetRect(). Returns the bounds for `annot`, or
// std::nullopt on failure.
std::optional<PdfRect> GetAnnotRect(FPDF_ANNOTATION annot);

// Wrapper around FPDF_GetPageBoundingBox(). Returns the bounds for `page`, or
// std::nullopt on failure.
std::optional<PdfRect> GetPageBoundingBox(FPDF_PAGE page);

// Wrapper around FPDFPageObj_GetBounds(). Returns the bounds for `page_object`,
// or std::nullopt on failure.
std::optional<PdfRect> GetPageObjectBounds(FPDF_PAGEOBJECT page_object);

// Wrapper around FPDFPageObjMark_GetName().
// Returns the name of `mark`, or an empty string on failure.
std::u16string GetPageObjectMarkName(FPDF_PAGEOBJECTMARK mark);

// Wrapper around FPDFText_GetCharBox(). Returns the bounds for text at `index`
// in `text_page`, or std::nullopt on failure.
std::optional<PdfRect> GetTextCharBox(FPDF_TEXTPAGE text_page, int index);

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
