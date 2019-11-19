// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_mac.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#import <iomanip>
#import <numeric>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "printing/print_settings_initializer_mac.h"
#include "printing/units.h"

namespace printing {

namespace {

const int kMaxPaperSizeDiffereceInPoints = 2;

// Return true if PPD name of paper is equal.
bool IsPaperNameEqual(CFStringRef name1, const PMPaper& paper2) {
  CFStringRef name2 = NULL;
  return (name1 && PMPaperGetPPDPaperName(paper2, &name2) == noErr) &&
         (CFStringCompare(name1, name2, kCFCompareCaseInsensitive) ==
          kCFCompareEqualTo);
}

PMPaper MatchPaper(CFArrayRef paper_list,
                   CFStringRef name,
                   double width,
                   double height) {
  double best_match = std::numeric_limits<double>::max();
  PMPaper best_matching_paper = NULL;
  int num_papers = CFArrayGetCount(paper_list);
  for (int i = 0; i < num_papers; ++i) {
    PMPaper paper = (PMPaper)[(NSArray*)paper_list objectAtIndex:i];
    double paper_width = 0.0;
    double paper_height = 0.0;
    PMPaperGetWidth(paper, &paper_width);
    PMPaperGetHeight(paper, &paper_height);
    double difference =
        std::max(fabs(width - paper_width), fabs(height - paper_height));

    // Ignore papers with size too different from expected.
    if (difference > kMaxPaperSizeDiffereceInPoints)
      continue;

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
std::unique_ptr<PrintingContext> PrintingContext::Create(Delegate* delegate) {
  return base::WrapUnique(new PrintingContextMac(delegate));
}

PrintingContextMac::PrintingContextMac(Delegate* delegate)
    : PrintingContext(delegate),
      print_info_([[NSPrintInfo sharedPrintInfo] copy]),
      context_(NULL) {}

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
    DCHECK([NSThread isMainThread]);

    // We deliberately don't feed max_pages into the dialog, because setting
    // NSPrintLastPage makes the print dialog pre-select the option to only
    // print a range.

    // TODO(stuartmorgan): implement 'print selection only' (probably requires
    // adding a new custom view to the panel on 10.5; 10.6 has
    // NSPrintPanelShowsPrintSelection).
    NSPrintPanel* panel = [NSPrintPanel printPanel];
    NSPrintInfo* printInfo = print_info_.get();

    NSPrintPanelOptions options = [panel options];
    options |= NSPrintPanelShowsPaperSize;
    options |= NSPrintPanelShowsOrientation;
    options |= NSPrintPanelShowsScaling;
    [panel setOptions:options];

    // Set the print job title text.
    gfx::NativeView parent_view = delegate_->GetParentView();
    if (parent_view) {
      NSString* job_title = [[parent_view.GetNativeNSView() window] title];
      if (job_title) {
        PMPrintSettings printSettings =
            (PMPrintSettings)[printInfo PMPrintSettings];
        PMPrintSettingsSetJobName(printSettings, (CFStringRef)job_title);
        [printInfo updateFromPMPrintSettings];
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
      NSInteger selection = [panel runModalWithPrintInfo:printInfo];
      if (selection == NSOKButton) {
        print_info_.reset([[panel printInfo] retain]);
        settings_->set_ranges(GetPageRangesFromPrintInfo());
        InitPrintSettingsFromPrintInfo();
        std::move(block_callback).Run(OK);
      } else {
        std::move(block_callback).Run(CANCEL);
      }
    }];
  }
}

gfx::Size PrintingContextMac::GetPdfPaperSizeDeviceUnits() {
  // NOTE: Reset |print_info_| with a copy of |sharedPrintInfo| so as to start
  // with a clean slate.
  print_info_.reset([[NSPrintInfo sharedPrintInfo] copy]);
  UpdatePageFormatWithPaperInfo();

  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);
  PMRect paper_rect;
  PMGetAdjustedPaperRect(page_format, &paper_rect);

  // Device units are in points. Units per inch is 72.
  gfx::Size physical_size_device_units((paper_rect.right - paper_rect.left),
                                       (paper_rect.bottom - paper_rect.top));
  DCHECK(settings_->device_units_per_inch() == kPointsPerInch);
  return physical_size_device_units;
}

PrintingContext::Result PrintingContextMac::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  print_info_.reset([[NSPrintInfo sharedPrintInfo] copy]);
  settings_->set_ranges(GetPageRangesFromPrintInfo());
  InitPrintSettingsFromPrintInfo();

  return OK;
}

PrintingContext::Result PrintingContextMac::UpdatePrinterSettings(
    bool external_preview,
    bool show_system_dialog,
    int page_count) {
  DCHECK(!show_system_dialog);
  DCHECK(!in_print_job_);

  // NOTE: Reset |print_info_| with a copy of |sharedPrintInfo| so as to start
  // with a clean slate.
  print_info_.reset([[NSPrintInfo sharedPrintInfo] copy]);

  if (external_preview) {
    if (!SetPrintPreviewJob())
      return OnError();
  } else {
    // Don't need this for preview.
    if (!SetPrinter(base::UTF16ToUTF8(settings_->device_name())) ||
        !SetCopiesInPrintSettings(settings_->copies()) ||
        !SetCollateInPrintSettings(settings_->collate()) ||
        !SetDuplexModeInPrintSettings(settings_->duplex_mode()) ||
        !SetOutputColor(settings_->color())) {
      return OnError();
    }
  }

  if (!UpdatePageFormatWithPaperInfo() ||
      !SetOrientationIsLandscape(settings_->landscape())) {
    return OnError();
  }

  [print_info_.get() updateFromPMPrintSettings];

  InitPrintSettingsFromPrintInfo();
  return OK;
}

bool PrintingContextMac::SetPrintPreviewJob() {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSessionSetDestination(print_session, print_settings,
                                 kPMDestinationPreview, NULL, NULL) == noErr;
}

void PrintingContextMac::InitPrintSettingsFromPrintInfo() {
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);
  PMPrinter printer;
  PMSessionGetCurrentPrinter(print_session, &printer);
  PrintSettingsInitializerMac::InitPrintSettings(printer, page_format,
                                                 settings_.get());
}

bool PrintingContextMac::SetPrinter(const std::string& device_name) {
  DCHECK(print_info_.get());
  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);

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
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);

  PMPageFormat default_page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);

  PMPrinter current_printer = NULL;
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
    CFStringRef tmp_paper_name = NULL;
    PMPaperGetPPDPaperName(default_paper, &tmp_paper_name);
    PMPaperGetMargins(default_paper, &margins);
    paper_name.reset(tmp_paper_name, base::scoped_policy::RETAIN);
  } else {
    const double kMutiplier =
        kPointsPerInch / static_cast<float>(kMicronsPerInch);
    page_width = media.size_microns.width() * kMutiplier;
    page_height = media.size_microns.height() * kMutiplier;
    paper_name.reset(base::SysUTF8ToCFStringRef(media.vendor_id));
  }

  CFArrayRef paper_list = NULL;
  if (PMPrinterGetPaperList(current_printer, &paper_list) != noErr)
    return false;

  PMPaper best_matching_paper =
      MatchPaper(paper_list, paper_name, page_width, page_height);

  if (best_matching_paper)
    return UpdatePageFormatWithPaper(best_matching_paper, default_page_format);

  // Do nothing if unmatched paper was default system paper.
  if (media.IsDefault())
    return true;

  PMPaper paper = NULL;
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
  PMPageFormat new_format = NULL;
  if (PMCreatePageFormatWithPMPaper(&new_format, paper) != noErr)
    return false;
  // Copy over the original format with the new page format.
  bool result = (PMCopyPageFormat(new_format, page_format) == noErr);
  [print_info_.get() updateFromPMPageFormat];
  PMRelease(new_format);
  return result;
}

bool PrintingContextMac::SetCopiesInPrintSettings(int copies) {
  if (copies < 1)
    return false;

  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSetCopies(pmPrintSettings, copies, false) == noErr;
}

bool PrintingContextMac::SetCollateInPrintSettings(bool collate) {
  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSetCollate(pmPrintSettings, collate) == noErr;
}

bool PrintingContextMac::SetOrientationIsLandscape(bool landscape) {
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);

  PMOrientation orientation = landscape ? kPMLandscape : kPMPortrait;

  if (PMSetOrientation(page_format, orientation, false) != noErr)
    return false;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);

  PMSessionValidatePageFormat(print_session, page_format, kPMDontWantBoolean);

  [print_info_.get() updateFromPMPageFormat];
  return true;
}

bool PrintingContextMac::SetDuplexModeInPrintSettings(DuplexMode mode) {
  PMDuplexMode duplexSetting;
  switch (mode) {
    case LONG_EDGE:
      duplexSetting = kPMDuplexNoTumble;
      break;
    case SHORT_EDGE:
      duplexSetting = kPMDuplexTumble;
      break;
    case SIMPLEX:
      duplexSetting = kPMDuplexNone;
      break;
    default:  // UNKNOWN_DUPLEX_MODE
      return true;
  }

  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  return PMSetDuplex(pmPrintSettings, duplexSetting) == noErr;
}

bool PrintingContextMac::SetOutputColor(int color_mode) {
  PMPrintSettings pmPrintSettings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  std::string color_setting_name;
  std::string color_value;
  GetColorModelForMode(color_mode, &color_setting_name, &color_value);
  base::ScopedCFTypeRef<CFStringRef> color_setting(
      base::SysUTF8ToCFStringRef(color_setting_name));
  base::ScopedCFTypeRef<CFStringRef> output_color(
      base::SysUTF8ToCFStringRef(color_value));

  return PMPrintSettingsSetValue(pmPrintSettings, color_setting.get(),
                                 output_color.get(), false) == noErr;
}

PageRanges PrintingContextMac::GetPageRangesFromPrintInfo() {
  PageRanges page_ranges;
  NSDictionary* print_info_dict = [print_info_.get() dictionary];
  if (![[print_info_dict objectForKey:NSPrintAllPages] boolValue]) {
    PageRange range;
    range.from = [[print_info_dict objectForKey:NSPrintFirstPage] intValue] - 1;
    range.to = [[print_info_dict objectForKey:NSPrintLastPage] intValue] - 1;
    page_ranges.push_back(range);
  }
  return page_ranges;
}

PrintingContext::Result PrintingContextMac::NewDocument(
    const base::string16& document_name) {
  DCHECK(!in_print_job_);

  in_print_job_ = true;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPrintSettings print_settings =
      static_cast<PMPrintSettings>([print_info_.get() PMPrintSettings]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);

  base::ScopedCFTypeRef<CFStringRef> job_title(
      base::SysUTF16ToCFStringRef(document_name));
  PMPrintSettingsSetJobName(print_settings, job_title.get());

  OSStatus status = PMSessionBeginCGDocumentNoDialog(
      print_session, print_settings, page_format);
  if (status != noErr)
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextMac::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(!context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMPageFormat page_format =
      static_cast<PMPageFormat>([print_info_.get() PMPageFormat]);
  OSStatus status;
  status = PMSessionBeginPageNoDialog(print_session, page_format, NULL);
  if (status != noErr)
    return OnError();
  status = PMSessionGetCGGraphicsContext(print_session, &context_);
  if (status != noErr)
    return OnError();

  return OK;
}

PrintingContext::Result PrintingContextMac::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);
  DCHECK(context_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  OSStatus status = PMSessionEndPageNoDialog(print_session);
  if (status != noErr)
    OnError();
  context_ = NULL;

  return OK;
}

PrintingContext::Result PrintingContextMac::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  OSStatus status = PMSessionEndDocumentNoDialog(print_session);
  if (status != noErr)
    OnError();

  ResetSettings();
  return OK;
}

void PrintingContextMac::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  context_ = NULL;

  PMPrintSession print_session =
      static_cast<PMPrintSession>([print_info_.get() PMPrintSession]);
  PMSessionEndPageNoDialog(print_session);
}

void PrintingContextMac::ReleaseContext() {
  print_info_.reset();
  context_ = NULL;
}

printing::NativeDrawingContext PrintingContextMac::context() const {
  return context_;
}

}  // namespace printing
