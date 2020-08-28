// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/atomic_sequence_num.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "printing/units.h"

#if defined(USE_CUPS) && (defined(OS_MAC) || defined(OS_CHROMEOS))
#include <cups/cups.h>
#endif

namespace printing {

namespace {

base::LazyInstance<std::string>::Leaky g_user_agent;

}  // namespace

void SetAgent(const std::string& user_agent) {
  g_user_agent.Get() = user_agent;
}

const std::string& GetAgent() {
  return g_user_agent.Get();
}

mojom::ColorModel ColorModeToColorModel(int color_mode) {
  if (color_mode < static_cast<int>(mojom::ColorModel::kUnknownColorModel) ||
      color_mode > static_cast<int>(mojom::ColorModel::kColorModelLast))
    return mojom::ColorModel::kUnknownColorModel;
  return static_cast<mojom::ColorModel>(color_mode);
}

#if defined(USE_CUPS)
void GetColorModelForModel(mojom::ColorModel color_model,
                           std::string* color_setting_name,
                           std::string* color_value) {
#if defined(OS_MAC)
  constexpr char kCUPSColorMode[] = "ColorMode";
  constexpr char kCUPSColorModel[] = "ColorModel";
  constexpr char kCUPSPrintoutMode[] = "PrintoutMode";
  constexpr char kCUPSProcessColorModel[] = "ProcessColorModel";
  constexpr char kCUPSBrotherMonoColor[] = "BRMonoColor";
  constexpr char kCUPSBrotherPrintQuality[] = "BRPrintQuality";
  constexpr char kCUPSEpsonInk[] = "Ink";
  constexpr char kCUPSSharpARCMode[] = "ARCMode";
  constexpr char kCUPSXeroxXRXColor[] = "XRXColor";
#else
  constexpr char kCUPSColorMode[] = "cups-ColorMode";
  constexpr char kCUPSColorModel[] = "cups-ColorModel";
  constexpr char kCUPSPrintoutMode[] = "cups-PrintoutMode";
  constexpr char kCUPSProcessColorModel[] = "cups-ProcessColorModel";
  constexpr char kCUPSBrotherMonoColor[] = "cups-BRMonoColor";
  constexpr char kCUPSBrotherPrintQuality[] = "cups-BRPrintQuality";
  constexpr char kCUPSEpsonInk[] = "cups-Ink";
  constexpr char kCUPSSharpARCMode[] = "cups-ARCMode";
  constexpr char kCUPSXeroxXRXColor[] = "cups-XRXColor";
#endif  // defined(OS_MAC)

  *color_setting_name = kCUPSColorModel;

  switch (color_model) {
    case mojom::ColorModel::kUnknownColorModel:
      *color_value = kGrayscale;
      break;
    case mojom::ColorModel::kGray:
      *color_value = kGray;
      break;
    case mojom::ColorModel::kColor:
      *color_value = kColor;
      break;
    case mojom::ColorModel::kCMYK:
      *color_value = kCMYK;
      break;
    case mojom::ColorModel::kCMY:
      *color_value = kCMY;
      break;
    case mojom::ColorModel::kKCMY:
      *color_value = kKCMY;
      break;
    case mojom::ColorModel::kCMYPlusK:
      *color_value = kCMY_K;
      break;
    case mojom::ColorModel::kBlack:
      *color_value = kBlack;
      break;
    case mojom::ColorModel::kGrayscale:
      *color_value = kGrayscale;
      break;
    case mojom::ColorModel::kRGB:
      *color_value = kRGB;
      break;
    case mojom::ColorModel::kRGB16:
      *color_value = kRGB16;
      break;
    case mojom::ColorModel::kRGBA:
      *color_value = kRGBA;
      break;
    case mojom::ColorModel::kColorModeColor:
      *color_setting_name = kCUPSColorMode;
      *color_value = kColor;
      break;
    case mojom::ColorModel::kColorModeMonochrome:
      *color_setting_name = kCUPSColorMode;
      *color_value = kMonochrome;
      break;
    case mojom::ColorModel::kHPColorColor:
      *color_setting_name = kColor;
      *color_value = kColor;
      break;
    case mojom::ColorModel::kHPColorBlack:
      *color_setting_name = kColor;
      *color_value = kBlack;
      break;
    case mojom::ColorModel::kPrintoutModeNormal:
      *color_setting_name = kCUPSPrintoutMode;
      *color_value = kNormal;
      break;
    case mojom::ColorModel::kPrintoutModeNormalGray:
      *color_setting_name = kCUPSPrintoutMode;
      *color_value = kNormalGray;
      break;
    case mojom::ColorModel::kProcessColorModelCMYK:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kCMYK;
      break;
    case mojom::ColorModel::kProcessColorModelGreyscale:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kGreyscale;
      break;
    case mojom::ColorModel::kProcessColorModelRGB:
      *color_setting_name = kCUPSProcessColorModel;
      *color_value = kRGB;
      break;
    case mojom::ColorModel::kBrotherCUPSColor:
      *color_setting_name = kCUPSBrotherMonoColor;
      *color_value = kFullColor;
      break;
    case mojom::ColorModel::kBrotherCUPSMono:
      *color_setting_name = kCUPSBrotherMonoColor;
      *color_value = kMono;
      break;
    case mojom::ColorModel::kBrotherBRScript3Color:
      *color_setting_name = kCUPSBrotherPrintQuality;
      *color_value = kColor;
      break;
    case mojom::ColorModel::kBrotherBRScript3Black:
      *color_setting_name = kCUPSBrotherPrintQuality;
      *color_value = kBlack;
      break;
    case mojom::ColorModel::kEpsonInkColor:
      *color_setting_name = kCUPSEpsonInk;
      *color_value = kEpsonColor;
      break;
    case mojom::ColorModel::kEpsonInkMono:
      *color_setting_name = kCUPSEpsonInk;
      *color_value = kEpsonMono;
      break;
    case mojom::ColorModel::kSharpARCModeCMColor:
      *color_setting_name = kCUPSSharpARCMode;
      *color_value = kSharpCMColor;
      break;
    case mojom::ColorModel::kSharpARCModeCMBW:
      *color_setting_name = kCUPSSharpARCMode;
      *color_value = kSharpCMBW;
      break;
    case mojom::ColorModel::kXeroxXRXColorAutomatic:
      *color_setting_name = kCUPSXeroxXRXColor;
      *color_value = kXeroxAutomatic;
      break;
    case mojom::ColorModel::kXeroxXRXColorBW:
      *color_setting_name = kCUPSXeroxXRXColor;
      *color_value = kXeroxBW;
      break;
  }
  // The default case is excluded from the above switch statement to ensure that
  // all ColorModel values are determinantly handled.
}

#if defined(OS_MAC) || defined(OS_CHROMEOS)
std::string GetIppColorModelForModel(mojom::ColorModel color_model) {
  // Accept |kUnknownColorModel| for consistency with GetColorModelForModel().
  if (color_model == mojom::ColorModel::kUnknownColorModel)
    return CUPS_PRINT_COLOR_MODE_MONOCHROME;

  base::Optional<bool> is_color = IsColorModelSelected(color_model);
  if (!is_color.has_value()) {
    NOTREACHED();
    return std::string();
  }

  return is_color.value() ? CUPS_PRINT_COLOR_MODE_COLOR
                          : CUPS_PRINT_COLOR_MODE_MONOCHROME;
}
#endif  // defined(OS_MAC) || defined(OS_CHROMEOS)
#endif  // defined(USE_CUPS)

base::Optional<bool> IsColorModelSelected(mojom::ColorModel color_model) {
  switch (color_model) {
    case mojom::ColorModel::kColor:
    case mojom::ColorModel::kCMYK:
    case mojom::ColorModel::kCMY:
    case mojom::ColorModel::kKCMY:
    case mojom::ColorModel::kCMYPlusK:
    case mojom::ColorModel::kRGB:
    case mojom::ColorModel::kRGB16:
    case mojom::ColorModel::kRGBA:
    case mojom::ColorModel::kColorModeColor:
    case mojom::ColorModel::kHPColorColor:
    case mojom::ColorModel::kPrintoutModeNormal:
    case mojom::ColorModel::kProcessColorModelCMYK:
    case mojom::ColorModel::kProcessColorModelRGB:
    case mojom::ColorModel::kBrotherCUPSColor:
    case mojom::ColorModel::kBrotherBRScript3Color:
    case mojom::ColorModel::kEpsonInkColor:
    case mojom::ColorModel::kSharpARCModeCMColor:
    case mojom::ColorModel::kXeroxXRXColorAutomatic:
      return true;
    case mojom::ColorModel::kGray:
    case mojom::ColorModel::kBlack:
    case mojom::ColorModel::kGrayscale:
    case mojom::ColorModel::kColorModeMonochrome:
    case mojom::ColorModel::kHPColorBlack:
    case mojom::ColorModel::kPrintoutModeNormalGray:
    case mojom::ColorModel::kProcessColorModelGreyscale:
    case mojom::ColorModel::kBrotherCUPSMono:
    case mojom::ColorModel::kBrotherBRScript3Black:
    case mojom::ColorModel::kEpsonInkMono:
    case mojom::ColorModel::kSharpARCModeCMBW:
    case mojom::ColorModel::kXeroxXRXColorBW:
      return false;
    case mojom::ColorModel::kUnknownColorModel:
      NOTREACHED();
      return base::nullopt;
  }
  // The default case is excluded from the above switch statement to ensure that
  // all ColorModel values are determinantly handled.
}

// Global SequenceNumber used for generating unique cookie values.
static base::AtomicSequenceNumber cookie_seq;

PrintSettings::PrintSettings() {
  Clear();
}

PrintSettings::~PrintSettings() = default;

void PrintSettings::Clear() {
  ranges_.clear();
  selection_only_ = false;
  margin_type_ = mojom::MarginType::kDefaultMargins;
  title_.clear();
  url_.clear();
  display_header_footer_ = false;
  should_print_backgrounds_ = false;
  collate_ = false;
  color_ = mojom::ColorModel::kUnknownColorModel;
  copies_ = 0;
  duplex_mode_ = mojom::DuplexMode::kUnknownDuplexMode;
  device_name_.clear();
  requested_media_ = RequestedMedia();
  page_setup_device_units_.Clear();
  dpi_ = gfx::Size();
  scale_factor_ = 1.0f;
  rasterize_pdf_ = false;
  landscape_ = false;
  supports_alpha_blend_ = true;
#if defined(OS_WIN)
  print_text_with_gdi_ = false;
  printer_type_ = PrintSettings::PrinterType::TYPE_NONE;
#endif
  is_modifiable_ = true;
  pages_per_sheet_ = 1;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  advanced_settings_.clear();
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
#if defined(OS_CHROMEOS)
  send_user_info_ = false;
  username_.clear();
  pin_value_.clear();
#endif  // defined(OS_CHROMEOS)
}

void PrintSettings::SetPrinterPrintableArea(
    const gfx::Size& physical_size_device_units,
    const gfx::Rect& printable_area_device_units,
    bool landscape_needs_flip) {
  int units_per_inch = device_units_per_inch();
  int header_footer_text_height = 0;
  if (display_header_footer_) {
    // Hard-code text_height = 0.5cm = ~1/5 of inch.
    header_footer_text_height = ConvertUnit(kSettingHeaderFooterInterstice,
                                            kPointsPerInch, units_per_inch);
  }

  PageMargins margins;
  bool small_paper_size = false;
  switch (margin_type_) {
    case mojom::MarginType::kDefaultMargins: {
      // Default margins 1.0cm = ~2/5 of an inch, unless a page dimension is
      // less than 2.54 cm = ~1 inch, in which case set the margins in that
      // dimension to 0.
      static constexpr double kCmInMicrons = 10000;
      int margin_printer_units =
          ConvertUnit(kCmInMicrons, kMicronsPerInch, units_per_inch);
      int min_size_printer_units = units_per_inch;
      margins.header = header_footer_text_height;
      margins.footer = header_footer_text_height;
      if (physical_size_device_units.height() > min_size_printer_units) {
        margins.top = margin_printer_units;
        margins.bottom = margin_printer_units;
      } else {
        margins.top = 0;
        margins.bottom = 0;
        small_paper_size = true;
      }
      if (physical_size_device_units.width() > min_size_printer_units) {
        margins.left = margin_printer_units;
        margins.right = margin_printer_units;
      } else {
        margins.left = 0;
        margins.right = 0;
        small_paper_size = true;
      }
      break;
    }
    case mojom::MarginType::kNoMargins:
    case mojom::MarginType::kPrintableAreaMargins: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = 0;
      margins.bottom = 0;
      margins.left = 0;
      margins.right = 0;
      break;
    }
    case mojom::MarginType::kCustomMargins: {
      margins.header = 0;
      margins.footer = 0;
      margins.top = ConvertUnitDouble(requested_custom_margins_in_points_.top,
                                      kPointsPerInch, units_per_inch);
      margins.bottom =
          ConvertUnitDouble(requested_custom_margins_in_points_.bottom,
                            kPointsPerInch, units_per_inch);
      margins.left = ConvertUnitDouble(requested_custom_margins_in_points_.left,
                                       kPointsPerInch, units_per_inch);
      margins.right =
          ConvertUnitDouble(requested_custom_margins_in_points_.right,
                            kPointsPerInch, units_per_inch);
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  if ((margin_type_ == mojom::MarginType::kDefaultMargins ||
       margin_type_ == mojom::MarginType::kPrintableAreaMargins) &&
      !small_paper_size) {
    page_setup_device_units_.SetRequestedMargins(margins);
  } else {
    page_setup_device_units_.ForceRequestedMargins(margins);
  }
  page_setup_device_units_.Init(physical_size_device_units,
                                printable_area_device_units,
                                header_footer_text_height);
  if (landscape_ && landscape_needs_flip)
    page_setup_device_units_.FlipOrientation();
}

void PrintSettings::SetCustomMargins(
    const PageMargins& requested_margins_in_points) {
  requested_custom_margins_in_points_ = requested_margins_in_points;
  margin_type_ = mojom::MarginType::kCustomMargins;
}

int PrintSettings::NewCookie() {
  // A cookie of 0 is used to mark a document as unassigned, count from 1.
  return cookie_seq.GetNext() + 1;
}

void PrintSettings::SetOrientation(bool landscape) {
  if (landscape_ != landscape) {
    landscape_ = landscape;
    page_setup_device_units_.FlipOrientation();
  }
}

}  // namespace printing
