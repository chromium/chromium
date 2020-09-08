// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_chromeos.h"

#include <cups/cups.h>
#include <stdint.h>
#include <unicode/ulocdata.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_ipp_helper.h"
#include "printing/backend/cups_printer.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/printing_features.h"
#include "printing/units.h"

namespace printing {

namespace {

// Returns a new char buffer which is a null-terminated copy of |value|.  The
// caller owns the returned string.
char* DuplicateString(const base::StringPiece value) {
  char* dst = new char[value.size() + 1];
  value.copy(dst, value.size());
  dst[value.size()] = '\0';
  return dst;
}

ScopedCupsOption ConstructOption(const base::StringPiece name,
                                 const base::StringPiece value) {
  // ScopedCupsOption frees the name and value buffers on deletion
  ScopedCupsOption option = ScopedCupsOption(new cups_option_t);
  option->name = DuplicateString(name);
  option->value = DuplicateString(value);
  return option;
}

base::StringPiece GetCollateString(bool collate) {
  return collate ? kCollated : kUncollated;
}

// This enum is used for UMA. It shouldn't be renumbered and numeric values
// shouldn't be reused.
enum class Attribute {
  kConfirmationSheetPrint = 0,
  kFinishings = 1,
  kIppAttributeFidelity = 2,
  kJobName = 3,
  kJobPriority = 4,
  kJobSheets = 5,
  kMultipleDocumentHandling = 6,
  kOrientationRequested = 7,
  kOutputBin = 8,
  kPrintQuality = 9,
  kMaxValue = kPrintQuality,
};

using AttributeMap = std::map<base::StringPiece, Attribute>;

AttributeMap GenerateAttributeMap() {
  AttributeMap result;
  result.emplace("confirmation-sheet-print",
                 Attribute::kConfirmationSheetPrint);
  result.emplace("finishings", Attribute::kFinishings);
  result.emplace("ipp-attribute-fidelity", Attribute::kIppAttributeFidelity);
  result.emplace("job-name", Attribute::kJobName);
  result.emplace("job-priority", Attribute::kJobPriority);
  result.emplace("job-sheets", Attribute::kJobSheets);
  result.emplace("multiple-document-handling",
                 Attribute::kMultipleDocumentHandling);
  result.emplace("orientation-requested", Attribute::kOrientationRequested);
  result.emplace("output-bin", Attribute::kOutputBin);
  result.emplace("print-quality", Attribute::kPrintQuality);
  return result;
}

void ReportEnumUsage(const std::string& attribute_name) {
  static const base::NoDestructor<AttributeMap> attributes(
      GenerateAttributeMap());
  auto it = attributes->find(attribute_name);
  if (it == attributes->end())
    return;

  base::UmaHistogramEnumeration("Printing.CUPS.IppAttributes", it->second);
}

// Given an integral |value| expressed in PWG units (1/100 mm), returns
// the same value expressed in device units.
int PwgUnitsToDeviceUnits(int value, float micrometers_per_device_unit) {
  return ConvertUnitDouble(value, micrometers_per_device_unit, 10);
}

// Given a |media_size|, the specification of the media's |margins|, and
// the number of micrometers per device unit, returns the rectangle
// bounding the apparent printable area of said media.
gfx::Rect RepresentPrintableArea(const gfx::Size& media_size,
                                 const CupsPrinter::CupsMediaMargins& margins,
                                 float micrometers_per_device_unit) {
  // These values express inward encroachment by margins, away from the
  // edges of the |media_size|.
  int left_bound =
      PwgUnitsToDeviceUnits(margins.left, micrometers_per_device_unit);
  int bottom_bound =
      PwgUnitsToDeviceUnits(margins.bottom, micrometers_per_device_unit);
  int right_bound =
      PwgUnitsToDeviceUnits(margins.right, micrometers_per_device_unit);
  int top_bound =
      PwgUnitsToDeviceUnits(margins.top, micrometers_per_device_unit);

  // These values express the bounding box of the printable area on the
  // page.
  int printable_width = media_size.width() - (left_bound + right_bound);
  int printable_height = media_size.height() - (top_bound + bottom_bound);

  if (printable_width > 0 && printable_height > 0) {
    return {left_bound, bottom_bound, printable_width, printable_height};
  }

  return {0, 0, media_size.width(), media_size.height()};
}

void SetPrintableArea(PrintSettings* settings,
                      const PrintSettings::RequestedMedia& media,
                      const CupsPrinter::CupsMediaMargins& margins,
                      bool flip) {
  if (!media.size_microns.IsEmpty()) {
    float device_microns_per_device_unit =
        static_cast<float>(kMicronsPerInch) / settings->device_units_per_inch();
    gfx::Size paper_size =
        gfx::Size(media.size_microns.width() / device_microns_per_device_unit,
                  media.size_microns.height() / device_microns_per_device_unit);

    gfx::Rect paper_rect = RepresentPrintableArea(
        paper_size, margins, device_microns_per_device_unit);
    settings->SetPrinterPrintableArea(paper_size, paper_rect, flip);
  }
}

}  // namespace

std::vector<ScopedCupsOption> SettingsToCupsOptions(
    const PrintSettings& settings) {
  const char* sides = nullptr;
  switch (settings.duplex_mode()) {
    case mojom::DuplexMode::kSimplex:
      sides = CUPS_SIDES_ONE_SIDED;
      break;
    case mojom::DuplexMode::kLongEdge:
      sides = CUPS_SIDES_TWO_SIDED_PORTRAIT;
      break;
    case mojom::DuplexMode::kShortEdge:
      sides = CUPS_SIDES_TWO_SIDED_LANDSCAPE;
      break;
    default:
      NOTREACHED();
  }

  std::vector<ScopedCupsOption> options;
  options.push_back(
      ConstructOption(kIppColor,
                      GetIppColorModelForModel(settings.color())));  // color
  options.push_back(ConstructOption(kIppDuplex, sides));         // duplexing
  options.push_back(
      ConstructOption(kIppMedia,
                      settings.requested_media().vendor_id));  // paper size
  options.push_back(
      ConstructOption(kIppCopies,
                      base::NumberToString(settings.copies())));  // copies
  options.push_back(
      ConstructOption(kIppCollate,
                      GetCollateString(settings.collate())));  // collate
  if (!settings.pin_value().empty()) {
    options.push_back(ConstructOption(kIppPin, settings.pin_value()));
    options.push_back(ConstructOption(kIppPinEncryption, kPinEncryptionNone));
  }

  if (settings.dpi_horizontal() > 0 && settings.dpi_vertical() > 0) {
    std::string dpi = base::NumberToString(settings.dpi_horizontal());
    if (settings.dpi_horizontal() != settings.dpi_vertical())
      dpi += "x" + base::NumberToString(settings.dpi_vertical());
    options.push_back(ConstructOption(kIppResolution, dpi + "dpi"));
  }

  if (base::FeatureList::IsEnabled(
          printing::features::kAdvancedPpdAttributes)) {
    size_t regular_attr_count = options.size();
    std::map<std::string, std::vector<std::string>> multival;
    for (const auto& setting : settings.advanced_settings()) {
      const std::string& key = setting.first;
      const std::string& value = setting.second.GetString();
      if (value.empty())
        continue;

      // Check for multivalue enum ("attribute/value").
      size_t pos = key.find('/');
      if (pos == std::string::npos) {
        // Regular value.
        ReportEnumUsage(key);
        options.push_back(ConstructOption(key, value));
        continue;
      }
      // Store selected enum values.
      if (value == kOptionTrue)
        multival[key.substr(0, pos)].push_back(key.substr(pos + 1));
    }
    // Pass multivalue enums as comma-separated lists.
    for (const auto& it : multival) {
      ReportEnumUsage(it.first);
      options.push_back(
          ConstructOption(it.first, base::JoinString(it.second, ",")));
    }
    base::UmaHistogramCounts1000("Printing.CUPS.IppAttributesUsed",
                                 options.size() - regular_attr_count);
  }

  return options;
}

// static
std::unique_ptr<PrintingContext> PrintingContext::Create(Delegate* delegate) {
  return std::make_unique<PrintingContextChromeos>(delegate);
}

PrintingContextChromeos::PrintingContextChromeos(Delegate* delegate)
    : PrintingContext(delegate),
      connection_(GURL(), HTTP_ENCRYPT_NEVER, true),
      send_user_info_(false) {}

PrintingContextChromeos::~PrintingContextChromeos() {
  ReleaseContext();
}

void PrintingContextChromeos::AskUserForSettings(
    int max_pages,
    bool has_selection,
    bool is_scripted,
    PrintSettingsCallback callback) {
  // We don't want to bring up a dialog here.  Ever.  This should not be called.
  NOTREACHED();
}

PrintingContext::Result PrintingContextChromeos::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();

  std::string device_name = base::UTF16ToUTF8(settings_->device_name());
  if (device_name.empty())
    return OnError();

  // TODO(skau): https://crbug.com/613779. See UpdatePrinterSettings for more
  // info.
  if (settings_->dpi() == 0) {
    DVLOG(1) << "Using Default DPI";
    settings_->set_dpi(kDefaultPdfDpi);
  }

  // Retrieve device information and set it
  if (InitializeDevice(device_name) != OK) {
    LOG(ERROR) << "Could not initialize printer";
    return OnError();
  }

  // Set printable area
  DCHECK(printer_);
  PrinterSemanticCapsAndDefaults::Paper paper = DefaultPaper(*printer_);

  PrintSettings::RequestedMedia media;
  media.vendor_id = paper.vendor_id;
  media.size_microns = paper.size_um;
  settings_->set_requested_media(media);

  CupsPrinter::CupsMediaMargins margins =
      printer_->GetMediaMarginsByName(paper.vendor_id);
  SetPrintableArea(settings_.get(), media, margins, true /* flip landscape */);

  return OK;
}

gfx::Size PrintingContextChromeos::GetPdfPaperSizeDeviceUnits() {
  int32_t width = 0;
  int32_t height = 0;
  UErrorCode error = U_ZERO_ERROR;
  ulocdata_getPaperSize(delegate_->GetAppLocale().c_str(), &height, &width,
                        &error);
  if (error > U_ZERO_ERROR) {
    // If the call failed, assume a paper size of 8.5 x 11 inches.
    LOG(WARNING) << "ulocdata_getPaperSize failed, using 8.5 x 11, error: "
                 << error;
    width =
        static_cast<int>(kLetterWidthInch * settings_->device_units_per_inch());
    height = static_cast<int>(kLetterHeightInch *
                              settings_->device_units_per_inch());
  } else {
    // ulocdata_getPaperSize returns the width and height in mm.
    // Convert this to pixels based on the dpi.
    float multiplier = settings_->device_units_per_inch() / kMicronsPerMil;
    width *= multiplier;
    height *= multiplier;
  }
  return gfx::Size(width, height);
}

PrintingContext::Result PrintingContextChromeos::UpdatePrinterSettings(
    bool external_preview,
    bool show_system_dialog,
    int page_count) {
  DCHECK(!show_system_dialog);

  if (InitializeDevice(base::UTF16ToUTF8(settings_->device_name())) != OK)
    return OnError();

  // TODO(skau): Convert to DCHECK when https://crbug.com/613779 is resolved
  // Print quality suffers when this is set to the resolution reported by the
  // printer but print quality is fine at this resolution. UseDefaultSettings
  // exhibits the same problem.
  if (settings_->dpi() == 0) {
    DVLOG(1) << "Using Default DPI";
    settings_->set_dpi(kDefaultPdfDpi);
  }

  // compute paper size
  PrintSettings::RequestedMedia media = settings_->requested_media();

  DCHECK(printer_);
  if (media.IsDefault()) {
    PrinterSemanticCapsAndDefaults::Paper paper = DefaultPaper(*printer_);

    media.vendor_id = paper.vendor_id;
    media.size_microns = paper.size_um;
    settings_->set_requested_media(media);
  }

  CupsPrinter::CupsMediaMargins margins =
      printer_->GetMediaMarginsByName(media.vendor_id);
  SetPrintableArea(settings_.get(), media, margins, true);
  cups_options_ = SettingsToCupsOptions(*settings_);
  send_user_info_ = settings_->send_user_info();
  if (send_user_info_) {
    DCHECK(printer_);
    std::string uri_string = printer_->GetUri();
    const base::StringPiece uri(uri_string);
    if (!base::StartsWith(uri, "ipps:") && !base::StartsWith(uri, "https:") &&
        !base::StartsWith(uri, "usb:") && !base::StartsWith(uri, "ippusb:")) {
      return OnError();
    }
  }
  username_ = send_user_info_ ? settings_->username() : std::string();

  return OK;
}

PrintingContext::Result PrintingContextChromeos::InitializeDevice(
    const std::string& device) {
  DCHECK(!in_print_job_);

  std::unique_ptr<CupsPrinter> printer = connection_.GetPrinter(device);
  if (!printer) {
    LOG(WARNING) << "Could not initialize device";
    return OnError();
  }

  printer_ = std::move(printer);

  return OK;
}

PrintingContext::Result PrintingContextChromeos::NewDocument(
    const base::string16& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

  std::string converted_name;
  if (send_user_info_)
    converted_name = base::UTF16ToUTF8(document_name);

  std::vector<cups_option_t> options;
  for (const ScopedCupsOption& option : cups_options_) {
    if (printer_->CheckOptionSupported(option->name, option->value)) {
      options.push_back(*(option.get()));
    } else {
      DVLOG(1) << "Unsupported option skipped " << option->name << ", "
               << option->value;
    }
  }

  ipp_status_t create_status =
      printer_->CreateJob(&job_id_, converted_name, username_, options);

  if (job_id_ == 0) {
    DLOG(WARNING) << "Creating cups job failed"
                  << ippErrorString(create_status);
    return OnError();
  }

  // we only send one document, so it's always the last one
  if (!printer_->StartDocument(job_id_, converted_name, true, username_,
                               options)) {
    LOG(ERROR) << "Starting document failed";
    return OnError();
  }

  return OK;
}

PrintingContext::Result PrintingContextChromeos::NewPage() {
  if (abort_printing_)
    return CANCEL;

  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextChromeos::PageDone() {
  if (abort_printing_)
    return CANCEL;

  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextChromeos::DocumentDone() {
  if (abort_printing_)
    return CANCEL;

  DCHECK(in_print_job_);

  if (!printer_->FinishDocument()) {
    LOG(WARNING) << "Finishing document failed";
    return OnError();
  }

  ipp_status_t job_status = printer_->CloseJob(job_id_, username_);
  job_id_ = 0;

  if (job_status != IPP_STATUS_OK) {
    LOG(WARNING) << "Closing job failed";
    return OnError();
  }

  ResetSettings();
  return OK;
}

void PrintingContextChromeos::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
}

void PrintingContextChromeos::ReleaseContext() {
  printer_.reset();
}

printing::NativeDrawingContext PrintingContextChromeos::context() const {
  // Intentional No-op.
  return nullptr;
}

PrintingContext::Result PrintingContextChromeos::StreamData(
    const std::vector<char>& buffer) {
  if (abort_printing_)
    return CANCEL;

  DCHECK(in_print_job_);
  DCHECK(printer_);

  if (!printer_->StreamData(buffer))
    return OnError();

  return OK;
}

}  // namespace printing
