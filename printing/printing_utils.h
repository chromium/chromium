// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_UTILS_H_
#define PRINTING_PRINTING_UTILS_H_

#include <stddef.h>

#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/string_piece.h"
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

#if BUILDFLAG(USE_CUPS) && !BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the paper size (microns) most common in the locale to the nearest
// millimeter. Defaults to ISO A4 for an empty or invalid locale.
COMPONENT_EXPORT(PRINTING_BASE)
gfx::Size GetDefaultPaperSizeFromLocaleMicrons(base::StringPiece locale);

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
#endif

// Helper for tests and DCHECKs to validate that `maybe_pdf_data` suggests a PDF
// document. This includes checking a minimal size and magic bytes.
COMPONENT_EXPORT(PRINTING_BASE)
bool LooksLikePdf(base::span<const char> maybe_pdf_data);

}  // namespace printing

#endif  // PRINTING_PRINTING_UTILS_H_
