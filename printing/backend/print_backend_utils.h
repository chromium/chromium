// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(USE_CUPS)
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"
#endif  // BUILDFLAG(USE_CUPS)

namespace gfx {
class Size;
}

namespace printing {

enum class Unit {
  kInches,
  kMillimeters,
};

// Parses the media name expressed by `value` into the size of the media
// in microns. Returns an empty size if `value` does not contain the display
// name nor the dimension, or if `value` contains a prefix of
// media sizes not meant for users' eyes.
COMPONENT_EXPORT(PRINT_BACKEND)
gfx::Size ParsePaperSize(base::StringPiece value);

#if BUILDFLAG(USE_CUPS)
// Parses the media name expressed by `value` into a Paper. Returns an
// empty Paper if `value` does not contain the display name nor the dimension,
// `value` contains a prefix of media sizes not meant for users' eyes, or if the
// paper size is empty.
// `margins` is used to calculate the Paper's printable area.
// We don't handle l10n here. We do populate the display_name member with the
// prettified vendor ID, but fully expect the caller to clobber this if a better
// localization exists.
COMPONENT_EXPORT(PRINT_BACKEND)
PrinterSemanticCapsAndDefaults::Paper ParsePaper(
    base::StringPiece value,
    const CupsPrinter::CupsMediaMargins& margins);
#endif  // BUILDFLAG(USE_CUPS)

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
