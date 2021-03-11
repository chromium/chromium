// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_UTILS_H_
#define PRINTING_PRINTING_UTILS_H_

#include <stddef.h>

#include <string>

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "printing/backend/print_backend.h"
#include "printing/printing_export.h"

namespace gfx {
class Size;
}

namespace printing {

// Simplify title to resolve issue with some drivers.
PRINTING_EXPORT base::string16 SimplifyDocumentTitle(
    const base::string16& title);

PRINTING_EXPORT base::string16 SimplifyDocumentTitleWithLength(
    const base::string16& title,
    size_t length);

PRINTING_EXPORT base::string16 FormatDocumentTitleWithOwner(
    const base::string16& owner,
    const base::string16& title);

PRINTING_EXPORT base::string16 FormatDocumentTitleWithOwnerAndLength(
    const base::string16& owner,
    const base::string16& title,
    size_t length);

// Returns the paper size (microns) most common in the locale to the nearest
// millimeter. Defaults to ISO A4 for an empty or invalid locale.
PRINTING_EXPORT gfx::Size GetDefaultPaperSizeFromLocaleMicrons(
    base::StringPiece locale);

// Returns true if both dimensions of the sizes have a delta less than or equal
// to the epsilon value.
PRINTING_EXPORT bool SizesEqualWithinEpsilon(const gfx::Size& lhs,
                                             const gfx::Size& rhs,
                                             int epsilon);

PRINTING_EXPORT PrinterSemanticCapsAndDefaults::Paper ParsePaper(
    base::StringPiece value);

}  // namespace printing

#endif  // PRINTING_PRINTING_UTILS_H_
