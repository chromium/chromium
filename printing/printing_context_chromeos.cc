// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_chromeos.h"

#include <cups/cups.h>
#include <stdint.h>
#include <unicode/ulocdata.h>

#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_ipp_helper.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/buildflags/buildflags.h"
#include "printing/client_info_helpers.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/printing_utils.h"
#include "printing/units.h"

namespace printing {

namespace {

// We only support sending username for secure printers.
const char kUsernamePlaceholder[] = "chronos";

// We only support sending document name for secure printers.
const char kDocumentNamePlaceholder[] = "-";

bool IsUriSecure(std::string_view uri) {
  return base::StartsWith(uri, "ipps:") || base::StartsWith(uri, "https:") ||
         base::StartsWith(uri, "usb:") || base::StartsWith(uri, "ippusb:");
}

// Populates the 'client-info' attribute of the IPP collection `options`. Each
// item in `client_infos` represents one collection in 'client-info'.
// Invalid 'client-info' items will be dropped.
void EncodeClientInfo(const std::vector<mojom::IppClientInfo>& client_infos,
                      ipp_t* options) {
  std::vector<ScopedIppPtr> option_values;
  std::vector<const ipp_t*> raw_option_values;
  option_values.reserve(client_infos.size());
  raw_option_values.reserve(client_infos.size());

  for (const mojom::IppClientInfo& client_info : client_infos) {
    if (!ValidateClientInfoItem(client_info)) {
      LOG(WARNING) << "Invalid client-info item skipped";
      continue;
    }

    // Create a temporary collection object owned by this function.
    ipp_t* collection = ippNew();
    option_values.emplace_back(WrapIpp(collection));
    raw_option_values.emplace_back(collection);

    ippAddString(collection, IPP_TAG_ZERO, IPP_TAG_NAME, kIppClientName,
                 nullptr, client_info.client_name.c_str());
    ippAddInteger(collection, IPP_TAG_ZERO, IPP_TAG_ENUM, kIppClientType,
                  static_cast<int>(client_info.client_type));
    ippAddString(collection, IPP_TAG_ZERO, IPP_TAG_TEXT,
                 kIppClientStringVersion, nullptr,
                 client_info.client_string_version.c_str());

    if (client_info.client_version.has_value()) {
      ippAddOctetString(collection, IPP_TAG_ZERO, kIppClientVersion,
                        client_info.client_version.value().data(),
                        client_info.client_version.value().size());
    }

    if (client_info.client_patches.has_value()) {
      ippAddString(collection, IPP_TAG_ZERO, IPP_TAG_TEXT, kIppClientPatches,
                   nullptr, client_info.client_patches.value().c_str());
    }
  }

  if (raw_option_values.empty()) {
    return;
  }

  // Now add the client-info list to the options.
  ippAddCollections(options, IPP_TAG_OPERATION, kIppClientInfo,
                    raw_option_values.size(), raw_option_values.data());
}

// Construct the IPP media-col attribute specifying media size, margins, source,
// etc., and add it to 'options'.
void EncodeMediaCol(ipp_t* options,
                    const gfx::Size& size_um,
                    const gfx::Rect& printable_area_um,
                    bool borderless,
                    const std::string& source,
                    const std::string& type) {
  // The size and printable area in microns were calculated from the size and
  // margins in PWG units, so we can losslessly convert them back. If
  // borderless printing was requested, though, set all margins to zero.
  DCHECK_EQ(size_um.width() % kMicronsPerPwgUnit, 0);
  DCHECK_EQ(size_um.height() % kMicronsPerPwgUnit, 0);
  int width = size_um.width() / kMicronsPerPwgUnit;
  int height = size_um.height() / kMicronsPerPwgUnit;
  int bottom_margin = 0, left_margin = 0, right_margin = 0, top_margin = 0;
  if (!borderless) {
    PwgMarginsFromSizeAndPrintableArea(size_um, printable_area_um,
                                       &bottom_margin, &left_margin,
                                       &right_margin, &top_margin);
  }

  ScopedIppPtr media_col = WrapIpp(ippNew());
  ScopedIppPtr media_size = WrapIpp(ippNew());
  ippAddInteger(media_size.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER, kIppXDimension,
                width);
  ippAddInteger(media_size.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER, kIppYDimension,
                height);
  ippAddCollection(media_col.get(), IPP_TAG_ZERO, kIppMediaSize,
                   media_size.get());
  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                kIppMediaBottomMargin, bottom_margin);
  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                kIppMediaLeftMargin, left_margin);
  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                kIppMediaRightMargin, right_margin);
  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                kIppMediaTopMargin, top_margin);
  if (!source.empty()) {
    ippAddString(media_col.get(), IPP_TAG_ZERO, IPP_TAG_KEYWORD,
                 kIppMediaSource, nullptr, source.c_str());
  }
  if (!type.empty()) {
    ippAddString(media_col.get(), IPP_TAG_ZERO, IPP_TAG_KEYWORD, kIppMediaType,
                 nullptr, type.c_str());
  }

  ippAddCollection(options, IPP_TAG_JOB, kIppMediaCol, media_col.get());
}

std::string GetCollateString(bool collate) {
  return collate ? kCollated : kUncollated;
}

void SetPrintableArea(PrintSettings* settings,
                      const PrintSettings::RequestedMedia& media,
                      const gfx::Rect& printable_area_um) {
  if (!media.size_microns.IsEmpty()) {
    float device_microns_per_device_unit =
        static_cast<float>(kMicronsPerInch) / settings->device_units_per_inch();
    gfx::Size paper_size =
        gfx::Size(media.size_microns.width() / device_microns_per_device_unit,
                  media.size_microns.height() / device_microns_per_device_unit);

    gfx::Rect paper_rect =
        gfx::Rect(printable_area_um.x() / device_microns_per_device_unit,
                  printable_area_um.y() / device_microns_per_device_unit,
                  printable_area_um.width() / device_microns_per_device_unit,
                  printable_area_um.height() / device_microns_per_device_unit);
    settings->SetPrinterPrintableArea(paper_size, paper_rect,
                                      /*landscape_needs_flip=*/true);
  }
}

}  // namespace

ScopedIppPtr SettingsToIPPOptions(const PrintSettings& settings,
                                  gfx::Rect printable_area_um) {
  ScopedIppPtr scoped_options = WrapIpp(ippNew());
  ipp_t* options = scoped_options.get();

  // The media width/height may have been swapped to ensure the media is
  // portrait (height greater than width).  When sending the IPP attributes to
  // CUPS, the media needs to be in the original format.  The way to determine
  // if the media size was swapped is to look at the vendor ID (which does not
  // get altered).  If its width is greater than its height, that means the
  // media size was swapped and needs to be swapped back when creating the IPP
  // attributes.
  gfx::Size media_size_microns = settings.requested_media().size_microns;
  const gfx::Size vendor_id_paper_size =
      ParsePaperSize(settings.requested_media().vendor_id);
  if (!vendor_id_paper_size.IsEmpty() &&
      vendor_id_paper_size.width() > vendor_id_paper_size.height()) {
    // Rotate 90 degrees counter-clockwise to undo the rotation in
    // cloud_print_cdd_conversion.cc.
    int new_x = media_size_microns.height() - printable_area_um.height() -
                printable_area_um.y();
    int new_y = printable_area_um.x();

    printable_area_um.SetRect(new_x, new_y, printable_area_um.height(),
                              printable_area_um.width());
    media_size_microns.SetSize(media_size_microns.height(),
                               media_size_microns.width());
  }

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

  // duplexing
  ippAddString(options, IPP_TAG_JOB, IPP_TAG_KEYWORD, kIppDuplex, nullptr,
               sides);
  // color
  ippAddString(options, IPP_TAG_JOB, IPP_TAG_KEYWORD, kIppColor, nullptr,
               GetIppColorModelForModel(settings.color()).c_str());
  // copies
  ippAddInteger(options, IPP_TAG_JOB, IPP_TAG_INTEGER, kIppCopies,
                settings.copies());
  // collate
  ippAddString(options, IPP_TAG_JOB, IPP_TAG_KEYWORD, kIppCollate, nullptr,
               GetCollateString(settings.collate()).c_str());

  if (!settings.pin_value().empty()) {
    ippAddOctetString(options, IPP_TAG_OPERATION, kIppPin,
                      settings.pin_value().data(), settings.pin_value().size());
    ippAddString(options, IPP_TAG_OPERATION, IPP_TAG_KEYWORD, kIppPinEncryption,
                 nullptr, kPinEncryptionNone);
  }

  // resolution
  if (settings.dpi_horizontal() > 0 && settings.dpi_vertical() > 0) {
    ippAddResolution(options, IPP_TAG_JOB, kIppResolution, IPP_RES_PER_INCH,
                     settings.dpi_horizontal(), settings.dpi_vertical());
  }

  std::map<std::string, std::vector<int>> multival;
  std::string media_source;
  for (const auto& setting : settings.advanced_settings()) {
    const std::string& key = setting.first;
    const std::string& value = setting.second.GetString();
    if (value.empty()) {
      continue;
    }
    if (key == kIppMediaSource) {
      media_source = value;
      continue;
    }

    // Check for multivalue enum ("attribute/value").
    size_t pos = key.find('/');
    if (pos == std::string::npos) {
      // Regular value.
      ippAddString(options, IPP_TAG_JOB, IPP_TAG_KEYWORD, key.c_str(), nullptr,
                   value.c_str());
      continue;
    }
    // Store selected enum values.
    if (value == kOptionTrue) {
      std::string option_name = key.substr(0, pos);
      std::string enum_string = key.substr(pos + 1);
      int enum_value = ippEnumValue(option_name.c_str(), enum_string.c_str());
      DCHECK_NE(enum_value, -1);
      multival[option_name].push_back(enum_value);
    }
  }

  // Construct the IPP media-col attribute specifying media size, margins,
  // source, etc.
  EncodeMediaCol(options, media_size_microns, printable_area_um,
                 settings.borderless(), media_source, settings.media_type());

  // Add multivalue enum options.
  for (const auto& it : multival) {
    ippAddIntegers(options, IPP_TAG_JOB, IPP_TAG_ENUM, it.first.c_str(),
                   it.second.size(), it.second.data());
  }

  // OAuth access token
  if (!settings.oauth_token().empty()) {
    ippAddString(options, IPP_TAG_JOB, IPP_TAG_NAME,
                 kSettingChromeOSAccessOAuthToken, nullptr,
                 settings.oauth_token().c_str());
  }

  // IPP client-info attribute.
  if (!settings.client_infos().empty()) {
    EncodeClientInfo(settings.client_infos(), options);
  }

  return scoped_options;
}

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    ProcessBehavior process_behavior) {
  return std::make_unique<PrintingContextChromeos>(delegate, process_behavior);
}

// static
std::unique_ptr<PrintingContextChromeos>
PrintingContextChromeos::CreateForTesting(
    Delegate* delegate,
    ProcessBehavior process_behavior,
    std::unique_ptr<CupsConnection> connection) {
  // Private ctor.
  return base::WrapUnique(new PrintingContextChromeos(
      delegate, process_behavior, std::move(connection)));
}

PrintingContextChromeos::PrintingContextChromeos(
    Delegate* delegate,
    ProcessBehavior process_behavior)
    : PrintingContext(delegate, process_behavior),
      connection_(CupsConnection::Create()),
      ipp_options_(WrapIpp(nullptr)) {}

PrintingContextChromeos::PrintingContextChromeos(
    Delegate* delegate,
    ProcessBehavior process_behavior,
    std::unique_ptr<CupsConnection> connection)
    : PrintingContext(delegate, process_behavior),
      connection_(std::move(connection)),
      ipp_options_(WrapIpp(nullptr)) {}

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

mojom::ResultCode PrintingContextChromeos::UseDefaultSettings() {
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
  if (InitializeDevice(device_name) != mojom::ResultCode::kSuccess) {
    LOG(ERROR) << "Could not initialize printer";
    return OnError();
  }

  // Set printable area
  DCHECK(printer_);
  PrinterSemanticCapsAndDefaults::Paper paper = DefaultPaper(*printer_);

  PrintSettings::RequestedMedia media;
  media.vendor_id = paper.vendor_id();
  media.size_microns = paper.size_um();
  settings_->set_requested_media(media);
  SetPrintableArea(settings_.get(), media, paper.printable_area_um());

  return mojom::ResultCode::kSuccess;
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

mojom::ResultCode PrintingContextChromeos::UpdatePrinterSettings(
    const PrinterSettings& printer_settings) {
  DCHECK(!printer_settings.show_system_dialog);

  if (InitializeDevice(base::UTF16ToUTF8(settings_->device_name())) !=
      mojom::ResultCode::kSuccess) {
    return OnError();
  }

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

    media.vendor_id = paper.vendor_id();
    media.size_microns = paper.size_um();
    settings_->set_requested_media(media);
  }

  gfx::Rect printable_area_um =
      GetPrintableAreaForSize(*printer_, media.size_microns);
  SetPrintableArea(settings_.get(), media, printable_area_um);
  ipp_options_ = SettingsToIPPOptions(*settings_, std::move(printable_area_um));
  send_user_info_ = settings_->send_user_info();
  if (send_user_info_) {
    DCHECK(printer_);
    username_ = IsUriSecure(printer_->GetUri()) ? settings_->username()
                                                : kUsernamePlaceholder;
  }

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextChromeos::InitializeDevice(
    const std::string& device) {
  DCHECK(!in_print_job_);

  std::unique_ptr<CupsPrinter> printer = connection_->GetPrinter(device);
  if (!printer) {
    LOG(WARNING) << "Could not initialize device";
    return OnError();
  }

  printer_ = std::move(printer);

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextChromeos::NewDocument(
    const std::u16string& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (process_behavior() == ProcessBehavior::kOopEnabledSkipSystemCalls) {
    return mojom::ResultCode::kSuccess;
  }
#endif

  std::string converted_name;
  if (send_user_info_) {
    DCHECK(printer_);
    converted_name = IsUriSecure(printer_->GetUri())
                         ? base::UTF16ToUTF8(document_name)
                         : kDocumentNamePlaceholder;
  }

  ipp_status_t create_status = printer_->CreateJob(
      &job_id_, converted_name, username_, ipp_options_.get());

  if (job_id_ == 0) {
    DLOG(WARNING) << "Creating cups job failed"
                  << ippErrorString(create_status);
    return OnError();
  }

  // we only send one document, so it's always the last one
  if (!printer_->StartDocument(job_id_, converted_name, true, username_,
                               ipp_options_.get())) {
    LOG(ERROR) << "Starting document failed";
    return OnError();
  }

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextChromeos::PrintDocument(
    const MetafilePlayer& metafile,
    const PrintSettings& settings,
    uint32_t num_pages) {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);

#if BUILDFLAG(USE_CUPS)
  std::vector<char> buffer;
  if (!metafile.GetDataAsVector(&buffer))
    return mojom::ResultCode::kFailed;

  return StreamData(buffer);
#else
  NOTREACHED();
#endif  // BUILDFLAG(USE_CUPS)
}

mojom::ResultCode PrintingContextChromeos::DocumentDone() {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;

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
  return mojom::ResultCode::kSuccess;
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

mojom::ResultCode PrintingContextChromeos::StreamData(
    const std::vector<char>& buffer) {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;

  DCHECK(in_print_job_);
  DCHECK(printer_);

  if (!printer_->StreamData(buffer))
    return OnError();

  return mojom::ResultCode::kSuccess;
}

}  // namespace printing
