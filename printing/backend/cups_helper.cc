// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_helper.h"

#include <cups/ppd.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/units.h"
#include "url/gurl.h"

using base::EqualsCaseInsensitiveASCII;

namespace printing {

// This section contains helper code for PPD parsing for semantic capabilities.
namespace {

const char kColorDevice[] = "ColorDevice";
const char kColorModel[] = "ColorModel";
const char kColorMode[] = "ColorMode";
const char kProcessColorModel[] = "ProcessColorModel";
const char kPrintoutMode[] = "PrintoutMode";
const char kDraftGray[] = "Draft.Gray";
const char kHighGray[] = "High.Gray";

constexpr char kDuplex[] = "Duplex";
constexpr char kDuplexNone[] = "None";
constexpr char kDuplexNoTumble[] = "DuplexNoTumble";
constexpr char kDuplexTumble[] = "DuplexTumble";
constexpr char kPageSize[] = "PageSize";

// Brother printer specific options.
constexpr char kBrotherDuplex[] = "BRDuplex";
constexpr char kBrotherMonoColor[] = "BRMonoColor";
constexpr char kBrotherPrintQuality[] = "BRPrintQuality";

// Samsung printer specific options.
constexpr char kSamsungColorTrue[] = "True";
constexpr char kSamsungColorFalse[] = "False";

void ParseLpOptions(const base::FilePath& filepath,
                    base::StringPiece printer_name,
                    int* num_options,
                    cups_option_t** options) {
  std::string content;
  if (!base::ReadFileToString(filepath, &content))
    return;

  const char kDest[] = "dest";
  const char kDefault[] = "default";
  const size_t kDestLen = sizeof(kDest) - 1;
  const size_t kDefaultLen = sizeof(kDefault) - 1;

  for (base::StringPiece line : base::SplitStringPiece(
           content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::StartsWith(line, base::StringPiece(kDefault, kDefaultLen),
                         base::CompareCase::INSENSITIVE_ASCII) &&
        isspace(line[kDefaultLen])) {
      line = line.substr(kDefaultLen);
    } else if (base::StartsWith(line, base::StringPiece(kDest, kDestLen),
                                base::CompareCase::INSENSITIVE_ASCII) &&
               isspace(line[kDestLen])) {
      line = line.substr(kDestLen);
    } else {
      continue;
    }

    line = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (line.empty())
      continue;

    size_t space_found = line.find(' ');
    if (space_found == base::StringPiece::npos)
      continue;

    base::StringPiece name = line.substr(0, space_found);
    if (name.empty())
      continue;

    if (!EqualsCaseInsensitiveASCII(printer_name, name))
      continue;  // This is not the required printer.

    line = line.substr(space_found + 1);
    // Remove extra spaces.
    line = base::TrimWhitespaceASCII(line, base::TRIM_ALL);
    if (line.empty())
      continue;

    // Parse the selected printer custom options. Need to pass a
    // null-terminated string.
    *num_options = cupsParseOptions(line.as_string().c_str(), 0, options);
  }
}

void MarkLpOptions(base::StringPiece printer_name, ppd_file_t** ppd) {
  cups_option_t* options = nullptr;
  int num_options = 0;

  const char kSystemLpOptionPath[] = "/etc/cups/lpoptions";
  const char kUserLpOptionPath[] = ".cups/lpoptions";

  std::vector<base::FilePath> file_locations;
  file_locations.push_back(base::FilePath(kSystemLpOptionPath));
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  file_locations.push_back(base::FilePath(homedir.Append(kUserLpOptionPath)));

  for (const base::FilePath& location : file_locations) {
    num_options = 0;
    options = nullptr;
    ParseLpOptions(location, printer_name, &num_options, &options);
    if (num_options > 0 && options) {
      cupsMarkOptions(*ppd, num_options, options);
      cupsFreeOptions(num_options, options);
    }
  }
}

void GetDuplexSettings(ppd_file_t* ppd,
                       std::vector<DuplexMode>* duplex_modes,
                       DuplexMode* duplex_default) {
  ppd_choice_t* duplex_choice = ppdFindMarkedChoice(ppd, kDuplex);
  ppd_option_t* option = ppdFindOption(ppd, kDuplex);
  if (!option)
    option = ppdFindOption(ppd, kBrotherDuplex);

  if (!option)
    return;

  if (!duplex_choice)
    duplex_choice = ppdFindChoice(option, option->defchoice);

  if (ppdFindChoice(option, kDuplexNone))
    duplex_modes->push_back(SIMPLEX);

  if (ppdFindChoice(option, kDuplexNoTumble))
    duplex_modes->push_back(LONG_EDGE);

  if (ppdFindChoice(option, kDuplexTumble))
    duplex_modes->push_back(SHORT_EDGE);

  if (!duplex_choice)
    return;

  const char* choice = duplex_choice->choice;
  if (EqualsCaseInsensitiveASCII(choice, kDuplexNone)) {
    *duplex_default = SIMPLEX;
  } else if (EqualsCaseInsensitiveASCII(choice, kDuplexTumble)) {
    *duplex_default = SHORT_EDGE;
  } else {
    *duplex_default = LONG_EDGE;
  }
}

bool GetBasicColorModelSettings(ppd_file_t* ppd,
                                ColorModel* color_model_for_black,
                                ColorModel* color_model_for_color,
                                bool* color_is_default) {
  ppd_option_t* color_model = ppdFindOption(ppd, kColorModel);
  if (!color_model)
    return false;

  if (ppdFindChoice(color_model, kBlack))
    *color_model_for_black = BLACK;
  else if (ppdFindChoice(color_model, kGray))
    *color_model_for_black = GRAY;
  else if (ppdFindChoice(color_model, kGrayscale))
    *color_model_for_black = GRAYSCALE;

  if (ppdFindChoice(color_model, kColor))
    *color_model_for_color = COLOR;
  else if (ppdFindChoice(color_model, kCMYK))
    *color_model_for_color = CMYK;
  else if (ppdFindChoice(color_model, kRGB))
    *color_model_for_color = RGB;
  else if (ppdFindChoice(color_model, kRGBA))
    *color_model_for_color = RGBA;
  else if (ppdFindChoice(color_model, kRGB16))
    *color_model_for_color = RGB16;
  else if (ppdFindChoice(color_model, kCMY))
    *color_model_for_color = CMY;
  else if (ppdFindChoice(color_model, kKCMY))
    *color_model_for_color = KCMY;
  else if (ppdFindChoice(color_model, kCMY_K))
    *color_model_for_color = CMY_K;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kColorModel);
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
                                  ColorModel* color_model_for_black,
                                  ColorModel* color_model_for_color,
                                  bool* color_is_default) {
  ppd_option_t* printout_mode = ppdFindOption(ppd, kPrintoutMode);
  if (!printout_mode)
    return false;

  *color_model_for_color = PRINTOUTMODE_NORMAL;
  *color_model_for_black = PRINTOUTMODE_NORMAL;

  // Check to see if NORMAL_GRAY value is supported by PrintoutMode.
  // If NORMAL_GRAY is not supported, NORMAL value is used to
  // represent grayscale. If NORMAL_GRAY is supported, NORMAL is used to
  // represent color.
  if (ppdFindChoice(printout_mode, kNormalGray))
    *color_model_for_black = PRINTOUTMODE_NORMAL_GRAY;

  // Get the default marked choice to identify the default color setting
  // value.
  ppd_choice_t* printout_mode_choice = ppdFindMarkedChoice(ppd, kPrintoutMode);
  if (!printout_mode_choice) {
    printout_mode_choice =
        ppdFindChoice(printout_mode, printout_mode->defchoice);
  }
  if (printout_mode_choice) {
    if (EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kNormalGray) ||
        EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kHighGray) ||
        EqualsCaseInsensitiveASCII(printout_mode_choice->choice, kDraftGray)) {
      *color_model_for_black = PRINTOUTMODE_NORMAL_GRAY;
      *color_is_default = false;
    }
  }
  return true;
}

bool GetColorModeSettings(ppd_file_t* ppd,
                          ColorModel* color_model_for_black,
                          ColorModel* color_model_for_color,
                          bool* color_is_default) {
  // Samsung printers use "ColorMode" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kColorMode);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kColor) ||
      ppdFindChoice(color_mode_option, kSamsungColorTrue)) {
    *color_model_for_color = COLORMODE_COLOR;
  }

  if (ppdFindChoice(color_mode_option, kMonochrome) ||
      ppdFindChoice(color_mode_option, kSamsungColorFalse)) {
    *color_model_for_black = COLORMODE_MONOCHROME;
  }

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
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
                             ColorModel* color_model_for_black,
                             ColorModel* color_model_for_color,
                             bool* color_is_default) {
  // Some Brother printers use "BRMonoColor" attribute in their PPDs.
  // Some Brother printers use "BRPrintQuality" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kBrotherMonoColor);
  if (!color_mode_option)
    color_mode_option = ppdFindOption(ppd, kBrotherPrintQuality);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kFullColor))
    *color_model_for_color = BROTHER_CUPS_COLOR;
  else if (ppdFindChoice(color_mode_option, kColor))
    *color_model_for_color = BROTHER_BRSCRIPT3_COLOR;

  if (ppdFindChoice(color_mode_option, kMono))
    *color_model_for_black = BROTHER_CUPS_MONO;
  else if (ppdFindChoice(color_mode_option, kBlack))
    *color_model_for_black = BROTHER_BRSCRIPT3_BLACK;

  ppd_choice_t* marked_choice = ppdFindMarkedChoice(ppd, kColorMode);
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
                        ColorModel* color_model_for_black,
                        ColorModel* color_model_for_color,
                        bool* color_is_default) {
  // HP printers use "Color/Color Model" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kColor);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kColor))
    *color_model_for_color = HP_COLOR_COLOR;
  if (ppdFindChoice(color_mode_option, kBlack))
    *color_model_for_black = HP_COLOR_BLACK;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kColorMode);
  if (!mode_choice) {
    mode_choice =
        ppdFindChoice(color_mode_option, color_mode_option->defchoice);
  }
  if (mode_choice) {
    *color_is_default = EqualsCaseInsensitiveASCII(mode_choice->choice, kColor);
  }
  return true;
}

bool GetProcessColorModelSettings(ppd_file_t* ppd,
                                  ColorModel* color_model_for_black,
                                  ColorModel* color_model_for_color,
                                  bool* color_is_default) {
  // Canon printers use "ProcessColorModel" attribute in their PPDs.
  ppd_option_t* color_mode_option = ppdFindOption(ppd, kProcessColorModel);
  if (!color_mode_option)
    return false;

  if (ppdFindChoice(color_mode_option, kRGB))
    *color_model_for_color = PROCESSCOLORMODEL_RGB;
  else if (ppdFindChoice(color_mode_option, kCMYK))
    *color_model_for_color = PROCESSCOLORMODEL_CMYK;

  if (ppdFindChoice(color_mode_option, kGreyscale))
    *color_model_for_black = PROCESSCOLORMODEL_GREYSCALE;

  ppd_choice_t* mode_choice = ppdFindMarkedChoice(ppd, kProcessColorModel);
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
                           ColorModel* cm_black,
                           ColorModel* cm_color,
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
         GetBrotherColorSettings(ppd, cm_black, cm_color, is_color) ||
         GetProcessColorModelSettings(ppd, cm_black, cm_color, is_color);
}

// Default port for IPP print servers.
const int kDefaultIPPServerPort = 631;

}  // namespace

// Helper wrapper around http_t structure, with connection and cleanup
// functionality.
HttpConnectionCUPS::HttpConnectionCUPS(const GURL& print_server_url,
                                       http_encryption_t encryption)
    : http_(nullptr) {
  // If we have an empty url, use default print server.
  if (print_server_url.is_empty())
    return;

  int port = print_server_url.IntPort();
  if (port == url::PORT_UNSPECIFIED)
    port = kDefaultIPPServerPort;

  http_ = httpConnectEncrypt(print_server_url.host().c_str(), port, encryption);
  if (!http_) {
    LOG(ERROR) << "CP_CUPS: Failed connecting to print server: "
               << print_server_url;
  }
}

HttpConnectionCUPS::~HttpConnectionCUPS() {
  if (http_)
    httpClose(http_);
}

void HttpConnectionCUPS::SetBlocking(bool blocking) {
  httpBlocking(http_, blocking ? 1 : 0);
}

http_t* HttpConnectionCUPS::http() {
  return http_;
}

bool ParsePpdCapabilities(base::StringPiece printer_name,
                          base::StringPiece printer_capabilities,
                          PrinterSemanticCapsAndDefaults* printer_info) {
  base::FilePath ppd_file_path;
  if (!base::CreateTemporaryFile(&ppd_file_path))
    return false;

  int data_size = printer_capabilities.length();
  if (data_size !=
      base::WriteFile(ppd_file_path, printer_capabilities.data(), data_size)) {
    base::DeleteFile(ppd_file_path, false);
    return false;
  }

  ppd_file_t* ppd = ppdOpenFile(ppd_file_path.value().c_str());
  if (!ppd) {
    int line = 0;
    ppd_status_t ppd_status = ppdLastError(&line);
    LOG(ERROR) << "Failed to open PDD file: error " << ppd_status << " at line "
               << line << ", " << ppdErrorString(ppd_status);
    return false;
  }
  ppdMarkDefaults(ppd);
  MarkLpOptions(printer_name, &ppd);

  PrinterSemanticCapsAndDefaults caps;
  caps.collate_capable = true;
  caps.collate_default = true;
  caps.copies_capable = true;

  GetDuplexSettings(ppd, &caps.duplex_modes, &caps.duplex_default);

  bool is_color = false;
  ColorModel cm_color = UNKNOWN_COLOR_MODEL, cm_black = UNKNOWN_COLOR_MODEL;
  if (!GetColorModelSettings(ppd, &cm_black, &cm_color, &is_color)) {
    VLOG(1) << "Unknown printer color model";
  }

  caps.color_changeable =
      ((cm_color != UNKNOWN_COLOR_MODEL) && (cm_black != UNKNOWN_COLOR_MODEL) &&
       (cm_color != cm_black));
  caps.color_default = is_color;
  caps.color_model = cm_color;
  caps.bw_model = cm_black;

  if (ppd->num_sizes > 0 && ppd->sizes) {
    VLOG(1) << "Paper list size - " << ppd->num_sizes;
    ppd_option_t* paper_option = ppdFindOption(ppd, kPageSize);
    for (int i = 0; i < ppd->num_sizes; ++i) {
      gfx::Size paper_size_microns(
          ConvertUnit(ppd->sizes[i].width, kPointsPerInch, kMicronsPerInch),
          ConvertUnit(ppd->sizes[i].length, kPointsPerInch, kMicronsPerInch));
      if (paper_size_microns.width() > 0 && paper_size_microns.height() > 0) {
        PrinterSemanticCapsAndDefaults::Paper paper;
        paper.size_um = paper_size_microns;
        paper.vendor_id = ppd->sizes[i].name;
        if (paper_option) {
          ppd_choice_t* paper_choice =
              ppdFindChoice(paper_option, ppd->sizes[i].name);
          // Human readable paper name should be UTF-8 encoded, but some PPDs
          // do not follow this standard.
          if (paper_choice && base::IsStringUTF8(paper_choice->text)) {
            paper.display_name = paper_choice->text;
          }
        }
        caps.papers.push_back(paper);
        if (i == 0 || ppd->sizes[i].marked) {
          caps.default_paper = paper;
        }
      }
    }
  }

  ppdClose(ppd);
  base::DeleteFile(ppd_file_path, false);

  *printer_info = caps;
  return true;
}

}  // namespace printing
