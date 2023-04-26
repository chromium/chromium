// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_utils.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "printing/buildflags/buildflags.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

#if BUILDFLAG(USE_CUPS)
#include "printing/backend/cups_printer.h"
#endif  // BUILDFLAG(USE_CUPS)

namespace printing {

namespace {

constexpr float kMmPerInch = 25.4f;
constexpr float kMicronsPerInch = kMmPerInch * kMicronsPerMm;

// Defines two prefixes of a special breed of media sizes not meant for
// users' eyes. CUPS incidentally returns these IPP values to us, but
// we have no use for them.
constexpr base::StringPiece kMediaCustomMinPrefix = "custom_min";
constexpr base::StringPiece kMediaCustomMaxPrefix = "custom_max";

bool IsValidMediaName(base::StringPiece& value,
                      std::vector<base::StringPiece>& pieces) {
  // We expect at least a display string and a dimension string.
  // Additionally, we drop the "custom_min*" and "custom_max*" special
  // "sizes" (not for users' eyes).
  return pieces.size() >= 2 &&
         !base::StartsWith(value, kMediaCustomMinPrefix) &&
         !base::StartsWith(value, kMediaCustomMaxPrefix);
}

std::vector<base::StringPiece> GetStringPiecesIfValid(base::StringPiece value) {
  // <name>_<width>x<height>{in,mm}
  // e.g. na_letter_8.5x11in, iso_a4_210x297mm
  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      value, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (!IsValidMediaName(value, pieces)) {
    return std::vector<base::StringPiece>();
  }
  return pieces;
}

gfx::Size DimensionsToMicrons(base::StringPiece value) {
  Unit unit;
  base::StringPiece dims;
  size_t unit_position;
  if ((unit_position = value.find("mm")) != base::StringPiece::npos) {
    unit = Unit::kMillimeters;
    dims = value.substr(0, unit_position);
  } else if ((unit_position = value.find("in")) != base::StringPiece::npos) {
    unit = Unit::kInches;
    dims = value.substr(0, unit_position);
  } else {
    LOG(WARNING) << "Could not parse paper dimensions";
    return {0, 0};
  }

  double width;
  double height;
  std::vector<std::string> pieces = base::SplitString(
      dims, "x", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2 || !base::StringToDouble(pieces[0], &width) ||
      !base::StringToDouble(pieces[1], &height)) {
    return {0, 0};
  }

  float scale;
  switch (unit) {
    case Unit::kMillimeters:
      scale = kMicronsPerMm;
      break;
    case Unit::kInches:
      scale = kMicronsPerInch;
      break;
  }

  return gfx::ToFlooredSize(gfx::ScaleSize(gfx::SizeF(width, height), scale));
}

}  // namespace

gfx::Size ParsePaperSize(base::StringPiece value) {
  std::vector<base::StringPiece> pieces = GetStringPiecesIfValid(value);
  if (pieces.empty()) {
    return gfx::Size();
  }

  base::StringPiece dimensions = pieces.back();
  return DimensionsToMicrons(dimensions);
}

#if BUILDFLAG(USE_CUPS)
PrinterSemanticCapsAndDefaults::Paper ParsePaper(
    base::StringPiece value,
    const CupsPrinter::CupsMediaMargins& margins) {
  std::vector<base::StringPiece> pieces = GetStringPiecesIfValid(value);
  if (pieces.empty()) {
    return PrinterSemanticCapsAndDefaults::Paper();
  }

  base::StringPiece dimensions = pieces.back();

  PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = std::string(value);
  paper.size_um = DimensionsToMicrons(dimensions);
  if (paper.size_um.IsEmpty()) {
    return PrinterSemanticCapsAndDefaults::Paper();
  }

  // The margins of the printable area are expressed in PWG units (100ths of
  // mm).
  int printable_area_left_um = margins.left * kMicronsPerPwgUnit;
  int printable_area_bottom_um = margins.bottom * kMicronsPerPwgUnit;
  int printable_area_width_um =
      paper.size_um.width() -
      ((margins.left + margins.right) * kMicronsPerPwgUnit);
  int printable_area_length_um =
      paper.size_um.height() -
      ((margins.top + margins.bottom) * kMicronsPerPwgUnit);
  paper.printable_area_um =
      gfx::Rect(printable_area_left_um, printable_area_bottom_um,
                printable_area_width_um, printable_area_length_um);

  // Default to the paper size if printable area is empty.
  // We've seen some drivers have a printable area that goes out of bounds
  // of the paper size. In those cases, set the printable area to be the
  // size. (See crbug.com/1412305.)
  const gfx::Rect size_um_rect = gfx::Rect(paper.size_um);
  if (paper.printable_area_um.IsEmpty() ||
      !size_um_rect.Contains(paper.printable_area_um)) {
    paper.printable_area_um = size_um_rect;
  }

  // Omits the final token describing the media dimensions.
  pieces.pop_back();
  paper.display_name = base::JoinString(pieces, " ");

  return paper;
}
#endif  // BUILDFLAG(USE_CUPS)

}  // namespace printing
