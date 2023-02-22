// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_helper.h"

#include <cups/cups.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "printing/backend/ipp_handler_map.h"
#include "printing/backend/ipp_handlers.h"
#include "printing/printing_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace printing {

#if BUILDFLAG(IS_CHROMEOS)
constexpr int kPinMinimumLength = 4;
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

constexpr double kMMPerInch = 25.4;
constexpr double kCmPerInch = kMMPerInch * 0.1;

struct ColorMap {
  const char* color;
  mojom::ColorModel model;
};

struct DuplexMap {
  const char* name;
  mojom::DuplexMode mode;
};

const ColorMap kColorList[]{
    {CUPS_PRINT_COLOR_MODE_COLOR, mojom::ColorModel::kColorModeColor},
    {CUPS_PRINT_COLOR_MODE_MONOCHROME, mojom::ColorModel::kColorModeMonochrome},
};

const DuplexMap kDuplexList[]{
    {CUPS_SIDES_ONE_SIDED, mojom::DuplexMode::kSimplex},
    {CUPS_SIDES_TWO_SIDED_PORTRAIT, mojom::DuplexMode::kLongEdge},
    {CUPS_SIDES_TWO_SIDED_LANDSCAPE, mojom::DuplexMode::kShortEdge},
};

mojom::ColorModel ColorModelFromIppColor(base::StringPiece ippColor) {
  for (const ColorMap& color : kColorList) {
    if (ippColor.compare(color.color) == 0) {
      return color.model;
    }
  }

  return mojom::ColorModel::kUnknownColorModel;
}

mojom::DuplexMode DuplexModeFromIpp(base::StringPiece ipp_duplex) {
  for (const DuplexMap& entry : kDuplexList) {
    if (base::EqualsCaseInsensitiveASCII(ipp_duplex, entry.name))
      return entry.mode;
  }
  return mojom::DuplexMode::kUnknownDuplexMode;
}

mojom::ColorModel DefaultColorModel(const CupsOptionProvider& printer) {
  // default color
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppColor);
  if (!attr)
    return mojom::ColorModel::kUnknownColorModel;

  const char* const value = ippGetString(attr, 0, nullptr);
  return value ? ColorModelFromIppColor(value)
               : mojom::ColorModel::kUnknownColorModel;
}

std::vector<mojom::ColorModel> SupportedColorModels(
    const CupsOptionProvider& printer) {
  std::vector<mojom::ColorModel> colors;

  std::vector<base::StringPiece> color_modes =
      printer.GetSupportedOptionValueStrings(kIppColor);
  for (base::StringPiece color : color_modes) {
    mojom::ColorModel color_model = ColorModelFromIppColor(color);
    if (color_model != mojom::ColorModel::kUnknownColorModel) {
      colors.push_back(color_model);
    }
  }

  return colors;
}

void ExtractColor(const CupsOptionProvider& printer,
                  PrinterSemanticCapsAndDefaults* printer_info) {
  printer_info->bw_model = mojom::ColorModel::kUnknownColorModel;
  printer_info->color_model = mojom::ColorModel::kUnknownColorModel;

  // color and b&w
  std::vector<mojom::ColorModel> color_models = SupportedColorModels(printer);
  for (mojom::ColorModel color : color_models) {
    switch (color) {
      case mojom::ColorModel::kColorModeColor:
        printer_info->color_model = mojom::ColorModel::kColorModeColor;
        break;
      case mojom::ColorModel::kColorModeMonochrome:
        printer_info->bw_model = mojom::ColorModel::kColorModeMonochrome;
        break;
      default:
        // value not needed
        break;
    }
  }

  // changeable
  printer_info->color_changeable =
      (printer_info->color_model != mojom::ColorModel::kUnknownColorModel &&
       printer_info->bw_model != mojom::ColorModel::kUnknownColorModel);

  // default color
  printer_info->color_default =
      DefaultColorModel(printer) == mojom::ColorModel::kColorModeColor;
}

void ExtractDuplexModes(const CupsOptionProvider& printer,
                        PrinterSemanticCapsAndDefaults* printer_info) {
  std::vector<base::StringPiece> duplex_modes =
      printer.GetSupportedOptionValueStrings(kIppDuplex);
  for (base::StringPiece duplex : duplex_modes) {
    mojom::DuplexMode duplex_mode = DuplexModeFromIpp(duplex);
    if (duplex_mode != mojom::DuplexMode::kUnknownDuplexMode)
      printer_info->duplex_modes.push_back(duplex_mode);
  }

  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppDuplex);
  if (!attr) {
    printer_info->duplex_default = mojom::DuplexMode::kUnknownDuplexMode;
    return;
  }

  const char* const attr_str = ippGetString(attr, 0, nullptr);
  printer_info->duplex_default = attr_str
                                     ? DuplexModeFromIpp(attr_str)
                                     : mojom::DuplexMode::kUnknownDuplexMode;
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
  printer_info->copies_max =
      (lower_bound != -1 && upper_bound >= 2) ? upper_bound : 1;
}

// Reads resolution from `attr` and puts into `size` in dots per inch.
absl::optional<gfx::Size> GetResolution(ipp_attribute_t* attr, int i) {
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

// Initializes `printer_info.dpis` with available resolutions and
// `printer_info.default_dpi` with default resolution provided by `printer`.
void ExtractResolutions(const CupsOptionProvider& printer,
                        PrinterSemanticCapsAndDefaults* printer_info) {
  // Provide a default DPI if no valid DPI is found.
#if BUILDFLAG(IS_MAC)
  constexpr gfx::Size kDefaultMissingDpi(kDefaultMacDpi, kDefaultMacDpi);
#elif BUILDFLAG(IS_LINUX)
  constexpr gfx::Size kDefaultMissingDpi(kPixelsPerInch, kPixelsPerInch);
#else
  constexpr gfx::Size kDefaultMissingDpi(kDefaultPdfDpi, kDefaultPdfDpi);
#endif

  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppResolution);
  if (!attr) {
    printer_info->dpis.push_back(kDefaultMissingDpi);
    return;
  }

  int num_options = ippGetCount(attr);
  for (int i = 0; i < num_options; i++) {
    absl::optional<gfx::Size> size = GetResolution(attr, i);
    if (size)
      printer_info->dpis.push_back(size.value());
  }
  ipp_attribute_t* def_attr = printer.GetDefaultOptionValue(kIppResolution);
  absl::optional<gfx::Size> size = GetResolution(def_attr, 0);
  if (size) {
    printer_info->default_dpi = size.value();
  } else if (!printer_info->dpis.empty()) {
    printer_info->default_dpi = printer_info->dpis[0];
  } else {
    printer_info->default_dpi = kDefaultMissingDpi;
  }

  if (printer_info->dpis.empty()) {
    printer_info->dpis.push_back(printer_info->default_dpi);
  }
}

PrinterSemanticCapsAndDefaults::Papers SupportedPapers(
    const CupsPrinter& printer) {
  std::vector<base::StringPiece> papers =
      printer.GetSupportedOptionValueStrings(kIppMedia);
  PrinterSemanticCapsAndDefaults::Papers parsed_papers;
  parsed_papers.reserve(papers.size());
  for (base::StringPiece paper : papers) {
    PrinterSemanticCapsAndDefaults::Paper parsed =
        ParsePaper(paper, printer.GetMediaMarginsByName(std::string(paper)));
    // If a paper fails to parse reasonably, we should avoid propagating
    // it - e.g. CUPS is known to give out empty vendor IDs at times:
    // https://crbug.com/920295#c23
    if (!parsed.display_name.empty()) {
      parsed_papers.push_back(parsed);
    }
  }

  return parsed_papers;
}

bool CollateCapable(const CupsOptionProvider& printer) {
  std::vector<base::StringPiece> values =
      printer.GetSupportedOptionValueStrings(kIppCollate);
  return base::Contains(values, kCollated) &&
         base::Contains(values, kUncollated);
}

bool CollateDefault(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppCollate);
  if (!attr)
    return false;

  const char* const name = ippGetString(attr, 0, nullptr);
  return name && !base::StringPiece(name).compare(kCollated);
}

#if BUILDFLAG(IS_CHROMEOS)
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

// Returns the number of IPP attributes added to `caps` (not necessarily in
// 1-to-1 correspondence).
size_t AddAttributes(const CupsOptionProvider& printer,
                     const char* attr_group_name,
                     AdvancedCapabilities* caps) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attr_group_name);
  if (!attr)
    return 0;

  int num_options = ippGetCount(attr);
  static const base::NoDestructor<HandlerMap> handlers(GenerateHandlers());
  std::vector<std::string> unknown_options;
  size_t attr_count = 0;
  for (int i = 0; i < num_options; i++) {
    const char* option_name = ippGetString(attr, i, nullptr);
    auto it = handlers->find(option_name);
    if (it == handlers->end()) {
      unknown_options.emplace_back(option_name);
      continue;
    }

    size_t previous_size = caps->size();
    // Run the handler that adds items to `caps` based on option type.
    it->second.Run(printer, option_name, caps);
    if (caps->size() > previous_size)
      attr_count++;
  }
  if (!unknown_options.empty()) {
    LOG(WARNING) << "Unknown IPP options: "
                 << base::JoinString(unknown_options, ", ");
  }
  return attr_count;
}

// Adds the "Input Tray" option to Advanced Attributes.
size_t AddInputTray(const CupsOptionProvider& printer,
                    AdvancedCapabilities* caps) {
  size_t previous_size = caps->size();
  // b/151324273: CUPS doesn't implement media-source in media-col-database like
  // it should according to the IPP specs. However, it does implement a naked
  // media-source attribute which we can use until the proper changes can be
  // made to media-col-database.
  KeywordHandler(printer, "media-source", caps);
  return caps->size() - previous_size;
}

void ExtractAdvancedCapabilities(const CupsOptionProvider& printer,
                                 PrinterSemanticCapsAndDefaults* printer_info) {
  AdvancedCapabilities* options = &printer_info->advanced_capabilities;
  size_t attr_count = AddInputTray(printer, options);
  attr_count += AddAttributes(printer, kIppJobAttributes, options);
  attr_count += AddAttributes(printer, kIppDocumentAttributes, options);
  base::UmaHistogramCounts1000("Printing.CUPS.IppAttributesCount", attr_count);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

PrinterSemanticCapsAndDefaults::Paper DefaultPaper(const CupsPrinter& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppMedia);
  if (!attr)
    return PrinterSemanticCapsAndDefaults::Paper();
  const char* const media_name = ippGetString(attr, 0, nullptr);
  if (!media_name) {
    return PrinterSemanticCapsAndDefaults::Paper();
  }

  return ParsePaper(media_name, printer.GetMediaMarginsByName(media_name));
}

void CapsAndDefaultsFromPrinter(const CupsPrinter& printer,
                                PrinterSemanticCapsAndDefaults* printer_info) {
  // collate
  printer_info->collate_default = CollateDefault(printer);
  printer_info->collate_capable = CollateCapable(printer);

  // paper
  printer_info->default_paper = DefaultPaper(printer);
  printer_info->papers = SupportedPapers(printer);

#if BUILDFLAG(IS_CHROMEOS)
  printer_info->pin_supported = PinSupported(printer);
  ExtractAdvancedCapabilities(printer, printer_info);
#endif  // BUILDFLAG(IS_CHROMEOS)

  ExtractCopies(printer, printer_info);
  ExtractColor(printer, printer_info);
  ExtractDuplexModes(printer, printer_info);
  ExtractResolutions(printer, printer_info);
}

ScopedIppPtr WrapIpp(ipp_t* ipp) {
  return ScopedIppPtr(ipp, &ippDelete);
}

}  //  namespace printing
