// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_UTILS_H_
#define PRINTING_PRINTING_UTILS_H_

#include <stddef.h>

#include <string>

#include "base/component_export.h"
#include "base/strings/string_piece.h"

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

}  // namespace printing

#endif  // PRINTING_PRINTING_UTILS_H_
