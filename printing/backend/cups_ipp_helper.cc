// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_helper.h"

#include <cups/cups.h>

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/algorithm.h"
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

constexpr auto kColorMap =
    base::MakeFixedFlatMap<std::string_view, mojom::ColorModel>({
        {CUPS_PRINT_COLOR_MODE_COLOR, mojom::ColorModel::kColorModeColor},
        {CUPS_PRINT_COLOR_MODE_MONOCHROME,
         mojom::ColorModel::kColorModeMonochrome},
    });

constexpr auto kDuplexMap =
    base::MakeFixedFlatMap<std::string_view, mojom::DuplexMode>({
        {CUPS_SIDES_ONE_SIDED, mojom::DuplexMode::kSimplex},
        {CUPS_SIDES_TWO_SIDED_PORTRAIT, mojom::DuplexMode::kLongEdge},
        {CUPS_SIDES_TWO_SIDED_LANDSCAPE, mojom::DuplexMode::kShortEdge},
    });

mojom::ColorModel ColorModelFromIppColor(std::string_view ipp_color) {
  auto it = kColorMap.find(ipp_color);
  return it != kColorMap.end() ? it->second
                               : mojom::ColorModel::kUnknownColorModel;
}

mojom::DuplexMode DuplexModeFromIpp(std::string_view ipp_duplex) {
  auto it = kDuplexMap.find(ipp_duplex);
  return it != kDuplexMap.end() ? it->second
                                : mojom::DuplexMode::kUnknownDuplexMode;
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

  std::vector<std::string_view> color_modes =
      printer.GetSupportedOptionValueStrings(kIppColor);
  for (std::string_view color : color_modes) {
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
  std::vector<std::string_view> duplex_modes =
      printer.GetSupportedOptionValueStrings(kIppDuplex);
  for (std::string_view duplex : duplex_modes) {
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
std::optional<gfx::Size> GetResolution(ipp_attribute_t* attr, int i) {
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
    std::optional<gfx::Size> size = GetResolution(attr, i);
    if (size)
      printer_info->dpis.push_back(size.value());
  }
  ipp_attribute_t* def_attr = printer.GetDefaultOptionValue(kIppResolution);
  std::optional<gfx::Size> size = GetResolution(def_attr, 0);
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

std::optional<PrinterSemanticCapsAndDefaults::Paper>
PaperFromMediaColDatabaseEntry(ipp_t* db_entry) {
  DCHECK(db_entry);

  std::optional<MediaColData> size = ExtractMediaColData(db_entry);
  if (!size) {
    LOG(WARNING) << "Unable to create Paper from media-col-database";
    return std::nullopt;
  }

  if (size->HasVariableWidth()) {
    LOG(WARNING) << "Invalid media-col-database entry:"
                 << " variable widths are not supported.";
    return std::nullopt;
  }

  // Some PPDs (only ones with a custom height range) have a min height of 0,
  // which doesn't work with the printing stack.  If there is a min height of 0
  // (or equal to the margins) change it to some small value.  For entries that
  // have a fixed height, the height will not get modified; if this fixed height
  // is invalid, it will just get rejected below.
  if (size->HasVariableHeight()) {
    size->min_height = base::ClampMax(
        size->min_height,
        base::ClampedNumeric<int>(size->top_margin) + size->bottom_margin + 1);
  }

  if (size->min_width <= 0 || size->min_height <= 0 ||
      size->bottom_margin < 0 || size->top_margin < 0 ||
      size->left_margin < 0 || size->right_margin < 0 ||
      size->min_width <=
          base::ClampedNumeric<int>(size->left_margin) + size->right_margin ||
      size->min_height <=
          base::ClampedNumeric<int>(size->bottom_margin) + size->top_margin ||
      (size->HasVariableHeight() && size->max_height < size->min_height)) {
    LOG(WARNING) << "Invalid media-col-database entry:"
                 << " width=" << size->min_width << "-" << size->max_width
                 << " height=" << size->min_height << "-" << size->max_height
                 << " media-bottom-margin=" << size->bottom_margin
                 << " media-left-margin=" << size->left_margin
                 << " media-right-margin=" << size->right_margin
                 << " media-top-margin=" << size->top_margin;
    return std::nullopt;
  }

  gfx::Size size_um(size->min_width * kMicronsPerPwgUnit,
                    size->min_height * kMicronsPerPwgUnit);
  gfx::Rect printable_area_um = PrintableAreaFromSizeAndPwgMargins(
      size_um, size->bottom_margin, size->left_margin, size->right_margin,
      size->top_margin);
  int max_height_um =
      size->HasVariableHeight() ? size->max_height * kMicronsPerPwgUnit : 0;

  return PrinterSemanticCapsAndDefaults::Paper(
      /*display_name=*/"", /*vendor_id=*/"", size_um, printable_area_um,
      max_height_um);
}

bool PaperIsBorderless(const PrinterSemanticCapsAndDefaults::Paper& paper) {
  return paper.printable_area_um().x() == 0 &&
         paper.printable_area_um().y() == 0 &&
         paper.printable_area_um().width() == paper.size_um().width() &&
         paper.printable_area_um().height() == paper.size_um().height();
}

PrinterSemanticCapsAndDefaults::Papers SupportedPapers(
    const CupsPrinter& printer) {
  auto size_comparer = [](const gfx::Size& a, const gfx::Size& b) {
    auto result = a.width() - b.width();
    if (result == 0) {
      result = a.height() - b.height();
    }
    return result < 0;
  };
  std::map<gfx::Size, PrinterSemanticCapsAndDefaults::Paper,
           decltype(size_comparer)>
      paper_map;

  ipp_attribute_t* attr = printer.GetMediaColDatabase();
  int count = ippGetCount(attr);

  for (int i = 0; i < count; i++) {
    std::optional<PrinterSemanticCapsAndDefaults::Paper> paper_opt =
        PaperFromMediaColDatabaseEntry(ippGetCollection(attr, i));
    if (!paper_opt.has_value()) {
      continue;
    }

    const auto& paper = paper_opt.value();
    auto existing_entry = paper_map.find(paper.size_um());

    if (existing_entry == paper_map.end()) {
      paper_map.emplace(paper.size_um(), paper);
      continue;
    }

    // When a paper size has both bordered and borderless variants, set the
    // printable area according to the bordered variant, and set the flag
    // indicating that a borderless variant exists.
    if (PaperIsBorderless(existing_entry->second)) {
      if (!PaperIsBorderless(paper)) {
        existing_entry->second = paper;
        existing_entry->second.set_has_borderless_variant(true);
      }
    } else if (PaperIsBorderless(paper)) {
      existing_entry->second.set_has_borderless_variant(true);
    }
  }

  PrinterSemanticCapsAndDefaults::Papers parsed_papers;
  parsed_papers.reserve(paper_map.size());
  for (const auto& entry : paper_map) {
    parsed_papers.push_back(entry.second);
  }
  return parsed_papers;
}

// Overrides the given printer's default media type as needed.
void CorrectDefaultMediaType(PrinterSemanticCapsAndDefaults*& printer_info) {
  // Some Canon printers give a proprietary default media type that's frequently
  // unavailable to users.
  if (base::StartsWith(printer_info->default_media_type.vendor_id, "com.canon",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    for (const auto& media_type : printer_info->media_types) {
      if (media_type.vendor_id == "stationery") {
        printer_info->default_media_type = media_type;
        break;
      }
    }
  }
}

void ExtractMediaTypes(const CupsOptionProvider& printer,
                       PrinterSemanticCapsAndDefaults* printer_info) {
  std::vector<std::string_view> names =
      printer.GetSupportedOptionValueStrings(kIppMediaType);
  if (names.empty()) {
    return;
  }
  printer_info->media_types.reserve(names.size());

  for (std::string_view vendor_id : names) {
    PrinterSemanticCapsAndDefaults::MediaType type;
    type.vendor_id = std::string(vendor_id);

    // This name will be overwritten by its Chromium localization if it's a
    // standard IPP media type. But for non-standard types, the only
    // human-readable name available is this one from the driver (or
    // driverless printer).
    const char* display_name = printer.GetLocalizedOptionValueName(
        kIppMediaType, type.vendor_id.c_str());
    if (display_name) {
      type.display_name = display_name;
    } else {
      type.display_name = std::string(vendor_id);
    }

    printer_info->media_types.push_back(type);
  }

  // Set default media type, or use the first in the list if unavailable.
  DCHECK(!printer_info->media_types.empty());
  printer_info->default_media_type = printer_info->media_types[0];
  ipp_t* media_col_default =
      ippGetCollection(printer.GetDefaultOptionValue(kIppMediaCol), 0);
  const char* media_type_default = ippGetString(
      ippFindAttribute(media_col_default, kIppMediaType, IPP_TAG_KEYWORD), 0,
      nullptr);
  if (media_type_default) {
    // Don't set the "default" media type if it isn't in the list of supported
    // media types for some reason.
    for (const auto& media_type : printer_info->media_types) {
      if (media_type.vendor_id == media_type_default) {
        printer_info->default_media_type = media_type;
      }
    }
  }

  CorrectDefaultMediaType(printer_info);
}

bool CollateCapable(const CupsOptionProvider& printer) {
  std::vector<std::string_view> values =
      printer.GetSupportedOptionValueStrings(kIppCollate);
  return base::Contains(values, kCollated) &&
         base::Contains(values, kUncollated);
}

bool CollateDefault(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppCollate);
  if (!attr)
    return false;

  const char* const name = ippGetString(attr, 0, nullptr);
  return name && !std::string_view(name).compare(kCollated);
}

#if BUILDFLAG(IS_CHROMEOS)
bool PinSupported(const CupsOptionProvider& printer) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(kIppPin);
  if (!attr)
    return false;
  int password_maximum_length_supported = ippGetInteger(attr, 0);
  if (password_maximum_length_supported < kPinMinimumLength)
    return false;

  std::vector<std::string_view> values =
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
  // The names of attributes that we know are not supported (b/266573545).
  static constexpr auto kOptionsToIgnore =
      base::MakeFixedFlatSet<std::string_view>(
          {"finishings-col", "ipp-attribute-fidelity", "job-name",
           "number-up-layout"});
  std::vector<std::string> unknown_options;
  size_t attr_count = 0;
  for (int i = 0; i < num_options; i++) {
    const char* option_name = ippGetString(attr, i, nullptr);
    if (base::Contains(kOptionsToIgnore, option_name)) {
      continue;
    }
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
  KeywordHandler(printer, kIppMediaSource, caps);
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
  ipp_attribute_t* attr = printer.GetDefaultOptionValue(kIppMediaCol);
  if (!attr)
    return PrinterSemanticCapsAndDefaults::Paper();
  ipp_t* media_col_default = ippGetCollection(attr, 0);
  if (!media_col_default) {
    return PrinterSemanticCapsAndDefaults::Paper();
  }

  PrinterSemanticCapsAndDefaults::Paper paper;
  return PaperFromMediaColDatabaseEntry(media_col_default)
      .value_or(PrinterSemanticCapsAndDefaults::Paper());
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
  ExtractMediaTypes(printer, printer_info);
}

gfx::Rect GetPrintableAreaForSize(const CupsPrinter& printer,
                                  const gfx::Size& size_um) {
  ipp_attribute_t* attr = printer.GetMediaColDatabase();
  int count = ippGetCount(attr);
  gfx::Rect result(0, 0, size_um.width(), size_um.height());

  for (int i = 0; i < count; i++) {
    ipp_t* db_entry = ippGetCollection(attr, i);

    std::optional<PrinterSemanticCapsAndDefaults::Paper> paper_opt =
        PaperFromMediaColDatabaseEntry(db_entry);
    if (!paper_opt.has_value()) {
      continue;
    }

    const auto& paper = paper_opt.value();
    if (paper.size_um() != size_um) {
      continue;
    }

    result = paper.printable_area_um();

    // If this is a borderless size, try to find a non-borderless version.
    if (!PaperIsBorderless(paper)) {
      return result;
    }
  }

  return result;
}

ScopedIppPtr WrapIpp(ipp_t* ipp) {
  return ScopedIppPtr(ipp, IppDeleter());
}

void IppDeleter::operator()(ipp_t* ipp) const {
  ippDelete(ipp);
}

std::optional<MediaColData> ExtractMediaColData(ipp_t* db_entry) {
  if (!db_entry) {
    LOG(WARNING) << "Invalid media-col-database entry: empty entry.";
    return std::nullopt;
  }
  ipp_t* media_size = ippGetCollection(
      ippFindAttribute(db_entry, kIppMediaSize, IPP_TAG_BEGIN_COLLECTION), 0);
  if (!media_size) {
    LOG(WARNING) << "Invalid media-col-database entry: empty media_size.";
    return std::nullopt;
  }

  ipp_attribute_t* bottom_attr =
      ippFindAttribute(db_entry, kIppMediaBottomMargin, IPP_TAG_INTEGER);
  ipp_attribute_t* left_attr =
      ippFindAttribute(db_entry, kIppMediaLeftMargin, IPP_TAG_INTEGER);
  ipp_attribute_t* right_attr =
      ippFindAttribute(db_entry, kIppMediaRightMargin, IPP_TAG_INTEGER);
  ipp_attribute_t* top_attr =
      ippFindAttribute(db_entry, kIppMediaTopMargin, IPP_TAG_INTEGER);
  if (!bottom_attr || !left_attr || !right_attr || !top_attr) {
    LOG(WARNING) << "Invalid media-col-database entry:"
                 << " margins are not present.";
    return std::nullopt;
  }
  int bottom_margin = ippGetInteger(bottom_attr, 0);
  int left_margin = ippGetInteger(left_attr, 0);
  int right_margin = ippGetInteger(right_attr, 0);
  int top_margin = ippGetInteger(top_attr, 0);

  ipp_attribute_t* width_attr =
      ippFindAttribute(media_size, kIppXDimension, IPP_TAG_INTEGER);
  ipp_attribute_t* height_attr =
      ippFindAttribute(media_size, kIppYDimension, IPP_TAG_INTEGER);
  ipp_attribute_t* width_range_attr =
      ippFindAttribute(media_size, kIppXDimension, IPP_TAG_RANGE);
  ipp_attribute_t* height_range_attr =
      ippFindAttribute(media_size, kIppYDimension, IPP_TAG_RANGE);

  // Ensure there is a width and height (or ranges).
  if ((!width_attr && !width_range_attr) ||
      (!height_attr && !height_range_attr)) {
    LOG(WARNING) << "Invalid media-col-database entry:"
                 << " media-size needs x and y (or x and y range).";
    return std::nullopt;
  }

  int min_width = 0;
  int min_height = 0;
  int max_width = 0;
  int max_height = 0;
  if (width_attr) {
    min_width = ippGetInteger(width_attr, 0);
    max_width = min_width;
  } else {
    min_width = ippGetRange(width_range_attr, 0, &max_width);
  }
  if (height_attr) {
    min_height = ippGetInteger(height_attr, 0);
    max_height = min_height;
  } else {
    min_height = ippGetRange(height_range_attr, 0, &max_height);
  }

  if (min_width > max_width) {
    LOG(WARNING) << "Invalid media-col-database entry:"
                 << " min_width (" << min_width << ") > max_width ("
                 << max_width << ").";
    return std::nullopt;
  }
  if (min_height > max_height) {
    LOG(WARNING) << "Invalid media-col-database entry:"
                 << " min_height (" << min_height << ") > max_height ("
                 << max_height << ").";
    return std::nullopt;
  }

#if BUILDFLAG(IS_MAC)
  pwg_media_t* media = pwgMediaForSize(max_width, max_height);
  if (media && (media->width != max_width || media->length != max_height)) {
    // Paper size detected to be close to a standard size.  While the maximum
    // size will be based on this media, must also do checks to adjust the
    // minimum dimensions, as they must not end up exceeding the new maximum.
    if (min_width == max_width || min_width > media->width) {
      min_width = media->width;
    }
    if (min_height == max_height || min_height > media->length) {
      min_height = media->length;
    }
    max_width = media->width;
    max_height = media->length;
  }
#endif

  return MediaColData{min_width,     min_height,  max_width,    max_height,
                      bottom_margin, left_margin, right_margin, top_margin};
}

ScopedIppPtr NewMediaCollection(const MediaColData& data) {
  // Don't create any entries with a variable width.
  if (data.HasVariableWidth()) {
    return nullptr;
  }

  ScopedIppPtr media_col = WrapIpp(ippNew());
  ScopedIppPtr media_size = WrapIpp(ippNew());

  ippAddInteger(media_size.get(), IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                kIppXDimension, data.min_width);
  if (data.HasVariableHeight()) {
    ippAddRange(media_size.get(), IPP_TAG_PRINTER, kIppYDimension,
                data.min_height, data.max_height);
  } else {
    ippAddInteger(media_size.get(), IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                  kIppYDimension, data.min_height);
  }
  ippAddCollection(media_col.get(), IPP_TAG_PRINTER, kIppMediaSize,
                   media_size.get());

  ippAddInteger(media_col.get(), IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                kIppMediaBottomMargin, data.bottom_margin);
  ippAddInteger(media_col.get(), IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                kIppMediaLeftMargin, data.left_margin);
  ippAddInteger(media_col.get(), IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                kIppMediaRightMargin, data.right_margin);
  ippAddInteger(media_col.get(), IPP_TAG_PRINTER, IPP_TAG_INTEGER,
                kIppMediaTopMargin, data.top_margin);

  return media_col;
}

void FilterMediaColSizes(ScopedIppPtr& attributes) {
  if (!attributes) {
    return;
  }

  ipp_attribute_t* media_col_db = ippFindAttribute(
      attributes.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  if (!media_col_db) {
    return;
  }

  // `fixed_widths` is the width for any media-col-database entry that is
  // non-variable.  For instance, if the media-col-database has 4 fixed
  // widthxheight entries and one variable widthxheight entry like so:
  //
  // 58mmx200mm
  // 58mmx2000mm
  // 80mmx200mm
  // 80mmx2000mm
  // 10-80mmx20-2000mm
  //
  // then `fixed_widths` will contain 58 and 80.  After filtering, the following
  // sizes will exist:
  //
  // 58mmx200mm
  // 58mmx2000mm
  // 80mmx200mm
  // 80mmx2000mm
  // 58mmx20-2000mm
  // 80mmx20-2000mm
  std::set<int> fixed_widths;
  std::vector<ScopedIppPtr> new_entries;
  std::vector<MediaColData> variable_height_entries;

  // Loop over all the `media_col_db` entries and either add them to
  // `new_entries`, skip them, or save them in `variable_height_entries` so
  // they can be added later (once all the fixed widths have been discovered).
  for (int i = 0; i < ippGetCount(media_col_db); i++) {
    ipp_t* db_entry = ippGetCollection(media_col_db, i);
    std::optional<MediaColData> size = ExtractMediaColData(db_entry);
    if (!size.has_value()) {
      return;
    }

    // TODO(nmuggli): Consider adding the boundaries of a variable-width entry
    // to `fixed_widths`.
    if (!size->HasVariableWidth()) {
      fixed_widths.insert(size->min_width);
    }

    // Handle four different cases:
    // 1.  Variable width and fixed height.  Skip this - variable widths are not
    // supported.
    // 2.  Fixed width and fixed height.  Add to the new array.
    // 3.  Fixed width and variable height.  Add to the new array.
    // 4.  Variable width and variable height.  Save this entry until after
    // all of the entries are processed.  For each fixed width that fits
    // within this variable width, a new entry will get added with the
    // variable height.

    // Case 1:  skip over this.
    if (size->HasVariableWidth() && !size->HasVariableHeight()) {
      continue;
    }

    // Case 2 and case 3 - add to `new_entries`.
    if (!size->HasVariableWidth()) {
      ScopedIppPtr new_entry = NewMediaCollection(size.value());
      if (new_entry) {
        new_entries.push_back(std::move(new_entry));
      }
      continue;
    }

    // Case 4: Save entry for later processing.
    variable_height_entries.push_back(size.value());
  }

  for (const MediaColData& variable_height_entry : variable_height_entries) {
    // For each fixed width, create a new media size entry for that width and a
    // variable height.
    for (int width : fixed_widths) {
      MediaColData size = variable_height_entry;
      // Ensure `width` fits within the variable width.
      if (width < size.min_width || width > size.max_width) {
        continue;
      }
      size.min_width = width;
      size.max_width = width;
      ScopedIppPtr new_entry = NewMediaCollection(size);
      if (new_entry) {
        new_entries.push_back(std::move(new_entry));
      }
    }
  }

  // Finally, update the attribute that was passed in with the new entries.
  ippDeleteAttribute(attributes.get(), media_col_db);
  std::vector<const ipp_t*> raw_entries(new_entries.size());
  base::ranges::transform(new_entries, raw_entries.begin(), &ScopedIppPtr::get);
  ippAddCollections(attributes.get(), IPP_TAG_PRINTER, kIppMediaColDatabase,
                    raw_entries.size(), raw_entries.data());
}

}  //  namespace printing
