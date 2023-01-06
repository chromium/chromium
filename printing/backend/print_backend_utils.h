// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "printing/backend/print_backend.h"

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

// Parses the media name expressed by `value` into a Paper. Returns an
// empty Paper if `value` does not contain the display name nor the dimension,
// or if `value` contains a prefix of media sizes not meant for users' eyes.
// We don't handle l10n here. We do populate the display_name member with the
// prettified vendor ID, but fully expect the caller to clobber this if a better
// localization exists.
COMPONENT_EXPORT(PRINT_BACKEND)
PrinterSemanticCapsAndDefaults::Paper ParsePaper(base::StringPiece value);

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
