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

#include "base/apple/bridging.h"
#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_piece.h"
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace printing {

namespace {

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

}  // namespace

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    bool skip_system_calls) {
  auto context = std::make_unique<PrintingContextMac>(delegate);
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (skip_system_calls)
    context->set_skip_system_calls();
#endif
  return context;
}

PrintingContextMac::PrintingContextMac(Delegate* delegate)
    : PrintingContext(delegate),
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
        std::move(block_callback).Run(mojom::ResultCode::kSuccess);
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

  base::ScopedCFTypeRef<CFStringRef> new_printer_id(
      base::SysUTF8ToCFStringRef(device_name));
  if (!new_printer_id.get())
    return false;

  if (CFStringCompare(new_printer_id.get(), current_printer_id, 0) ==
      kCFCompareEqualTo) {
    return true;
  }

  PMPrinter new_printer = PMPrinterCreateFromPrinterID(new_printer_id.get());
  if (!new_printer)
    return false;

  OSStatus status = PMSessionSetCurrentPMPrinter(print_session, new_printer);
  PMRelease(new_printer);
  return status == noErr;
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
  base::ScopedCFTypeRef<CFStringRef> paper_name;
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
      MatchPaper(paper_list, paper_name, page_width, page_height);

  if (best_matching_paper)
    return UpdatePageFormatWithPaper(best_matching_paper, default_page_format);

  // Do nothing if unmatched paper was default system paper.
  if (media.IsDefault())
    return true;

  PMPaper paper = nullptr;
  if (PMPaperCreateCustom(current_printer, CFSTR("Custom paper ID"),
                          CFSTR("Custom paper"), page_width, page_height,
                          &margins, &paper) != noErr) {
    return false;
  }
  bool result = UpdatePageFormatWithPaper(paper, default_page_format);
  PMRelease(paper);
  return result;
}

bool PrintingContextMac::UpdatePageFormatWithPaper(PMPaper paper,
                                                   PMPageFormat page_format) {
  PMPageFormat new_format = nullptr;
  if (PMCreatePageFormatWithPMPaper(&new_format, paper) != noErr)
    return false;
  // Copy over the original format with the new page format.
  bool result = (PMCopyPageFormat(new_format, page_format) == noErr);
  [print_info_ updateFromPMPageFormat];
  PMRelease(new_format);
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
  const bool is_color = IsColorModelSelected(color_model).value_or(false);
  for (const auto& setting : GetKnownPpdColorSettings()) {
    const base::StringPiece& color_setting_name = setting.name;
    const base::StringPiece& color_value =
        is_color ? setting.color : setting.bw;
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

bool PrintingContextMac::SetKeyValue(base::StringPiece key,
                                     base::StringPiece value) {
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  base::ScopedCFTypeRef<CFStringRef> cf_key = base::SysUTF8ToCFStringRef(key);
  base::ScopedCFTypeRef<CFStringRef> cf_value =
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

  if (skip_system_calls())
    return mojom::ResultCode::kSuccess;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_ PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_ PMPrintSettings]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_ PMPageFormat]);

  base::ScopedCFTypeRef<CFStringRef> job_title =
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
