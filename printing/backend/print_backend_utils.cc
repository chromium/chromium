// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_utils.h"

#include <string_view>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
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
constexpr std::string_view kMediaCustomMinPrefix = "custom_min";
constexpr std::string_view kMediaCustomMaxPrefix = "custom_max";

bool IsValidMediaName(std::string_view& value,
                      std::vector<std::string_view>& pieces) {
  // We expect at least a display string and a dimension string.
  // Additionally, we drop the "custom_min*" and "custom_max*" special
  // "sizes" (not for users' eyes).
  return pieces.size() >= 2 &&
         !base::StartsWith(value, kMediaCustomMinPrefix) &&
         !base::StartsWith(value, kMediaCustomMaxPrefix);
}

std::vector<std::string_view> GetStringPiecesIfValid(std::string_view value) {
  // <name>_<width>x<height>{in,mm}
  // e.g. na_letter_8.5x11in, iso_a4_210x297mm
  std::vector<std::string_view> pieces = base::SplitStringPiece(
      value, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (!IsValidMediaName(value, pieces)) {
    return std::vector<std::string_view>();
  }
  return pieces;
}

gfx::Size DimensionsToMicrons(std::string_view value) {
  Unit unit;
  std::string_view dims;
  size_t unit_position;
  if ((unit_position = value.find("mm")) != std::string_view::npos) {
    unit = Unit::kMillimeters;
    dims = value.substr(0, unit_position);
  } else if ((unit_position = value.find("in")) != std::string_view::npos) {
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

gfx::Size ParsePaperSize(std::string_view value) {
  std::vector<std::string_view> pieces = GetStringPiecesIfValid(value);
  if (pieces.empty()) {
    return gfx::Size();
  }

  std::string_view dimensions = pieces.back();
  return DimensionsToMicrons(dimensions);
}

#if BUILDFLAG(USE_CUPS)
gfx::Rect PrintableAreaFromSizeAndPwgMargins(const gfx::Size& size_um,
                                             int bottom_pwg,
                                             int left_pwg,
                                             int right_pwg,
                                             int top_pwg) {
  // The margins of the printable area are expressed in PWG units (100ths of
  // mm) in the IPP 'media-col-database' attribute.
  int printable_area_left_um = left_pwg * kMicronsPerPwgUnit;
  int printable_area_bottom_um = bottom_pwg * kMicronsPerPwgUnit;
  int printable_area_width_um =
      size_um.width() - ((left_pwg + right_pwg) * kMicronsPerPwgUnit);
  int printable_area_height_um =
      size_um.height() - ((top_pwg + bottom_pwg) * kMicronsPerPwgUnit);
  return gfx::Rect(printable_area_left_um, printable_area_bottom_um,
                   printable_area_width_um, printable_area_height_um);
}

void PwgMarginsFromSizeAndPrintableArea(const gfx::Size& size_um,
                                        const gfx::Rect& printable_area_um,
                                        int* bottom_pwg,
                                        int* left_pwg,
                                        int* right_pwg,
                                        int* top_pwg) {
  DCHECK(bottom_pwg);
  DCHECK(left_pwg);
  DCHECK(right_pwg);
  DCHECK(top_pwg);

  // These values in microns were obtained in the first place by converting
  // from PWG units, so we can losslessly convert them back.
  int bottom_um = printable_area_um.y();
  int left_um = printable_area_um.x();
  int right_um = size_um.width() - printable_area_um.right();
  int top_um = size_um.height() - printable_area_um.bottom();
  DCHECK_EQ(bottom_um % kMicronsPerPwgUnit, 0);
  DCHECK_EQ(left_um % kMicronsPerPwgUnit, 0);
  DCHECK_EQ(right_um % kMicronsPerPwgUnit, 0);
  DCHECK_EQ(top_um % kMicronsPerPwgUnit, 0);

  *bottom_pwg = bottom_um / kMicronsPerPwgUnit;
  *left_pwg = left_um / kMicronsPerPwgUnit;
  *right_pwg = right_um / kMicronsPerPwgUnit;
  *top_pwg = top_um / kMicronsPerPwgUnit;
}
#endif  // BUILDFLAG(USE_CUPS)

COMPONENT_EXPORT(PRINT_BACKEND)
std::string GetDisplayName(const std::string& printer_name,
                           std::string_view info) {
#if BUILDFLAG(IS_MAC)
  // It is possible to create a printer with a blank display name, so just
  // use the printer name in such a case.
  if (!info.empty()) {
    return std::string(info);
  }
#endif
  return printer_name;
}

COMPONENT_EXPORT(PRINT_BACKEND)
std::string_view GetPrinterDescription(std::string_view drv_info,
                                       std::string_view info) {
#if BUILDFLAG(IS_MAC)
  // On Mac, `drv_info` specifies the printer description
  if (!drv_info.empty()) {
    return drv_info;
  }
#else
  if (!info.empty()) {
    return info;
  }
#endif
  return std::string_view();
}

}  // namespace printing
