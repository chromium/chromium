// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_no_system_dialog.h"

#include <stdint.h>
#include <unicode/ulocdata.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "printing/metafile.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace printing {

#if !defined(USE_CUPS)
// static
std::unique_ptr<PrintingContext> PrintingContext::Create(Delegate* delegate) {
  return std::make_unique<PrintingContextNoSystemDialog>(delegate);
}
#endif  // !defined(USE_CUPS)

PrintingContextNoSystemDialog::PrintingContextNoSystemDialog(Delegate* delegate)
    : PrintingContext(delegate) {}

PrintingContextNoSystemDialog::~PrintingContextNoSystemDialog() {
  ReleaseContext();
}

void PrintingContextNoSystemDialog::AskUserForSettings(
    int max_pages,
    bool has_selection,
    bool is_scripted,
    PrintSettingsCallback callback) {
  // We don't want to bring up a dialog here.  Ever.  Just signal the callback.
  std::move(callback).Run(OK);
}

PrintingContext::Result PrintingContextNoSystemDialog::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();
  settings_->set_dpi(kDefaultPdfDpi);
  gfx::Size physical_size = GetPdfPaperSizeDeviceUnits();
  // Assume full page is printable for now.
  gfx::Rect printable_area(0, 0, physical_size.width(), physical_size.height());
  DCHECK_EQ(settings_->device_units_per_inch(), kDefaultPdfDpi);
  settings_->SetPrinterPrintableArea(physical_size, printable_area, true);
  return OK;
}

gfx::Size PrintingContextNoSystemDialog::GetPdfPaperSizeDeviceUnits() {
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

PrintingContext::Result PrintingContextNoSystemDialog::UpdatePrinterSettings(
    bool external_preview,
    bool show_system_dialog,
    int page_count) {
  DCHECK(!show_system_dialog);

  if (settings_->dpi() == 0)
    UseDefaultSettings();

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::NewDocument(
    const base::string16& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  ResetSettings();
  return OK;
}

void PrintingContextNoSystemDialog::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
}

void PrintingContextNoSystemDialog::ReleaseContext() {
  // Intentional No-op.
}

printing::NativeDrawingContext PrintingContextNoSystemDialog::context() const {
  // Intentional No-op.
  return nullptr;
}

}  // namespace printing
