// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_utils.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace printing {

namespace {

constexpr int kMicronsPerMm = 1000;
constexpr float kMmPerInch = 25.4f;
constexpr float kMicronsPerInch = kMmPerInch * kMicronsPerMm;

// Defines two prefixes of a special breed of media sizes not meant for
// users' eyes. CUPS incidentally returns these IPP values to us, but
// we have no use for them.
constexpr base::StringPiece kMediaCustomMinPrefix = "custom_min";
constexpr base::StringPiece kMediaCustomMaxPrefix = "custom_max";

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

// We read the media name expressed by `value` and return a Paper
// with the vendor_id and size_um members populated.
// We don't handle l10n here. We do populate the display_name member
// with the prettified vendor ID, but fully expect the caller to clobber
// this if a better localization exists.
PrinterSemanticCapsAndDefaults::Paper ParsePaper(base::StringPiece value) {
  // <name>_<width>x<height>{in,mm}
  // e.g. na_letter_8.5x11in, iso_a4_210x297mm

  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      value, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // We expect at least a display string and a dimension string.
  // Additionally, we drop the "custom_min*" and "custom_max*" special
  // "sizes" (not for users' eyes).
  if (pieces.size() < 2 || base::StartsWith(value, kMediaCustomMinPrefix) ||
      base::StartsWith(value, kMediaCustomMaxPrefix)) {
    return PrinterSemanticCapsAndDefaults::Paper();
  }

  base::StringPiece dimensions = pieces.back();

  PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = std::string(value);
  paper.size_um = DimensionsToMicrons(dimensions);
  // Omits the final token describing the media dimensions.
  pieces.pop_back();
  paper.display_name = base::JoinString(pieces, " ");

  return paper;
}

}  // namespace printing
