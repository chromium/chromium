// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_UTILS_H_
#define PRINTING_PRINTING_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)

#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/win_handle_types.h"
#include "ui/gfx/geometry/rect.h"
#endif

namespace gfx {
class Size;
}

namespace printing {

// Simplify title to resolve issue with some drivers.
COMPONENT_EXPORT(PRINTING_BASE)
std::u16string SimplifyDocumentTitle(const std::u16string& title);

COMPONENT_EXPORT(PRINTING_BASE)
std::u16string SimplifyDocumentTitleWithLength(const std::u16string& title,
                                               size_t length);

COMPONENT_EXPORT(PRINTING_BASE)
std::u16string FormatDocumentTitleWithOwner(const std::u16string& owner,
                                            const std::u16string& title);

COMPONENT_EXPORT(PRINTING_BASE)
std::u16string FormatDocumentTitleWithOwnerAndLength(
    const std::u16string& owner,
    const std::u16string& title,
    size_t length);

#if BUILDFLAG(USE_CUPS)
// Returns the paper size (microns) most common in the locale to the nearest
// millimeter. Defaults to ISO A4 for an empty or invalid locale.
COMPONENT_EXPORT(PRINTING_BASE)
gfx::Size GetDefaultPaperSizeFromLocaleMicrons(std::string_view locale);

// Returns true if both dimensions of the sizes have a delta less than or equal
// to the epsilon value.
COMPONENT_EXPORT(PRINTING_BASE)
bool SizesEqualWithinEpsilon(const gfx::Size& lhs,
                             const gfx::Size& rhs,
                             int epsilon);
#endif

#if BUILDFLAG(IS_WIN)
// Get page content rect adjusted based on
// http://dev.w3.org/csswg/css3-page/#positioning-page-box
COMPONENT_EXPORT(PRINTING_BASE)
gfx::Rect GetCenteredPageContentRect(const gfx::Size& paper_size,
                                     const gfx::Size& page_size,
                                     const gfx::Rect& page_content_rect);

// Returns the printable area in device units for `hdc`.
COMPONENT_EXPORT(PRINTING_BASE)
gfx::Rect GetPrintableAreaDeviceUnits(HDC hdc);

// Identifies the type of data generated in a print document.
enum class DocumentDataType { kUnknown, kPdf, kXps };

// Helper for tests and CHECKs to determine the type of data that was generated
// for the document to be printed.  This includes checking a minimal size and
// magic bytes for known signatures.
COMPONENT_EXPORT(PRINTING_BASE)
DocumentDataType DetermineDocumentDataType(base::span<const uint8_t> data);

// Helper for tests and CHECKs to validate that `maybe_xps_data` suggests an
// XPS document. This includes checking a minimal size and magic bytes.
COMPONENT_EXPORT(PRINTING_BASE)
bool LooksLikeXps(base::span<const uint8_t> maybe_xps_data);
#endif  // BUILDFLAG(IS_WIN)

// Helper for tests and DCHECKs to validate that `maybe_pdf_data` suggests a PDF
// document. This includes checking a minimal size and magic bytes.
COMPONENT_EXPORT(PRINTING_BASE)
bool LooksLikePdf(base::span<const uint8_t> maybe_pdf_data);

}  // namespace printing

#endif  // PRINTING_PRINTING_UTILS_H_
