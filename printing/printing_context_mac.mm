// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_mac.h"

#import <AppKit/AppKit.h>
#include <CoreFoundation/CoreFoundation.h>
#import <QuartzCore/QuartzCore.h>
#include <cups/cups.h>

#import <iomanip>
#import <numeric>
#include <string_view>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_typeref.h"
#include "base/check_op.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_job_constants_cups.h"
#include "printing/print_settings_initializer_mac.h"
#include "printing/printing_features.h"
#include "printing/units.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#endif

namespace printing {

namespace {

template <typename T>
struct ScopedPMTypeTraits {
  static T InvalidValue() { return nullptr; }
  static T Retain(T object) {
    PMRetain(object);
    return object;
  }
  static void Release(T object) { PMRelease(object); }
};

template <typename T>
using ScopedPMType = base::apple::ScopedTypeRef<T, ScopedPMTypeTraits<T>>;

const int kMaxPaperSizeDifferenceInPoints = 2;

// Return true if PPD name of paper is equal.
bool IsPaperNameEqual(CFStringRef name1, const PMPaper& paper2) {
  CFStringRef name2 = nullptr;
  return (name1 && PMPaperGetPPDPaperName(paper2, &name2) == noErr) &&
         (CFStringCompare(name1, name2, kCFCompareCaseInsensitive) ==
          kCFCompareEqualTo);
}

PMPaper MatchPaper(CFArrayRef paper_list,
                   CFStringRef name,
                   double width,
                   double height) {
  double best_match = std::numeric_limits<double>::max();
  PMPaper best_matching_paper = nullptr;

  CFIndex num_papers = CFArrayGetCount(paper_list);
  for (CFIndex i = 0; i < num_papers; ++i) {
    PMPaper paper = (PMPaper)CFArrayGetValueAtIndex(paper_list, i);
    double paper_width = 0.0;
    double paper_height = 0.0;
    PMPaperGetWidth(paper, &paper_width);
    PMPaperGetHeight(paper, &paper_height);
    double difference =
        std::max(fabs(width - paper_width), fabs(height - paper_height));

    // Ignore papers with size too different from expected.
    if (difference > kMaxPaperSizeDifferenceInPoints) {
      continue;
    }

    if (name && IsPaperNameEqual(name, paper))
      return paper;

    if (difference < best_match) {
      best_matching_paper = paper;
      best_match = difference;
    }
  }
  return best_matching_paper;
}

bool IsIppColorModelColorful(mojom::ColorModel color_model) {
  // Accept `kUnknownColorModel` as it can occur with raw CUPS printers.
  // Treat it similarly to the behavior in  `GetColorModelForModel()`.
  if (color_model == mojom::ColorModel::kUnknownColorModel) {
    return false;
  }
  return IsColorModelSelected(color_model).value();
}

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

// The set of "capture" routines run in the browser process.
// The set of "apply" routines run in the Print Backend service.

base::expected<std::vector<uint8_t>, mojom::ResultCode>
CaptureSystemPrintSettings(PMPrintSettings& print_settings) {
  base::apple::ScopedCFTypeRef<CFDataRef> data_ref;
  OSStatus status = PMPrintSettingsCreateDataRepresentation(
      print_settings, data_ref.InitializeInto(), kPMDataFormatXMLDefault);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status)
        << "Failed to create data representation of print settings";
    return base::unexpected(mojom::ResultCode::kFailed);
  }

  auto data_span = base::apple::CFDataToSpan(data_ref.get());
  return std::vector<uint8_t>(data_span.begin(), data_span.end());
}

base::expected<std::vector<uint8_t>, mojom::ResultCode> CaptureSystemPageFormat(
    PMPageFormat& page_format) {
  base::apple::ScopedCFTypeRef<CFDataRef> data_ref;
  OSStatus status = PMPageFormatCreateDataRepresentation(
      page_format, data_ref.InitializeInto(), kPMDataFormatXMLDefault);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status)
        << "Failed to create data representation of page format";
    return base::unexpected(mojom::ResultCode::kFailed);
  }

  auto data_span = base::apple::CFDataToSpan(data_ref.get());
  return std::vector<uint8_t>(data_span.begin(), data_span.end());
}

base::expected<base::apple::ScopedCFTypeRef<CFStringRef>, mojom::ResultCode>
CaptureSystemDestinationFormat(PMPrintSession& print_session,
                               PMPrintSettings& print_settings) {
  CFStringRef destination_format_ref = nullptr;
  OSStatus status = PMSessionCopyDestinationFormat(
      print_session, print_settings, &destination_format_ref);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to get printing destination format";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  return base::apple::ScopedCFTypeRef<CFStringRef>(destination_format_ref);
}

base::expected<base::apple::ScopedCFTypeRef<CFURLRef>, mojom::ResultCode>
CaptureSystemDestinationLocation(PMPrintSession& print_session,
                                 PMPrintSettings& print_settings) {
  CFURLRef destination_location_ref = nullptr;
  OSStatus status = PMSessionCopyDestinationLocation(
      print_session, print_settings, &destination_location_ref);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status)
        << "Failed to get printing destination location";
    return base::unexpected(mojom::ResultCode::kFailed);
  }
  return base::apple::ScopedCFTypeRef<CFURLRef>(destination_location_ref);
}

mojom::ResultCode CaptureSystemPrintDialogData(NSPrintInfo* print_info,
                                               PrintSettings* settings) {
  PMPrintSettings print_settings =
      (PMPrintSettings)[print_info PMPrintSettings];

  base::expected<std::vector<uint8_t>, mojom::ResultCode> print_settings_data =
      CaptureSystemPrintSettings(print_settings);
  if (!print_settings_data.has_value()) {
    return print_settings_data.error();
  }

  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info PMPageFormat]);

  base::expected<std::vector<uint8_t>, mojom::ResultCode> page_format_data =
      CaptureSystemPageFormat(page_format);
  if (!page_format_data.has_value()) {
    return page_format_data.error();
  }

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info PMPrintSession]);

  PMDestinationType destination_type = kPMDestinationInvalid;
  PMSessionGetDestinationType(print_session, print_settings, &destination_type);

  base::expected<base::apple::ScopedCFTypeRef<CFStringRef>, mojom::ResultCode>
      destination_format =
          CaptureSystemDestinationFormat(print_session, print_settings);
  if (!destination_format.has_value()) {
    return destination_format.error();
  }

  base::expected<base::apple::ScopedCFTypeRef<CFURLRef>, mojom::ResultCode>
      destination_location =
          CaptureSystemDestinationLocation(print_session, print_settings);
  if (!destination_location.has_value()) {
    return destination_location.error();
  }

  base::Value::Dict dialog_data;
  dialog_data.Set(kMacSystemPrintDialogDataPrintSettings,
                  std::move(print_settings_data.value()));
  dialog_data.Set(kMacSystemPrintDialogDataPageFormat,
                  std::move(page_format_data.value()));
  dialog_data.Set(kMacSystemPrintDialogDataDestinationType, destination_type);
  if (destination_format.value()) {
    dialog_data.Set(
        kMacSystemPrintDialogDataDestinationFormat,
        base::SysCFStringRefToUTF8(destination_format.value().get()));
  }
  if (destination_location.value()) {
    dialog_data.Set(kMacSystemPrintDialogDataDestinationLocation,
                    base::SysCFStringRefToUTF8(
                        CFURLGetString(destination_location.value().get())));
  }
  settings->set_system_print_dialog_data(std::move(dialog_data));
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode ApplySystemPrintSettings(
    const base::Value::Dict& system_print_dialog_data,
    NSPrintInfo* print_info,
    PMPrintSession& print_session,
    PMPrintSettings& print_settings) {
  const base::Value::BlobStorage* data =
      system_print_dialog_data.FindBlob(kMacSystemPrintDialogDataPrintSettings);
  CHECK(data);
  uint32_t data_size = data->size();
  CHECK_GT(data_size, 0u);
  CFDataRef data_ref =
      CFDataCreate(kCFAllocatorDefault,
                   static_cast<const UInt8*>(&data->front()), data_size);
  CHECK(data_ref);
  base::apple::ScopedCFTypeRef<CFDataRef> scoped_data_ref(data_ref);

  ScopedPMType<PMPrintSettings> new_print_settings;
  OSStatus status = PMPrintSettingsCreateWithDataRepresentation(
      data_ref, new_print_settings.InitializeInto());
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to create print settings";
    return mojom::ResultCode::kFailed;
  }

  status = PMSessionValidatePrintSettings(
      print_session, new_print_settings.get(), kPMDontWantBoolean);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to validate print settings";
    return mojom::ResultCode::kFailed;
  }
  status = PMCopyPrintSettings(new_print_settings.get(), print_settings);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to copy print settings";
    return mojom::ResultCode::kFailed;
  }

  [print_info updateFromPMPrintSettings];
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode ApplySystemPageFormat(
    const base::Value::Dict& system_print_dialog_data,
    NSPrintInfo* print_info,
    PMPrintSession& print_session,
    PMPageFormat& page_format) {
  const base::Value::BlobStorage* data =
      system_print_dialog_data.FindBlob(kMacSystemPrintDialogDataPageFormat);
  CHECK(data);
  uint32_t data_size = data->size();
  CHECK_GT(data_size, 0u);
  CFDataRef data_ref =
      CFDataCreate(kCFAllocatorDefault,
                   static_cast<const UInt8*>(&data->front()), data_size);
  CHECK(data_ref);

  ScopedPMType<PMPageFormat> new_page_format;
  OSStatus status = PMPageFormatCreateWithDataRepresentation(
      data_ref, new_page_format.InitializeInto());
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to create page format";
    return mojom::ResultCode::kFailed;
  }
  status = PMSessionValidatePageFormat(print_session, page_format,
                                       kPMDontWantBoolean);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to validate page format";
    return mojom::ResultCode::kFailed;
  }
  status = PMCopyPageFormat(new_page_format.get(), page_format);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to copy page format";
    return mojom::ResultCode::kFailed;
  }

  [print_info updateFromPMPageFormat];
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode ApplySystemDestination(
    const std::u16string& device_name,
    const base::Value::Dict& system_print_dialog_data,
    PMPrintSession& print_session,
    PMPrintSettings& print_settings) {
  std::optional<int> destination_type = system_print_dialog_data.FindInt(
      kMacSystemPrintDialogDataDestinationType);

  CHECK(destination_type.has_value());
  CHECK(base::IsValueInRangeForNumericType<uint16_t>(*destination_type));

  const std::string* destination_format_str =
      system_print_dialog_data.FindString(
          kMacSystemPrintDialogDataDestinationFormat);
  const std::string* destination_location_str =
      system_print_dialog_data.FindString(
          kMacSystemPrintDialogDataDestinationLocation);

  base::apple::ScopedCFTypeRef<CFStringRef> destination_format;
  if (destination_format_str) {
    destination_format.reset(
        base::SysUTF8ToCFStringRef(*destination_format_str));
  }

  base::apple::ScopedCFTypeRef<CFURLRef> destination_location;
  if (destination_location_str) {
    destination_location.reset(CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault,
        base::SysUTF8ToCFStringRef(*destination_location_str).get(),
        kCFURLPOSIXPathStyle,
        /*isDirectory=*/FALSE));
  }

  base::apple::ScopedCFTypeRef<CFStringRef> destination_name(
      base::SysUTF16ToCFStringRef(device_name));
  ScopedPMType<PMPrinter> printer(
      PMPrinterCreateFromPrinterID(destination_name.get()));
  if (!printer) {
    LOG(ERROR) << "Unable to create printer from printer ID `" << device_name
               << "`";
    return mojom::ResultCode::kFailed;
  }
  OSStatus status = PMSessionSetCurrentPMPrinter(print_session, printer.get());
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to set current printer";
    return mojom::ResultCode::kFailed;
  }

  status = PMSessionSetDestination(
      print_session, print_settings,
      static_cast<PMDestinationType>(*destination_type),
      destination_format.get(), destination_location.get());
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "Failed to set destination";
    return mojom::ResultCode::kFailed;
  }
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode ApplySystemPrintDialogData(
    const std::u16string& device_name,
    const base::Value::Dict& system_print_dialog_data,
    NSPrintInfo* print_info) {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info PMPrintSettings]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info PMPageFormat]);

  mojom::ResultCode result = ApplySystemDestination(
      device_name, system_print_dialog_data, print_session, print_settings);
  if (result != mojom::ResultCode::kSuccess) {
    return result;
  }
  result = ApplySystemPrintSettings(system_print_dialog_data, print_info,
                                    print_session, print_settings);
  if (result != mojom::ResultCode::kSuccess) {
    return result;
  }
  return ApplySystemPageFormat(system_print_dialog_data, print_info,
                               print_session, page_format);
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)

}  // namespace

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    ProcessBehavior process_behavior) {
  return std::make_unique<PrintingContextMac>(delegate, process_behavior);
}

PrintingContextMac::PrintingContextMac(Delegate* delegate,
                                       ProcessBehavior process_behavior)
    : PrintingContext(delegate, process_behavior),
      print_info_([NSPrintInfo.sharedPrintInfo copy]) {}

PrintingContextMac::~PrintingContextMac() {
  ReleaseContext();
}

void PrintingContextMac::AskUserForSettings(int max_pages,
                                            bool has_selection,
                                            bool is_scripted,
                                            PrintSettingsCallback callback) {
  // Exceptions can also happen when the NSPrintPanel is being
  // deallocated, so it must be autoreleased within this scope.
  @autoreleasepool {
    DCHECK(NSThread.isMainThread);

    // We deliberately don't feed max_pages into the dialog, because setting
    // NSPrintLastPage makes the print dialog pre-select the option to only
    // print a range.

    // TODO(stuartmorgan): implement 'print selection only' (probably requires
    // adding a new custom view to the panel on 10.5; 10.6 has
    // NSPrintPanelShowsPrintSelection).
    NSPrintPanel* panel = [NSPrintPanel printPanel];
    panel.options |= NSPrintPanelShowsPaperSize | NSPrintPanelShowsOrientation |
                     NSPrintPanelShowsScaling;

    // Set the print job title text.
    gfx::NativeView parent_view = delegate_->GetParentView();
    if (parent_view) {
      NSString* job_title = parent_view.GetNativeNSView().window.title;
      if (job_title) {
        PMPrintSettings print_settings =
            static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
        PMPrintSettingsSetJobName(print_settings,
                                  base::apple::NSToCFPtrCast(job_title));
        [print_info_ updateFromPMPrintSettings];
      }
    }

    // TODO(stuartmorgan): We really want a tab sheet here, not a modal window.
    // Will require restructuring the PrintingContext API to use a callback.

    // This function may be called in the middle of a CATransaction, where
    // running a modal panel is forbidden. That situation isn't ideal, but from
    // this code's POV the right answer is to defer running the panel until
    // after the current transaction. See https://crbug.com/849538.
    __block auto block_callback = std::move(callback);
    [CATransaction setCompletionBlock:^{
      NSInteger selection = [panel runModalWithPrintInfo:print_info_];
      if (selection == NSModalResponseOK) {
        print_info_ = [panel printInfo];
        settings_->set_ranges(GetPageRangesFromPrintInfo());
        InitPrintSettingsFromPrintInfo();
        mojom::ResultCode result = mojom::ResultCode::kSuccess;
#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
        if (process_behavior() == ProcessBehavior::kOopEnabledSkipSystemCalls) {
          // This is running in the browser process, where system calls are
          // normally not allowed except for this system dialog exception.
          // Capture the setting here to be transmitted to a PrintBackend
          // service when the document is printed.
          result = CaptureSystemPrintDialogData(print_info_, settings_.get());
        }
#endif
        std::move(block_callback).Run(result);
      } else {
        std::move(block_callback).Run(mojom::ResultCode::kCanceled);
      }
    }];
  }
}

gfx::Size PrintingContextMac::GetPdfPaperSizeDeviceUnits() {
  // NOTE: Reset |print_info_| with a copy of |sharedPrintInfo| so as to start
  // with a clean slate.
  print_info_ = [[NSPrintInfo sharedPrintInfo] copy];
  UpdatePageFormatWithPaperInfo();

  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);
  PMRect paper_rect;
  PMGetAdjustedPaperRect(page_format, &paper_rect);

  // Device units are in points. Units per inch is 72.
  gfx::Size physical_size_device_units((paper_rect.right - paper_rect.left),
                                       (paper_rect.bottom - paper_rect.top));
  DCHECK(settings_->device_units_per_inch() == kPointsPerInch);
  return physical_size_device_units;
}

mojom::ResultCode PrintingContextMac::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  print_info_ = [[NSPrintInfo sharedPrintInfo] copy];
  settings_->set_ranges(GetPageRangesFromPrintInfo());
  InitPrintSettingsFromPrintInfo();

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextMac::UpdatePrinterSettings(
    const PrinterSettings& printer_settings) {
  DCHECK(!printer_settings.show_system_dialog);
  DCHECK(!in_print_job_);

  // NOTE: Reset |print_info_| with a copy of |sharedPrintInfo| so as to start
  // with a clean slate.
  print_info_ = [[NSPrintInfo sharedPrintInfo] copy];

  if (printer_settings.external_preview) {
    if (!SetPrintPreviewJob())
      return OnError();
  } else {
    // Don't need this for preview.
    if (!SetPrinter(base::UTF16ToUTF8(settings_->device_name())) ||
        !SetCopiesInPrintSettings(settings_->copies()) ||
        !SetCollateInPrintSettings(settings_->collate()) ||
        !SetDuplexModeInPrintSettings(settings_->duplex_mode()) ||
        !SetOutputColor(static_cast<int>(settings_->color())) ||
        !SetResolution(settings_->dpi_size())) {
      return OnError();
    }
  }

  if (!UpdatePageFormatWithPaperInfo() ||
      !SetOrientationIsLandscape(settings_->landscape())) {
    return OnError();
  }

  [print_info_ updateFromPMPrintSettings];

  InitPrintSettingsFromPrintInfo();
  return mojom::ResultCode::kSuccess;
}

bool PrintingContextMac::SetPrintPreviewJob() {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  return PMSessionSetDestination(print_session, print_settings,
                                 kPMDestinationPreview, nullptr,
                                 nullptr) == noErr;
}

void PrintingContextMac::InitPrintSettingsFromPrintInfo() {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);
  PMPrinter printer;
  PMSessionGetCurrentPrinter(print_session, &printer);
  PrintSettingsInitializerMac::InitPrintSettings(printer, page_format,
                                                 settings_.get());
}

bool PrintingContextMac::SetPrinter(const std::string& device_name) {
  DCHECK(print_info_);
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);

  PMPrinter current_printer;
  if (PMSessionGetCurrentPrinter(print_session, &current_printer) != noErr)
    return false;

  CFStringRef current_printer_id = PMPrinterGetID(current_printer);
  if (!current_printer_id)
    return false;

  base::apple::ScopedCFTypeRef<CFStringRef> new_printer_id(
      base::SysUTF8ToCFStringRef(device_name));
  if (!new_printer_id.get())
    return false;

  if (CFStringCompare(new_printer_id.get(), current_printer_id, 0) ==
      kCFCompareEqualTo) {
    return true;
  }

  ScopedPMType<PMPrinter> new_printer(
      PMPrinterCreateFromPrinterID(new_printer_id.get()));
  if (!new_printer) {
    return false;
  }

  return PMSessionSetCurrentPMPrinter(print_session, new_printer.get()) ==
         noErr;
}

bool PrintingContextMac::UpdatePageFormatWithPaperInfo() {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);

  PMPageFormat default_page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);

  PMPrinter current_printer = nullptr;
  if (PMSessionGetCurrentPrinter(print_session, &current_printer) != noErr)
    return false;

  double page_width = 0.0;
  double page_height = 0.0;
  base::apple::ScopedCFTypeRef<CFStringRef> paper_name;
  PMPaperMargins margins = {0};

  const PrintSettings::RequestedMedia& media = settings_->requested_media();
  if (media.IsDefault()) {
    PMPaper default_paper;
    if (PMGetPageFormatPaper(default_page_format, &default_paper) != noErr ||
        PMPaperGetWidth(default_paper, &page_width) != noErr ||
        PMPaperGetHeight(default_paper, &page_height) != noErr) {
      return false;
    }

    // Ignore result, because we can continue without following.
    CFStringRef tmp_paper_name = nullptr;
    PMPaperGetPPDPaperName(default_paper, &tmp_paper_name);
    PMPaperGetMargins(default_paper, &margins);
    paper_name.reset(tmp_paper_name, base::scoped_policy::RETAIN);
  } else {
    const double kMultiplier =
        kPointsPerInch / static_cast<float>(kMicronsPerInch);
    page_width = media.size_microns.width() * kMultiplier;
    page_height = media.size_microns.height() * kMultiplier;
    paper_name.reset(base::SysUTF8ToCFStringRef(media.vendor_id));
  }

  CFArrayRef paper_list = nullptr;
  if (PMPrinterGetPaperList(current_printer, &paper_list) != noErr)
    return false;

  PMPaper best_matching_paper =
      MatchPaper(paper_list, paper_name.get(), page_width, page_height);

  if (best_matching_paper)
    return UpdatePageFormatWithPaper(best_matching_paper, default_page_format);

  // Do nothing if unmatched paper was default system paper.
  if (media.IsDefault())
    return true;

  ScopedPMType<PMPaper> paper;
  if (PMPaperCreateCustom(current_printer, CFSTR("Custom paper ID"),
                          CFSTR("Custom paper"), page_width, page_height,
                          &margins, paper.InitializeInto()) != noErr) {
    return false;
  }
  return UpdatePageFormatWithPaper(paper.get(), default_page_format);
}

bool PrintingContextMac::UpdatePageFormatWithPaper(PMPaper paper,
                                                   PMPageFormat page_format) {
  ScopedPMType<PMPageFormat> new_format;
  if (PMCreatePageFormatWithPMPaper(new_format.InitializeInto(), paper) !=
      noErr) {
    return false;
  }
  // Copy over the original format with the new page format.
  bool result = (PMCopyPageFormat(new_format.get(), page_format) == noErr);
  [print_info_ updateFromPMPageFormat];
  return result;
}

bool PrintingContextMac::SetCopiesInPrintSettings(int copies) {
  if (copies < 1)
    return false;

  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  return PMSetCopies(print_settings, copies, false) == noErr;
}

bool PrintingContextMac::SetCollateInPrintSettings(bool collate) {
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  return PMSetCollate(print_settings, collate) == noErr;
}

bool PrintingContextMac::SetOrientationIsLandscape(bool landscape) {
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);

  PMOrientation orientation = landscape ? kPMLandscape : kPMPortrait;

  if (PMSetOrientation(page_format, orientation, false) != noErr)
    return false;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);

  PMSessionValidatePageFormat(print_session, page_format, kPMDontWantBoolean);

  [print_info_ updateFromPMPageFormat];
  return true;
}

bool PrintingContextMac::SetDuplexModeInPrintSettings(mojom::DuplexMode mode) {
  PMDuplexMode duplexSetting;
  switch (mode) {
    case mojom::DuplexMode::kLongEdge:
      duplexSetting = kPMDuplexNoTumble;
      break;
    case mojom::DuplexMode::kShortEdge:
      duplexSetting = kPMDuplexTumble;
      break;
    case mojom::DuplexMode::kSimplex:
      duplexSetting = kPMDuplexNone;
      break;
    default:  // kUnknownDuplexMode
      return true;
  }

  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  return PMSetDuplex(print_settings, duplexSetting) == noErr;
}

bool PrintingContextMac::SetOutputColor(int color_mode) {
  const mojom::ColorModel color_model = ColorModeToColorModel(color_mode);

  if (!base::FeatureList::IsEnabled(features::kCupsIppPrintingBackend)) {
    std::string color_setting_name;
    std::string color_value;
    GetColorModelForModel(color_model, &color_setting_name, &color_value);
    return SetKeyValue(color_setting_name, color_value);
  }

  // First, set the default CUPS IPP output color.
  if (!SetKeyValue(CUPS_PRINT_COLOR_MODE,
                   GetIppColorModelForModel(color_model))) {
    return false;
  }

  // Even when interfacing with printer settings using CUPS IPP, the print job
  // may still expect PPD color values if the printer was added to the system
  // with a PPD. To avoid parsing PPDs (which is the point of using CUPS IPP),
  // set every single known PPD color setting and hope that one of them sticks.
  const bool is_color = IsIppColorModelColorful(color_model);
  for (const auto& setting : GetKnownPpdColorSettings()) {
    std::string_view color_setting_name = setting.name;
    std::string_view color_value = is_color ? setting.color : setting.bw;
    if (!SetKeyValue(color_setting_name, color_value))
      return false;
  }

  return true;
}

bool PrintingContextMac::SetResolution(const gfx::Size& dpi_size) {
  if (dpi_size.IsEmpty())
    return true;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPrinter current_printer;
  if (PMSessionGetCurrentPrinter(print_session, &current_printer) != noErr)
    return false;

  PMResolution resolution;
  resolution.hRes = dpi_size.width();
  resolution.vRes = dpi_size.height();

  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  return PMPrinterSetOutputResolution(current_printer, print_settings,
                                      &resolution) == noErr;
}

bool PrintingContextMac::SetKeyValue(std::string_view key,
                                     std::string_view value) {
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  base::apple::ScopedCFTypeRef<CFStringRef> cf_key =
      base::SysUTF8ToCFStringRef(key);
  base::apple::ScopedCFTypeRef<CFStringRef> cf_value =
      base::SysUTF8ToCFStringRef(value);

  return PMPrintSettingsSetValue(print_settings, cf_key.get(), cf_value.get(),
                                 /*locked=*/false) == noErr;
}

PageRanges PrintingContextMac::GetPageRangesFromPrintInfo() {
  PageRanges page_ranges;
  NSDictionary* print_info_dict = [print_info_ dictionary];
  if (![print_info_dict[NSPrintAllPages] boolValue]) {
    PageRange range;
    range.from = [print_info_dict[NSPrintFirstPage] intValue] - 1;
    range.to = [print_info_dict[NSPrintLastPage] intValue] - 1;
    page_ranges.push_back(range);
  }
  return page_ranges;
}

mojom::ResultCode PrintingContextMac::NewDocument(
    const std::u16string& document_name) {
  DCHECK(!in_print_job_);

  in_print_job_ = true;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (process_behavior() == ProcessBehavior::kOopEnabledSkipSystemCalls) {
    return mojom::ResultCode::kSuccess;
  }
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  if (process_behavior() == ProcessBehavior::kOopEnabledPerformSystemCalls &&
      !settings_->system_print_dialog_data().empty()) {
    // Settings which the browser process captured from the system dialog now
    // need to be applied to the printing context here which is running in a
    // PrintBackend service.

    // NOTE: Reset `print_info_` with a copy of `sharedPrintInfo` so as to
    // start with a clean slate.
    print_info_ = [[NSPrintInfo sharedPrintInfo] copy];

    mojom::ResultCode result = ApplySystemPrintDialogData(
        settings_->device_name(), settings_->system_print_dialog_data(),
        print_info_);
    if (result != mojom::ResultCode::kSuccess) {
      return result;
    }
  }
#endif

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);

  base::apple::ScopedCFTypeRef<CFStringRef> job_title =
      base::SysUTF16ToCFStringRef(document_name);
  PMPrintSettingsSetJobName(print_settings, job_title.get());

  OSStatus status = PMSessionBeginCGDocumentNoDialog(
      print_session, print_settings, page_format);
  if (status != noErr)
    return OnError();

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextMac::NewPage() {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  DCHECK(!context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);
  OSStatus status;
  status = PMSessionBeginPageNoDialog(print_session, page_format, nullptr);
  if (status != noErr)
    return OnError();
  status = PMSessionGetCGGraphicsContext(print_session, &context_);
  if (status != noErr)
    return OnError();

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextMac::PageDone() {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  DCHECK(context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  OSStatus status = PMSessionEndPageNoDialog(print_session);
  if (status != noErr)
    OnError();
  context_ = nullptr;

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextMac::PrintDocument(
    const MetafilePlayer& metafile,
    const PrintSettings& settings,
    uint32_t num_pages) {
  const PageSetup& page_setup = settings.page_setup_device_units();
  const CGRect paper_rect = gfx::Rect(page_setup.physical_size()).ToCGRect();

  for (size_t metafile_page_number = 1; metafile_page_number <= num_pages;
       metafile_page_number++) {
    mojom::ResultCode result = NewPage();
    if (result != mojom::ResultCode::kSuccess)
      return result;
    if (!metafile.RenderPage(metafile_page_number, context_, paper_rect,
                             /*autorotate=*/true, /*fit_to_page=*/false)) {
      return mojom::ResultCode::kFailed;
    }
    result = PageDone();
    if (result != mojom::ResultCode::kSuccess)
      return result;
  }
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextMac::DocumentDone() {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  OSStatus status = PMSessionEndDocumentNoDialog(print_session);
  if (status != noErr)
    OnError();

  ResetSettings();
  return mojom::ResultCode::kSuccess;
}

void PrintingContextMac::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  context_ = nullptr;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMSessionEndPageNoDialog(print_session);
}

void PrintingContextMac::ReleaseContext() {
  print_info_ = nil;
  context_ = nullptr;
}

printing::NativeDrawingContext PrintingContextMac::context() const {
  return context_;
}

}  // namespace printing
