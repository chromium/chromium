// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "printing/backend/cups_helper.h"

#include <cups/ppd.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "printing/backend/cups_deleters.h"
#include "printing/backend/cups_weak_functions.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants_cups.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using base::EqualsCaseInsensitiveASCII;

namespace printing {

// This section contains helper code for PPD parsing for semantic capabilities.
namespace {

// Timeout for establishing a CUPS connection.  It is expected that cupsd is
// able to start and respond on all systems within this duration.
constexpr base::TimeDelta kCupsTimeout = base::Seconds(5);

// CUPS default max copies value (parsed from kCupsMaxCopies PPD attribute).
constexpr int32_t kDefaultMaxCopies = 9999;
constexpr char kCupsMaxCopies[] = "cupsMaxCopies";

constexpr char kColorDevice[] = "ColorDevice";

constexpr char kDuplex[] = "Duplex";
constexpr char kDuplexNone[] = "None";
constexpr char kDuplexNoTumble[] = "DuplexNoTumble";
constexpr char kDuplexTumble[] = "DuplexTumble";
constexpr char kPageSize[] = "PageSize";

// Brother printer specific options.
constexpr char kBrotherDuplex[] = "BRDuplex";

int32_t GetCopiesMax(ppd_file_t* ppd) {
  ppd_attr_t* attr = ppdFindAttr(ppd, kCupsMaxCopies, nullptr);
  if (!attr || !attr->value) {
    return kDefaultMaxCopies;
  }

  int32_t ret;
  return base::StringToInt(attr->value, &ret) ? ret : kDefaultMaxCopies;
}

std::pair<std::vector<mojom::DuplexMode>, mojom::DuplexMode> GetDuplexSettings(
    ppd_file_t* ppd) {
  std::vector<mojom::DuplexMode> duplex_modes;
  mojom::DuplexMode duplex_default = mojom::DuplexMode::kUnknownDuplexMode;

  ppd_choice_t* duplex_choice = ppdFindMarkedChoice(ppd, kDuplex);
  ppd_option_t* option = ppdFindOption(ppd, kDuplex);
  if (!option)
    option = ppdFindOption(ppd, kBrotherDuplex);

  if (!option)
    return std::make_pair(std::move(duplex_modes), duplex_default);

  if (!duplex_choice)
    duplex_choice = ppdFindChoice(option, option->defchoice);

  if (ppdFindChoice(option, kDuplexNone))
    duplex_modes.push_back(mojom::DuplexMode::kSimplex);

  if (ppdFindChoice(option, kDuplexNoTumble))
    duplex_modes.push_back(mojom::DuplexMode::kLongEdge);

  if (ppdFindChoice(option, kDuplexTumble))
    duplex_modes.push_back(mojom::DuplexMode::kShortEdge);

  if (!duplex_choice)
    return std::make_pair(std::move(duplex_modes), duplex_default);

  const char* choice = duplex_choice->choice;
  if (EqualsCaseInsensitiveASCII(choice, kDuplexNone)) {
    duplex_default = mojom::DuplexMode::kSimplex;
  } else if (EqualsCaseInsensitiveASCII(choice, kDuplexTumble)) {
    duplex_default = mojom::DuplexMode::kShortEdge;
  } else {
    duplex_default = mojom::DuplexMode::kLongEdge;
  }
  return std::make_pair(std::move(duplex_modes), duplex_default);
}

std::optional<gfx::Size> ParseResolutionString(const char* input) {
  int len = strlen(input);
  if (len == 0) {
    VLOG(1) << "Bad PPD resolution choice: null string";
    return std::nullopt;
  }

  int n = 0;  // number of chars successfully parsed by sscanf()
  int dpi_x;
  int dpi_y;
  sscanf(input, "%ddpi%n", &dpi_x, &n);
  if (n == len) {
    dpi_y = dpi_x;
  } else {
    sscanf(input, "%dx%ddpi%n", &dpi_x, &dpi_y, &n);
    if (n != len) {
      VLOG(1) << "Bad PPD resolution choice: " << input;
      return std::nullopt;
    }
  }
  if (dpi_x <= 0 || dpi_y <= 0) {
    VLOG(1) << "Invalid PPD resolution dimensions: " << dpi_x << " " << dpi_y;
    return std::nullopt;
  }

  return gfx::Size(dpi_x, dpi_y);
}

std::pair<std::vector<gfx::Size>, gfx::Size> GetResolutionSettings(
    ppd_file_t* ppd) {
  static constexpr const char* kResolutions[] = {
      "Resolution",     "JCLResolution", "SetResolution", "CNRes_PGP",
      "HPPrintQuality", "LXResolution",  "BRResolution"};
  ppd_option_t* res;
  for (const char* res_name : kResolutions) {
    res = ppdFindOption(ppd, res_name);
    if (res)
      break;
  }

  // Some printers, such as Generic-CUPS-BRF-Printer, do not specify a
  // resolution in their ppd file. Provide a default DPI if no valid DPI is
  // found.
#if BUILDFLAG(IS_MAC)
  constexpr gfx::Size kDefaultMissingDpi(kDefaultMacDpi, kDefaultMacDpi);
#else
  constexpr gfx::Size kDefaultMissingDpi(kDefaultPdfDpi, kDefaultPdfDpi);
#endif

  std::vector<gfx::Size> dpis;
  gfx::Size default_dpi;
  if (res) {
    for (int i = 0; i < res->num_choices; i++) {
      char* choice = res->choices[i].choice;
      CHECK(choice);
      std::optional<gfx::Size> parsed_size = ParseResolutionString(choice);
      if (!parsed_size.has_value()) {
        continue;
      }

      dpis.push_back(parsed_size.value());
      if (!strcmp(choice, res->defchoice)) {
        default_dpi = dpis.back();
      }
    }
  } else {
    // If there is no resolution option, then check for a standalone
    // DefaultResolution.
    ppd_attr_t* attr = ppdFindAttr(ppd, "DefaultResolution", nullptr);
    if (attr) {
      CHECK(attr->value);
      std::optional<gfx::Size> parsed_size = ParseResolutionString(attr->value);
      if (parsed_size.has_value()) {
        dpis.push_back(parsed_size.value());
        default_dpi = parsed_size.value();
      }
    }
  }

  if (dpis.empty()) {
    dpis.push_back(kDefaultMissingDpi);
    default_dpi = kDefaultMissingDpi;
  }
  return std::make_pair(std::move(dpis), default_dpi);
}

bool GetBasicColorModelSettings(ppd_file_t* ppd,
                                mojom::ColorModel* color_model_for_black,
                                mojom::ColorModel* color_model_for_color,
                                bool* color_is_default) {
  ppd_option_t* color_model = ppdFindOption(ppd, kCUPSColorModel);
  if (!color_model)
    return false;

  if (ppdFindChoice(color_model, kBlack))
    *color_model_for_black = mojom::ColorModel::kBlack;
  else if (ppdFindChoice(color_model, kGray))
    *color_model_for_black = mojom::ColorModel::kGray;
  else if (ppdFindChoice(color_model, kGrayscale))
    *color_model_for_black = mojom::ColorModel::kGrayscale;

  if (ppdFindChoice(color_model, kColor))
    *color_model_for_color = mojom::ColorModel::kColor;
  else if (ppdFindChoice(color_model, kCMYK))
    *color_model_for_color = mojom::ColorModel::kCMYK;
  else if (ppdFindChoice(color_model, kRGB))
    *color_model_for_color = mojom::ColorModel::kRGB;
  else if (ppdFindChoice(color_model, kRGBA))
    *color_model_for_color = mojom::ColorModel::kRGBA;
  else if (ppdFindChoice(color_model, kRGB16))
    *color_model_for_color = mojom::ColorModel::kRGB16;
  else if (ppdFindChoice(color_model, kCMY))
    *color_model_for_color = mojom::ColorModel::kCMY;
  else if (ppdFindChoice(color_model, kKCMY))
    *color_model_for_color = mojom::ColorModel::kKCMY;
  else if (ppdFindChoice(color_model, kCMY_K))
    *color_model_for_color = mojom::ColorModel::kCMYPlusK;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kCUPSColorModel);
  if (!marked_choice)
    marked_choice = ppdFindChoice(color_model, color_model->defchoice);

  if (marked_choice) {
    *color_is_default =
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kBlack) &&
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kGray) &&
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kGrayscale);
  }
  return true;
}

bool GetPrintOutModeColorSettings(ppd_file_t* ppd,
                                  mojom::ColorModel* color_model_for_black,
                                  mojom::ColorModel* color_model_for_color,
                                  bool* color_is_default) {
  ppd_option_t* printout_mode = ppdFindOption(ppd, kCUPSPrintoutMode);
  if (!printout_mode)
    return false;

  *color_model_for_color = mojom::ColorModel::kPrintoutModeNormal;
  *color_model_for_black = mojom::ColorModel::kPrintoutModeNormal;

  // Check to see if NORMAL_GRAY value is supported by PrintoutMode.
  // If NORMAL_GRAY is not supported, NORMAL value is used to
  // represent grayscale. If NORMAL_GRAY is supported, NORMAL is used to
  // represent color.
  if (ppdFindChoice(printout_mode, kNormalGray))
    *color_model_for_black = mojom::ColorModel::kPrintoutModeNormalGray;

  // Get the default marked choice to identify the default color setting
  // value.
  ppd_choice_t* printout_mode_choice =
      ppdFindMarkedChoice(ppd, kCUPSPrintoutMode);
  if (!printout_mode_choice) {
    printout_mode_choice =
        ppdFindChoice(printout_mode, printout_mode->defchoice);
  }
  if (printout_mode_choice) {
    if (EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kNormalGray) ||
        EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kHighGray) ||
        EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kDraftGray)) {
      *color_model_for_black = mojom::ColorModel::kPrintoutModeNormalGray;
      *color_is_default = false;
    }
  }
  return true;
}

bool GetColorModeSettings(ppd_file_t* ppd,
                          mojom::ColorModel* color_model_for_black,
                          mojom::ColorModel* color_model_for_color,
                          bool* color_is_default) {
  // Samsung printers use "ColorMode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSColorMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kColor) ||
      ppdFindChoice(color_mode_option, kSamsungColorTrue)) {
    *color_model_for_color = mojom::ColorModel::kColorModeColor;
  }

  if (ppdFindChoice(color_mode_option, kMonochrome) ||
      ppdFindChoice(color_mode_option, kSamsungColorFalse)) {
    *color_model_for_black = mojom::ColorModel::kColorModeMonochrome;
  }

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSColorMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        EqualsCaseInsensitiveASCII(mode_choice->choice, kColor) ||
        EqualsCaseInsensitiveASCII(mode_choice->choice, kSamsungColorTrue);
  }
  return true;
}

bool GetBrotherColorSettings(ppd_file_t* ppd,
                             mojom::ColorModel* color_model_for_black,
                             mojom::ColorModel* color_model_for_color,
                             bool* color_is_default) {
  // Some Brother printers use "BRMonoColor" attribute in their PPDs.
  // Some Brother printers use "BRPrintQuality" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSBrotherMonoColor);
  if (!color_mode_option)
    color_mode_option = ppdFindOption(ppd, kCUPSBrotherPrintQuality);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kFullColor))
    *color_model_for_color = mojom::ColorModel::kBrotherCUPSColor;
  else if (ppdFindChoice(color_mode_option, kColor))
    *color_model_for_color = mojom::ColorModel::kBrotherBRScript3Color;

  if (ppdFindChoice(color_mode_option, kMono))
    *color_model_for_black = mojom::ColorModel::kBrotherCUPSMono;
  else if (ppdFindChoice(color_mode_option, kBlack))
    *color_model_for_black = mojom::ColorModel::kBrotherBRScript3Black;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kCUPSColorMode);
  if (!marked_choice) {
    marked_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (marked_choice) {
    *color_is_default =
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kBlack) &&
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kMono);
  }
  return true;
}

bool GetHPColorSettings(ppd_file_t* ppd,
                        mojom::ColorModel* color_model_for_black,
                        mojom::ColorModel* color_model_for_color,
                        bool* color_is_default) {
  // Some HP printers use "Color/Color Model" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kColor);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kColor))
    *color_model_for_color = mojom::ColorModel::kHPColorColor;
  if (ppdFindChoice(color_mode_option, kBlack))
    *color_model_for_black = mojom::ColorModel::kHPColorBlack;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSColorMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kColor);
  }
  return true;
}

bool GetHPColorModeSettings(ppd_file_t* ppd,
                            mojom::ColorModel* color_model_for_black,
                            mojom::ColorModel* color_model_for_color,
                            bool* color_is_default) {
  // Some HP printers use "HPColorMode/Mode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSHpColorMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kHpColorPrint))
    *color_model_for_color = mojom::ColorModel::kHPColorColor;
  if (ppdFindChoice(color_mode_option, kHpGrayscalePrint))
    *color_model_for_black = mojom::ColorModel::kHPColorBlack;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSHpColorMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (mode_choice) {
    *color_is_default =
        EqualsCaseInsensitiveASCII(mode_choice->choice, kHpColorPrint);
  }
  return true;
}

bool GetHpPjlColorAsGrayModeSettings(ppd_file_t* ppd,
                                     mojom::ColorModel* color_model_for_black,
                                     mojom::ColorModel* color_model_for_color,
                                     bool* color_is_default) {
  // Some HP printers use "HPPJLColorAsGray" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSHpPjlColorAsGray);
  if (!color_mode_option) {
    return false;
  }

  if (ppdFindChoice(color_mode_option, kHpPjlColorAsGrayYes)) {
    *color_model_for_black = mojom::ColorModel::kHpPjlColorAsGrayYes;
  }

  if (ppdFindChoice(color_mode_option, kHpPjlColorAsGrayNo)) {
    *color_model_for_color = mojom::ColorModel::kHpPjlColorAsGrayNo;
  }

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kCUPSHpPjlColorAsGray);
  if (!marked_choice) {
    marked_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (marked_choice) {
    *color_is_default =
        EqualsCaseInsensitiveASCII(marked_choice->choice, kHpPjlColorAsGrayNo);
  }
  return true;
}

bool GetCanonCNColorModeSettings(ppd_file_t* ppd,
                                 mojom::ColorModel* color_model_for_black,
                                 mojom::ColorModel* color_model_for_color,
                                 bool* color_is_default) {
  // Some Canon printers use "CNColorMode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSCanonCNColorMode);
  if (!color_mode_option) {
    return false;
  }

  if (ppdFindChoice(color_mode_option, kColor)) {
    *color_model_for_color = mojom::ColorModel::kCanonCNColorModeColor;
  }

  if (ppdFindChoice(color_mode_option, kMono)) {
    *color_model_for_black = mojom::ColorModel::kCanonCNColorModeMono;
  }

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kCUPSCanonCNColorMode);
  if (!marked_choice) {
    marked_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (marked_choice) {
    *color_is_default =
        EqualsCaseInsensitiveASCII(marked_choice->choice, kColor);
  }
  return true;
}

bool GetCanonCNIJGrayscaleSettings(ppd_file_t* ppd,
                                   mojom::ColorModel* color_model_for_black,
                                   mojom::ColorModel* color_model_for_color,
                                   bool* color_is_default) {
  // Some Canon printers use "CNIJGrayScale" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSCanonCNIJGrayScale);
  if (!color_mode_option) {
    return false;
  }

  if (ppdFindChoice(color_mode_option, kZero)) {
    *color_model_for_color = mojom::ColorModel::kCanonCNIJGrayScaleZero;
  }
  if (ppdFindChoice(color_mode_option, kOne)) {
    *color_model_for_black = mojom::ColorModel::kCanonCNIJGrayScaleOne;
  }

  ppd_choice_t* marked_choice =
      ppdFindMarkedChoice(ppd, kCUPSCanonCNIJGrayScale);
  if (marked_choice) {
    *color_is_default =
        !EqualsCaseInsensitiveASCII(marked_choice->choice, kOne);
  }
  return true;
}

bool GetEpsonInkSettings(ppd_file_t* ppd,
                         mojom::ColorModel* color_model_for_black,
                         mojom::ColorModel* color_model_for_color,
                         bool* color_is_default) {
  // Epson printers use "Ink" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSEpsonInk);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kEpsonColor))
    *color_model_for_color = mojom::ColorModel::kEpsonInkColor;
  if (ppdFindChoice(color_mode_option, kEpsonMono))
    *color_model_for_black = mojom::ColorModel::kEpsonInkMono;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSEpsonInk);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kColor);
  }
  return true;
}

bool GetKonicaMinoltaSelectColorSettings(
    ppd_file_t* ppd,
    mojom::ColorModel* color_model_for_black,
    mojom::ColorModel* color_model_for_color,
    bool* color_is_default) {
  ppd_option_t* color_mode_option =
      ppdFindOption(ppd, kCUPSKonicaMinoltaSelectColor);
  if (!color_mode_option) {
    return false;
  }

  if (ppdFindChoice(color_mode_option, kColor)) {
    *color_model_for_color = mojom::ColorModel::kColor;
  }
  if (ppdFindChoice(color_mode_option, kGrayscale)) {
    *color_model_for_black = mojom::ColorModel::kGrayscale;
  }

  ppd_choice_t* mode_choice =
      ppdFindMarkedChoice(ppd, kCUPSKonicaMinoltaSelectColor);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kColor);
  }
  return true;
}

bool GetLexmarkBLWSettings(ppd_file_t* ppd,
                           mojom::ColorModel* color_model_for_black,
                           mojom::ColorModel* color_model_for_color,
                           bool* color_is_default) {
  // Lexmark printers use "BLW" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSLexmarkBLW);
  if (!color_mode_option) {
    return false;
  }

  if (ppdFindChoice(color_mode_option, kLexmarkBLWFalse)) {
    *color_model_for_color = mojom::ColorModel::kColor;
  }
  if (ppdFindChoice(color_mode_option, kLexmarkBLWTrue)) {
    *color_model_for_black = mojom::ColorModel::kGray;
  }

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSLexmarkBLW);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kColor);
  }
  return true;
}

bool GetOkiSettings(ppd_file_t* ppd,
                    mojom::ColorModel* color_model_for_black,
                    mojom::ColorModel* color_model_for_color,
                    bool* color_is_default) {
  // Oki printers use "OKControl" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSOkiControl);
  if (!color_mode_option) {
    return false;
  }

  if (ppdFindChoice(color_mode_option, kAuto)) {
    *color_model_for_color = mojom::ColorModel::kOkiOKControlColor;
  }
  if (ppdFindChoice(color_mode_option, kGray)) {
    *color_model_for_black = mojom::ColorModel::kOkiOKControlGray;
  }

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSOkiControl);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kAuto);
  }
  return true;
}

bool GetSharpARCModeSettings(ppd_file_t* ppd,
                             mojom::ColorModel* color_model_for_black,
                             mojom::ColorModel* color_model_for_color,
                             bool* color_is_default) {
  // Sharp printers use "ARCMode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSSharpARCMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kSharpCMColor))
    *color_model_for_color = mojom::ColorModel::kSharpARCModeCMColor;
  if (ppdFindChoice(color_mode_option, kSharpCMBW))
    *color_model_for_black = mojom::ColorModel::kSharpARCModeCMBW;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSSharpARCMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    // Many Sharp printers use "CMAuto" as the default color mode.
    *color_is_default =
        !EqualsCaseInsensitiveASCII(mode_choice->choice, kSharpCMBW);
  }
  return true;
}

bool GetXeroxColorSettings(ppd_file_t* ppd,
                           mojom::ColorModel* color_model_for_black,
                           mojom::ColorModel* color_model_for_color,
                           bool* color_is_default) {
  // Some Xerox printers use "XRXColor" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSXeroxXRXColor);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kXeroxAutomatic))
    *color_model_for_color = mojom::ColorModel::kXeroxXRXColorAutomatic;
  if (ppdFindChoice(color_mode_option, kXeroxBW))
    *color_model_for_black = mojom::ColorModel::kXeroxXRXColorBW;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSXeroxXRXColor);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    // Many Xerox printers use "Automatic" as the default color mode.
    *color_is_default =
        !EqualsCaseInsensitiveASCII(mode_choice->choice, kXeroxBW);
  }
  return true;
}

bool GetXeroxOutputColorSettings(ppd_file_t* ppd,
                                 mojom::ColorModel* color_model_for_black,
                                 mojom::ColorModel* color_model_for_color,
                                 bool* color_is_default) {
  // Some Xerox printers use "XROutputColor" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSXeroxXROutputColor);
  if (!color_mode_option) {
    return false;
  }

  if (ppdFindChoice(color_mode_option, kPrintAsColor)) {
    *color_model_for_color = mojom::ColorModel::kXeroxXROutputColorPrintAsColor;
  }
  if (ppdFindChoice(color_mode_option, kPrintAsGrayscale)) {
    *color_model_for_black =
        mojom::ColorModel::kXeroxXROutputColorPrintAsGrayscale;
  }

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSXeroxXROutputColor);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        EqualsCaseInsensitiveASCII(mode_choice->choice, kPrintAsColor);
  }
  return true;
}

bool GetProcessColorModelSettings(ppd_file_t* ppd,
                                  mojom::ColorModel* color_model_for_black,
                                  mojom::ColorModel* color_model_for_color,
                                  bool* color_is_default) {
  // Canon printers use "ProcessColorModel" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kCUPSProcessColorModel);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kRGB))
    *color_model_for_color = mojom::ColorModel::kProcessColorModelRGB;
  else if (ppdFindChoice(color_mode_option, kCMYK))
    *color_model_for_color = mojom::ColorModel::kProcessColorModelCMYK;

  if (ppdFindChoice(color_mode_option, kGreyscale))
    *color_model_for_black = mojom::ColorModel::kProcessColorModelGreyscale;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kCUPSProcessColorModel);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }

  if (mode_choice) {
    *color_is_default =
        !EqualsCaseInsensitiveASCII(mode_choice->choice, kGreyscale);
  }
  return true;
}

bool GetColorModelSettings(ppd_file_t* ppd,
                           mojom::ColorModel* cm_black,
                           mojom::ColorModel* cm_color,
                           bool* is_color) {
  bool is_color_device = false;
  ppd_attr_t* attr = ppdFindAttr(ppd, kColorDevice, nullptr);
  if (attr && attr->value)
    is_color_device = ppd->color_device;

  *is_color = is_color_device;
  return (is_color_device &&
          GetBasicColorModelSettings(ppd, cm_black, cm_color, is_color)) ||
         GetPrintOutModeColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetColorModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetHPColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetHPColorModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetHpPjlColorAsGrayModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetBrotherColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetCanonCNColorModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetCanonCNIJGrayscaleSettings(ppd, cm_black, cm_color, is_color) ||
         GetEpsonInkSettings(ppd, cm_black, cm_color, is_color) ||
         GetKonicaMinoltaSelectColorSettings(ppd, cm_black, cm_color,
                                             is_color) ||
         GetLexmarkBLWSettings(ppd, cm_black, cm_color, is_color) ||
         GetOkiSettings(ppd, cm_black, cm_color, is_color) ||
         GetSharpARCModeSettings(ppd, cm_black, cm_color, is_color) ||
         GetXeroxColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetXeroxOutputColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetProcessColorModelSettings(ppd, cm_black, cm_color, is_color);
}

// Default port for IPP print servers.
const int kDefaultIPPServerPort = 631;

}  // namespace

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
HttpConnectionCUPS::HttpConnectionCUPS(const GURL& print_server_url,
                                       http_encryption_t encryption,
                                       bool blocking) {
  // If we have an empty url, use default print server.
  if (print_server_url.is_empty())
    return;

  int port = print_server_url.IntPort();
  if (port == url::PORT_UNSPECIFIED)
    port = kDefaultIPPServerPort;

  http_ = HttpConnect2(print_server_url.host().c_str(), port,
                       /*addrlist=*/nullptr, AF_UNSPEC, encryption,
                       blocking ? 1 : 0, kCupsTimeout.InMilliseconds(),
                       /*cancel=*/nullptr);
}

HttpConnectionCUPS::~HttpConnectionCUPS() = default;

http_t* HttpConnectionCUPS::http() {
  return http_.get();
}

bool ParsePpdCapabilities(cups_dest_t* dest,
                          std::string_view locale,
                          std::string_view printer_capabilities,
                          PrinterSemanticCapsAndDefaults* printer_info) {
  // A file created while in a sandbox will be automatically deleted once all
  // handles to it have been closed.  This precludes the use of multiple
  // operations against a file path.
  //
  // Underlying CUPS libraries process the PPD using standard I/O file
  // descriptors, so `FILE` stream APIs that don't support that are not an
  // option (e.g., can't use fmemopen()).
  //
  // Previous attempts to just read & write with a single disk `FILE` stream
  // demonstrated occasional data corruption in the wild, so resort to working
  // directly with lower-level file descriptors.
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir))
    return false;

  base::FilePath ppd_file_path;
  base::ScopedFD ppd_fd =
      base::CreateAndOpenFdForTemporaryFileInDir(temp_dir, &ppd_file_path);
  if (!ppd_fd.is_valid())
    return false;

  // Unlike Windows, POSIX platforms do not have the ability to mark files as
  // "delete on close". So just delete `ppd_file_path` here. The file is still
  // accessible via `ppd_fd`.
  if (!base::DeleteFile(ppd_file_path))
    return false;

  if (!base::WriteFileDescriptor(ppd_fd.get(), printer_capabilities) ||
      lseek(ppd_fd.get(), 0, SEEK_SET) == -1) {
    return false;
  }

  // We release ownership of `ppd_fd` here because ppdOpenFd() assumes ownership
  // of it in all but one case (see below).
  int unowned_ppd_fd = ppd_fd.release();
  ppd_file_t* ppd = ppdOpenFd(unowned_ppd_fd);
  if (!ppd) {
    int line = 0;
    ppd_status_t ppd_status = ppdLastError(&line);
    LOG(ERROR) << "Failed to open PDD file: error " << ppd_status << " at line "
               << line << ", " << ppdErrorString(ppd_status);
    if (ppd_status == PPD_FILE_OPEN_ERROR) {
      // Normally ppdOpenFd assumes ownership of the file descriptor we give it,
      // regardless of success or failure. The one exception is when it fails
      // with PPD_FILE_OPEN_ERROR. In that case ownership is retained by the
      // caller, so we must explicitly close it.
      close(unowned_ppd_fd);
    }
    return false;
  }

  ppdMarkDefaults(ppd);
  if (dest)
    cupsMarkOptions(ppd, dest->num_options, dest->options);

  PrinterSemanticCapsAndDefaults caps;
  caps.collate_capable = true;
  caps.collate_default = true;
  caps.copies_max = GetCopiesMax(ppd);

  std::tie(caps.duplex_modes, caps.duplex_default) = GetDuplexSettings(ppd);
  std::tie(caps.dpis, caps.default_dpi) = GetResolutionSettings(ppd);

  mojom::ColorModel cm_black = mojom::ColorModel::kUnknownColorModel;
  mojom::ColorModel cm_color = mojom::ColorModel::kUnknownColorModel;
  bool is_color = false;
  if (!GetColorModelSettings(ppd, &cm_black, &cm_color, &is_color)) {
    VLOG(1) << "Unknown printer color model";
  }

  caps.color_changeable =
      ((cm_color != mojom::ColorModel::kUnknownColorModel) &&
       (cm_black != mojom::ColorModel::kUnknownColorModel) &&
       (cm_color != cm_black));
  caps.color_default = is_color;
  caps.color_model = cm_color;
  caps.bw_model = cm_black;

  if (ppd->num_sizes > 0 && ppd->sizes) {
    VLOG(1) << "Paper list size - " << ppd->num_sizes;
    ppd_option_t* paper_option = ppdFindOption(ppd, kPageSize);
    bool is_default_found = false;
    for (int i = 0; i < ppd->num_sizes; ++i) {
      const gfx::Size paper_size_um(
          ConvertUnit(ppd->sizes[i].width, kPointsPerInch, kMicronsPerInch),
          ConvertUnit(ppd->sizes[i].length, kPointsPerInch, kMicronsPerInch));
      if (!paper_size_um.IsEmpty()) {
        std::string display_name;
        if (paper_option) {
          ppd_choice_t* paper_choice =
              ppdFindChoice(paper_option, ppd->sizes[i].name);
          // Human readable paper name should be UTF-8 encoded, but some PPDs
          // do not follow this standard.
          if (paper_choice && base::IsStringUTF8(paper_choice->text)) {
            display_name = paper_choice->text;
          }
        }
        int printable_area_left_um =
            ConvertUnit(ppd->sizes[i].left, kPointsPerInch, kMicronsPerInch);
        int printable_area_bottom_um =
            ConvertUnit(ppd->sizes[i].bottom, kPointsPerInch, kMicronsPerInch);
        // ppd->sizes[i].right is the horizontal distance from the left of the
        // paper to the right of the printable area.
        int printable_area_right_um =
            ConvertUnit(ppd->sizes[i].right, kPointsPerInch, kMicronsPerInch);
        // ppd->sizes[i].top is the vertical distance from the bottom of the
        // paper to the top of the printable area.
        int printable_area_top_um =
            ConvertUnit(ppd->sizes[i].top, kPointsPerInch, kMicronsPerInch);

        gfx::Rect printable_area_um(
            printable_area_left_um, printable_area_bottom_um,
            /*width=*/printable_area_right_um - printable_area_left_um,
            /*height=*/printable_area_top_um - printable_area_bottom_um);

        // Default to the paper size if printable area is empty.
        // We've seen some drivers have a printable area that goes out of bounds
        // of the paper size. In those cases, set the printable area to be the
        // size. (See crbug.com/1412305.)
        const gfx::Rect size_um_rect = gfx::Rect(paper_size_um);
        if (printable_area_um.IsEmpty() ||
            !size_um_rect.Contains(printable_area_um)) {
          printable_area_um = size_um_rect;
        }

        PrinterSemanticCapsAndDefaults::Paper paper(
            display_name,
            /*vendor_id=*/ppd->sizes[i].name, paper_size_um, printable_area_um);

        caps.papers.push_back(paper);
        if (ppd->sizes[i].marked) {
          caps.default_paper = paper;
          is_default_found = true;
        }
      }
    }
    if (!is_default_found) {
      gfx::Size locale_paper_um = GetDefaultPaperSizeFromLocaleMicrons(locale);
      for (const PrinterSemanticCapsAndDefaults::Paper& paper : caps.papers) {
        // Set epsilon to 500 microns to allow tolerance of rounded paper sizes.
        // While the above utility function returns paper sizes in microns, they
        // are still rounded to the nearest millimeter (1000 microns).
        constexpr int kSizeEpsilon = 500;
        if (SizesEqualWithinEpsilon(paper.size_um(), locale_paper_um,
                                    kSizeEpsilon)) {
          caps.default_paper = paper;
          is_default_found = true;
          break;
        }
      }

      // If no default was set in the PPD or if the locale default is not within
      // the printer's capabilities, select the first on the list.
      if (!is_default_found)
        caps.default_paper = caps.papers[0];
    }
  }

  ppdClose(ppd);

  *printer_info = caps;
  return true;
}

ScopedHttpPtr HttpConnect2(const char* host,
                           int port,
                           http_addrlist_t* addrlist,
                           int family,
                           http_encryption_t encryption,
                           int blocking,
                           int msec,
                           int* cancel) {
  ScopedHttpPtr http;
  if (httpConnect2) {
    http.reset(httpConnect2(host, port,
                            /*addrlist=*/nullptr, AF_UNSPEC, encryption,
                            blocking ? 1 : 0, kCupsTimeout.InMilliseconds(),
                            /*cancel=*/nullptr));
  } else {
    // Continue to use deprecated CUPS calls because because older Linux
    // distribution such as RHEL/CentOS 7 are shipped with CUPS 1.6.
    http.reset(httpConnectEncrypt(host, port, encryption));
  }

  if (!http) {
    LOG(ERROR) << "CP_CUPS: Failed connecting to print server: " << host;
    return nullptr;
  }

  if (!httpConnect2) {
    httpBlocking(http.get(), blocking ? 1 : 0);
  }

  return http;
}

}  // namespace printing
