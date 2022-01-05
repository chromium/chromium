// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_linux.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_dialog_gtk_interface.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace printing {

namespace {

// Function pointer for creating print dialogs. `callback` is only used when
// `show_dialog` is true.
PrintDialogGtkInterface* (*create_dialog_func_)(PrintingContextLinux* context) =
    nullptr;

// Function pointer for determining paper size.
gfx::Size (*get_pdf_paper_size_)(PrintingContextLinux* context) = nullptr;

}  // namespace

// static
std::unique_ptr<PrintingContext> PrintingContext::CreateImpl(
    Delegate* delegate,
    bool skip_system_calls) {
  auto context = std::make_unique<PrintingContextLinux>(delegate);
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (skip_system_calls)
    context->set_skip_system_calls();
#endif
  return context;
}

PrintingContextLinux::PrintingContextLinux(Delegate* delegate)
    : PrintingContext(delegate), print_dialog_(nullptr) {}

PrintingContextLinux::~PrintingContextLinux() {
  ReleaseContext();

  if (print_dialog_)
    print_dialog_->ReleaseDialog();
}

// static
void PrintingContextLinux::SetCreatePrintDialogFunction(
    PrintDialogGtkInterface* (*create_dialog_func)(
        PrintingContextLinux* context)) {
  DCHECK(create_dialog_func);
  DCHECK(!create_dialog_func_);
  create_dialog_func_ = create_dialog_func;
}

// static
void PrintingContextLinux::SetPdfPaperSizeFunction(
    gfx::Size (*get_pdf_paper_size)(PrintingContextLinux* context)) {
  DCHECK(get_pdf_paper_size);
  DCHECK(!get_pdf_paper_size_);
  get_pdf_paper_size_ = get_pdf_paper_size;
}

void PrintingContextLinux::AskUserForSettings(int max_pages,
                                              bool has_selection,
                                              bool is_scripted,
                                              PrintSettingsCallback callback) {
  if (!print_dialog_) {
    // Can only get here if the renderer is sending bad messages.
    // http://crbug.com/341777
    NOTREACHED();
    std::move(callback).Run(mojom::ResultCode::kFailed);
    return;
  }

  print_dialog_->ShowDialog(delegate_->GetParentView(), has_selection,
                            std::move(callback));
}

mojom::ResultCode PrintingContextLinux::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();

  if (!create_dialog_func_)
    return mojom::ResultCode::kSuccess;

  if (!print_dialog_) {
    print_dialog_ = create_dialog_func_(this);
    print_dialog_->AddRefToDialog();
  }
  print_dialog_->UseDefaultSettings();

  return mojom::ResultCode::kSuccess;
}

gfx::Size PrintingContextLinux::GetPdfPaperSizeDeviceUnits() {
  if (get_pdf_paper_size_)
    return get_pdf_paper_size_(this);

  return gfx::Size();
}

mojom::ResultCode PrintingContextLinux::UpdatePrinterSettings(
    const PrinterSettings& printer_settings) {
  DCHECK(!printer_settings.show_system_dialog);
  DCHECK(!in_print_job_);

  if (!create_dialog_func_)
    return mojom::ResultCode::kSuccess;

  if (!print_dialog_) {
    print_dialog_ = create_dialog_func_(this);
    print_dialog_->AddRefToDialog();
  }

  // PrintDialogGtk::UpdateSettings() calls InitWithSettings() so settings_ will
  // remain non-null after this line.
  print_dialog_->UpdateSettings(std::move(settings_));
  DCHECK(settings_);

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

  // If this implementation is expanded to include system calls then such calls
  // should be gated upon `skip_system_calls()`.

  document_name_ = document_name;

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintingContextLinux::PrintDocument(
    const MetafilePlayer& metafile,
    const PrintSettings& settings,
    uint32_t num_pages) {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  DCHECK(print_dialog_);
  // TODO(crbug.com/1252685)  Plumb error code back from
  // `PrintDialogGtkInterface`.
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
