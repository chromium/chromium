// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_linux.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_dialog_linux_interface.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include "printing/printing_features.h"
#endif

// Avoid using LinuxUi on Fuchsia.
#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"
#endif

namespace printing {

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    ProcessBehavior process_behavior) {
  return std::make_unique<PrintingContextLinux>(delegate, process_behavior);
}

PrintingContextLinux::PrintingContextLinux(Delegate* delegate,
                                           ProcessBehavior process_behavior)
    : PrintingContext(delegate, process_behavior), print_dialog_(nullptr) {}

PrintingContextLinux::~PrintingContextLinux() {
  ReleaseContext();

  if (print_dialog_)
    print_dialog_.ExtractAsDangling()->ReleaseDialog();
}

void PrintingContextLinux::AskUserForSettings(int max_pages,
                                              bool has_selection,
                                              bool is_scripted,
                                              PrintSettingsCallback callback) {
  if (!print_dialog_) {
    // Can only get here if the renderer is sending bad messages.
    // http://crbug.com/341777
    NOTREACHED();
  }

  print_dialog_->ShowDialog(delegate_->GetParentView(), has_selection,
                            std::move(callback));
}

mojom::ResultCode PrintingContextLinux::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();

#if BUILDFLAG(IS_LINUX)
  if (!ui::LinuxUi::instance())
    return mojom::ResultCode::kSuccess;

  if (!print_dialog_)
    print_dialog_ = ui::LinuxUi::instance()->CreatePrintDialog(this);

  if (print_dialog_) {
    print_dialog_->UseDefaultSettings();
  }
#endif

  return mojom::ResultCode::kSuccess;
}

gfx::Size PrintingContextLinux::GetPdfPaperSizeDeviceUnits() {
#if BUILDFLAG(IS_LINUX)
  if (ui::LinuxUi::instance())
    return ui::LinuxUi::instance()->GetPdfPaperSize(this);
#endif

  return gfx::Size();
}

mojom::ResultCode PrintingContextLinux::UpdatePrinterSettings(
    const PrinterSettings& printer_settings) {
  DCHECK(!printer_settings.show_system_dialog);
  DCHECK(!in_print_job_);

#if BUILDFLAG(IS_LINUX)
  if (!ui::LinuxUi::instance())
    return mojom::ResultCode::kSuccess;

  if (!print_dialog_)
    print_dialog_ = ui::LinuxUi::instance()->CreatePrintDialog(this);

  if (print_dialog_) {
    // PrintDialogGtk::UpdateSettings() calls InitWithSettings() so settings_ will
    // remain non-null after this line.
    print_dialog_->UpdateSettings(std::move(settings_));
    DCHECK(settings_);
  }
#endif

  return mojom::ResultCode::kSuccess;
}

void PrintingContextLinux::InitWithSettings(
    std::unique_ptr<PrintSettings> settings) {
  DCHECK(!in_print_job_);

  settings_ = std::move(settings);
}

mojom::ResultCode PrintingContextLinux::NewDocument(
    const std::u16string& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

  document_name_ = document_name;

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  if (process_behavior() == ProcessBehavior::kOopEnabledSkipSystemCalls) {
    return mojom::ResultCode::kSuccess;
  }

  if (process_behavior() == ProcessBehavior::kOopEnabledPerformSystemCalls &&
      !settings_->system_print_dialog_data().empty()) {
    // Take the settings captured by the browser process from the system print
    // dialog and apply them to this printing context in the PrintBackend
    // service.
    if (!print_dialog_) {
      CHECK(ui::LinuxUi::instance());
      print_dialog_ = ui::LinuxUi::instance()->CreatePrintDialog(this);
    }
    print_dialog_->LoadPrintSettings(*settings_);
  }
#endif

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextLinux::PrintDocument(
    const MetafilePlayer& metafile,
    const PrintSettings& settings,
    uint32_t num_pages) {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  if (!print_dialog_) {
    return mojom::ResultCode::kFailed;
  }
  // TODO(crbug.com/40198881)  Plumb error code back from
  // `PrintDialogLinuxInterface`.
  print_dialog_->PrintDocument(metafile, document_name_);
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextLinux::DocumentDone() {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);

  ResetSettings();
  return mojom::ResultCode::kSuccess;
}

void PrintingContextLinux::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
}

void PrintingContextLinux::ReleaseContext() {
  // Intentional No-op.
}

printing::NativeDrawingContext PrintingContextLinux::context() const {
  // Intentional No-op.
  return nullptr;
}

}  // namespace printing
