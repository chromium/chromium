// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_utils.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

constexpr int kMicronsPerMM = 1000;
constexpr double kMMPerInch = 25.4;
constexpr double kMicronsPerInch = kMMPerInch * kMicronsPerMM;

// Defines two prefixes of a special breed of media sizes not meant for
// users' eyes. CUPS incidentally returns these IPP values to us, but
// we have no use for them.
constexpr base::StringPiece kMediaCustomMinPrefix = "custom_min";
constexpr base::StringPiece kMediaCustomMaxPrefix = "custom_max";

enum Unit {
  INCHES,
  MILLIMETERS,
};

gfx::Size DimensionsToMicrons(base::StringPiece value) {
  Unit unit;
  base::StringPiece dims;
  size_t unit_position;
  if ((unit_position = value.find("mm")) != base::StringPiece::npos) {
    unit = MILLIMETERS;
    dims = value.substr(0, unit_position);
  } else if ((unit_position = value.find("in")) != base::StringPiece::npos) {
    unit = INCHES;
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

  int width_microns;
  int height_microns;
  switch (unit) {
    case MILLIMETERS:
      width_microns = width * kMicronsPerMM;
      height_microns = height * kMicronsPerMM;
      break;
    case INCHES:
      width_microns = width * kMicronsPerInch;
      height_microns = height * kMicronsPerInch;
      break;
    default:
      NOTREACHED();
      break;
  }

  return gfx::Size{width_microns, height_microns};
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
