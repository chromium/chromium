// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_util.h"

#include <cups/cups.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "printing/backend/cups_ipp_advanced_caps.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/units.h"

#if defined(OS_CHROMEOS)
#include "printing/printing_features_chromeos.h"
#endif  // defined(OS_CHROMEOS)

namespace printing {

#if defined(OS_CHROMEOS)
constexpr int kPinMinimumLength = 4;
#endif  // defined(OS_CHROMEOS)

namespace {

constexpr int kMicronsPerMM = 1000;
constexpr double kMMPerInch = 25.4;
constexpr double kMicronsPerInch = kMMPerInch * kMicronsPerMM;
constexpr double kCmPerInch = kMMPerInch * 0.1;

enum Unit {
  INCHES,
  MILLIMETERS,
};

struct ColorMap {
  const char* color;
  ColorModel model;
};

struct DuplexMap {
  const char* name;
  DuplexMode mode;
};

const ColorMap kColorList[]{
    {CUPS_PRINT_COLOR_MODE_COLOR, COLORMODE_COLOR},
    {CUPS_PRINT_COLOR_MODE_MONOCHROME, COLORMODE_MONOCHROME},
};

const DuplexMap kDuplexList[]{
    {CUPS_SIDES_ONE_SIDED, SIMPLEX},
    {CUPS_SIDES_TWO_SIDED_PORTRAIT, LONG_EDGE},
    {CUPS_SIDES_TWO_SIDED_LANDSCAPE, SHORT_EDGE},
};

ColorModel ColorModelFromIppColor(base::StringPiece ippColor) {
  for (const ColorMap& color : kColorList) {
    if (ippColor.compare(color.color) == 0) {
      return color.model;
    }
  }

  return UNKNOWN_COLOR_MODEL;
}

DuplexMode DuplexModeFromIpp(base::StringPiece ipp_duplex) {
  for (const DuplexMap& entry : kDuplexList) {
    if (base::EqualsCaseInsensitiveASCII(ipp_duplex, entry.name))
      return entry.mode;
  }
  return UNKNOWN_DUPLEX_MODE;
}

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

// We read the media name expressed by |value| and return a Paper
// with the vendor_id and size_um members populated.
// We don't handle l10n here. We do populate the display_name member
// with the prettified vendor ID, but fully expect the caller to clobber
// this if a better localization exists.
PrinterSemanticCapsAndDefaults::Paper ParsePaper(base::StringPiece value) {
  // <name>_<width>x<height>{in,mm}
  // e.g. na_letter_8.5x11in, iso_a4_210x297mm

  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      value, "_", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // we expect at least a display string and a dimension string
  if (pieces.size() < 2)
    return PrinterSemanticCapsAndDefaults::Paper();

  base::StringPiece dimensions = pieces.back();

  PrinterSemanticCapsAndDefaults::Paper paper;
  paper.vendor_id = value.as_string();
  paper.size_um = DimensionsToMicrons(dimensions);
  // Omits the final token describing the media dimensions.
  pieces.pop_back();
  paper.display_name = base::JoinString(pieces, " ");

  return paper;
}

ColorModel DefaultColorModel(const CupsOptionProvider& printer) {
  // default color
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppColor);
  if (!attr)
    return UNKNOWN_COLOR_MODEL;

  return ColorModelFromIppColor(ippGetString(attr, 0, nullptr));
}

std::vector<ColorModel> SupportedColorModels(
    const CupsOptionProvider& printer) {
  std::vector<ColorModel> colors;

  std::vector<base::StringPiece> color_modes =
      printer.GetSupportedOptionValueStrings(kIppColor);
  for (base::StringPiece color : color_modes) {
    ColorModel color_model = ColorModelFromIppColor(color);
    if (color_model != UNKNOWN_COLOR_MODEL) {
      colors.push_back(color_model);
    }
  }

  return colors;
}

void ExtractColor(const CupsOptionProvider& printer,
                  PrinterSemanticCapsAndDefaults* printer_info) {
  printer_info->bw_model = UNKNOWN_COLOR_MODEL;
  printer_info->color_model = UNKNOWN_COLOR_MODEL;

  // color and b&w
  std::vector<ColorModel> color_models = SupportedColorModels(printer);
  for (ColorModel color : color_models) {
    switch (color) {
      case COLORMODE_COLOR:
        printer_info->color_model = COLORMODE_COLOR;
        break;
      case COLORMODE_MONOCHROME:
        printer_info->bw_model = COLORMODE_MONOCHROME;
        break;
      default:
        // value not needed
        break;
    }
  }

  // changeable
  printer_info->color_changeable =
      (printer_info->color_model != UNKNOWN_COLOR_MODEL &&
       printer_info->bw_model != UNKNOWN_COLOR_MODEL);

  // default color
  printer_info->color_default = DefaultColorModel(printer) == COLORMODE_COLOR;
}

void ExtractDuplexModes(const CupsOptionProvider& printer,
                        PrinterSemanticCapsAndDefaults* printer_info) {
  std::vector<base::StringPiece> duplex_modes =
      printer.GetSupportedOptionValueStrings(kIppDuplex);
  for (base::StringPiece duplex : duplex_modes) {
    DuplexMode duplex_mode = DuplexModeFromIpp(duplex);
    if (duplex_mode != UNKNOWN_DUPLEX_MODE)
      printer_info->duplex_modes.push_back(duplex_mode);
  }
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppDuplex);
  printer_info->duplex_default =
      attr ? DuplexModeFromIpp(ippGetString(attr, 0, nullptr))
           : UNKNOWN_DUPLEX_MODE;
}

void CopiesRange(const CupsOptionProvider& printer,
                 int* lower_bound,
                 int* upper_bound) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppCopies);
  if (!attr) {
    *lower_bound = -1;
    *upper_bound = -1;
  }

  *lower_bound = ippGetRange(attr, 0, upper_bound);
}

void ExtractCopies(const CupsOptionProvider& printer,
                   PrinterSemanticCapsAndDefaults* printer_info) {
  // copies
  int lower_bound;
  int upper_bound;
  CopiesRange(printer, &lower_bound, &upper_bound);
  printer_info->copies_capable = (lower_bound != -1) && (upper_bound >= 2);
}

// Reads resolution from |attr| and puts into |size| in dots per inch.
base::Optional<gfx::Size> GetResolution(ipp_attribute_t* attr, int i) {
  ipp_res_t units;
  int yres;
  int xres = ippGetResolution(attr, i, &yres, &units);
  if (!xres)
    return {};

  switch (units) {
    case IPP_RES_PER_INCH:
      return gfx::Size(xres, yres);
    case IPP_RES_PER_CM:
      return gfx::Size(xres * kCmPerInch, yres * kCmPerInch);
  }
  return {};
}

// Initializes |printer_info.dpis| with available resolutions and
// |printer_info.default_dpi| with default resolution provided by |printer|.
void ExtractResolutions(const CupsOptionProvider& printer,
                        PrinterSemanticCapsAndDefaults* printer_info) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppResolution);
  if (!attr)
    return;

  int num_options = ippGetCount(attr);
  for (int i = 0; i < num_options; i++) {
    base::Optional<gfx::Size> size = GetResolution(attr, i);
    if (size)
      printer_info->dpis.push_back(size.value());
  }
  ipp_attribute_t* def_attr = printer.GetDefaultOptionValue(kIppResolution);
  base::Optional<gfx::Size> size = GetResolution(def_attr, 0);
  if (size)
    printer_info->default_dpi = size.value();
}

PrinterSemanticCapsAndDefaults::Papers SupportedPapers(
    const CupsOptionProvider& printer) {
  std::vector<base::StringPiece> papers =
      printer.GetSupportedOptionValueStrings(kIppMedia);
  PrinterSemanticCapsAndDefaults::Papers parsed_papers;
  parsed_papers.reserve(papers.size());
  for (base::StringPiece paper : papers)
    parsed_papers.push_back(ParsePaper(paper));

  return parsed_papers;
}

bool CollateCapable(const CupsOptionProvider& printer) {
  std::vector<base::StringPiece> values =
      printer.GetSupportedOptionValueStrings(kIppCollate);
  return base::Contains(values, kCollated);
}

bool CollateDefault(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppCollate);
  if (!attr)
    return false;

  base::StringPiece name = ippGetString(attr, 0, nullptr);
  return name.compare(kCollated) == 0;
}

#if defined(OS_CHROMEOS)
bool PinSupported(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppPin);
  if (!attr)
    return false;
  int password_maximum_length_supported = ippGetInteger(attr, 0);
  if (password_maximum_length_supported < kPinMinimumLength)
    return false;

  std::vector<base::StringPiece> values =
      printer.GetSupportedOptionValueStrings(kIppPinEncryption);
  return base::Contains(values, kPinEncryptionNone);
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

PrinterSemanticCapsAndDefaults::Paper DefaultPaper(
    const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppMedia);
  if (!attr)
    return PrinterSemanticCapsAndDefaults::Paper();

  return ParsePaper(ippGetString(attr, 0, nullptr));
}

void CapsAndDefaultsFromPrinter(const CupsOptionProvider& printer,
                                PrinterSemanticCapsAndDefaults* printer_info) {
  // collate
  printer_info->collate_default = CollateDefault(printer);
  printer_info->collate_capable = CollateCapable(printer);

  // paper
  printer_info->default_paper = DefaultPaper(printer);
  printer_info->papers = SupportedPapers(printer);

#if defined(OS_CHROMEOS)
  printer_info->pin_supported = PinSupported(printer);
  if (base::FeatureList::IsEnabled(printing::kAdvancedPpdAttributes))
    ExtractAdvancedCapabilities(printer, printer_info);
#endif  // defined(OS_CHROMEOS)

  ExtractCopies(printer, printer_info);
  ExtractColor(printer, printer_info);
  ExtractDuplexModes(printer, printer_info);
  ExtractResolutions(printer, printer_info);
}

ScopedIppPtr WrapIpp(ipp_t* ipp) {
  return ScopedIppPtr(ipp, &ippDelete);
}

}  //  namespace printing
