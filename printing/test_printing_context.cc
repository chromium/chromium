// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/test_printing_context.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "printing/backend/print_backend.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"
#include "printing/units.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
#include "printing/printing_features.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "printing/printed_page_win.h"
#endif

namespace printing {

TestPrintingContextDelegate::TestPrintingContextDelegate() = default;

TestPrintingContextDelegate::~TestPrintingContextDelegate() = default;

gfx::NativeView TestPrintingContextDelegate::GetParentView() {
  return gfx::NativeView();
}

std::string TestPrintingContextDelegate::GetAppLocale() {
  return std::string();
}

TestPrintingContext::TestPrintingContext(Delegate* delegate,
                                         ProcessBehavior process_behavior)
    : PrintingContext(delegate, process_behavior) {}

TestPrintingContext::~TestPrintingContext() = default;

void TestPrintingContext::SetDeviceSettings(
    const std::string& device_name,
    std::unique_ptr<PrintSettings> settings) {
  device_settings_.emplace(device_name, std::move(settings));
}

void TestPrintingContext::SetUserSettings(const PrintSettings& settings) {
  user_settings_ = settings;
}

void TestPrintingContext::AskUserForSettings(int max_pages,
                                             bool has_selection,
                                             bool is_scripted,
                                             PrintSettingsCallback callback) {
  std::move(callback).Run(
      AskUserForSettingsImpl(max_pages, has_selection, is_scripted));
}

mojom::ResultCode TestPrintingContext::AskUserForSettingsImpl(
    int max_pages,
    bool has_selection,
    bool is_scripted) {
  // Do not actually ask the user with a dialog, just pretend like user
  // made some kind of interaction.
  if (ask_user_for_settings_cancel_) {
    // Pretend the user hit the Cancel button.
    return mojom::ResultCode::kCanceled;
  }

  // Allow for test-specific user modifications.
  if (user_settings_.has_value()) {
    *settings_ = *user_settings_;
  } else {
    // Pretend the user selected the default printer and used the default
    // settings for it.
    scoped_refptr<PrintBackend> print_backend =
        PrintBackend::CreateInstance(/*locale=*/std::string());
    std::string printer_name;
    if (print_backend->GetDefaultPrinterName(printer_name) !=
        mojom::ResultCode::kSuccess) {
      return mojom::ResultCode::kFailed;
    }
    auto found = device_settings_.find(printer_name);
    if (found == device_settings_.end()) {
      return mojom::ResultCode::kFailed;
    }
    settings_ = std::make_unique<PrintSettings>(*found->second);
  }

  // Capture a snapshot, simluating changes made to platform device context.
  applied_settings_ = *settings_;

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintingContext::UseDefaultSettings() {
  if (use_default_settings_fails_)
    return mojom::ResultCode::kFailed;

  scoped_refptr<PrintBackend> print_backend =
      PrintBackend::CreateInstance(/*locale=*/std::string());
  std::string printer_name;
  mojom::ResultCode result = print_backend->GetDefaultPrinterName(printer_name);
  if (result != mojom::ResultCode::kSuccess)
    return result;
  auto found = device_settings_.find(printer_name);
  if (found == device_settings_.end())
    return mojom::ResultCode::kFailed;
  settings_ = std::make_unique<PrintSettings>(*found->second);

  // Capture a snapshot, simluating changes made to platform device context.
  applied_settings_ = *settings_;

  return mojom::ResultCode::kSuccess;
}

gfx::Size TestPrintingContext::GetPdfPaperSizeDeviceUnits() {
  // Default to A4 paper size, which is an alternative to Letter size that is
  // often used as the fallback size for some platform-specific
  // implementations.
  return gfx::Size(kA4WidthInch * settings_->device_units_per_inch(),
                   kA4HeightInch * settings_->device_units_per_inch());
}

mojom::ResultCode TestPrintingContext::UpdatePrinterSettings(
    const PrinterSettings& printer_settings) {
  DCHECK(!in_print_job_);

  // The printer name is to be embedded in the printing context's existing
  // settings.
  const std::string& device_name = base::UTF16ToUTF8(settings_->device_name());
  auto found = device_settings_.find(device_name);
  if (found == device_settings_.end()) {
    DLOG(ERROR) << "No such device found in test printing context: `"
                << device_name << "`";
    return mojom::ResultCode::kFailed;
  }

  // Perform some initialization, akin to various platform-specific actions in
  // `InitPrintSettings()`.
  DVLOG(1) << "Updating context settings for device `" << device_name << "`";
  std::unique_ptr<PrintSettings> existing_settings = std::move(settings_);
  settings_ = std::make_unique<PrintSettings>(*found->second);
  settings_->set_dpi(existing_settings->dpi());
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  for (const auto& item : existing_settings->advanced_settings())
    settings_->advanced_settings().emplace(item.first, item.second.Clone());
#endif

#if BUILDFLAG(IS_WIN)
  if (printer_settings.show_system_dialog) {
    return AskUserForSettingsImpl(printer_settings.page_count,
                                  /*has_selection=*/false,
                                  /*is_scripted=*/false);
  }
#endif

#if BUILDFLAG(IS_MAC)
  destination_is_preview_ = printer_settings.external_preview;
#endif

  // Capture a snapshot, simluating changes made to platform device context.
  applied_settings_ = *settings_;

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintingContext::NewDocument(
    const std::u16string& document_name) {
  DCHECK(!in_print_job_);

#if BUILDFLAG(ENABLE_OOP_PRINTING_NO_OOP_BASIC_PRINT_DIALOG)
  if (process_behavior() == ProcessBehavior::kOopEnabledPerformSystemCalls &&
      !settings_->system_print_dialog_data().empty()) {
    // Mimic the update when system print dialog settings are provided to
    // Print Backend service from the browser process.
    applied_settings_ = *settings_;
  }
#endif

  if (on_new_document_callback_) {
    on_new_document_callback_.Run(
#if BUILDFLAG(IS_MAC)
        destination_is_preview_,
#endif
        applied_settings_);
  }

  abort_printing_ = false;
  in_print_job_ = true;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  const bool make_system_calls =
      process_behavior() != ProcessBehavior::kOopEnabledSkipSystemCalls;
#else
  const bool make_system_calls = true;
#endif
  if (make_system_calls) {
    if (new_document_cancels_) {
      return mojom::ResultCode::kCanceled;
    }
    if (new_document_fails_)
      return mojom::ResultCode::kFailed;
    if (new_document_blocked_by_permissions_)
      return mojom::ResultCode::kAccessDenied;
  }

  // No-op.
  return mojom::ResultCode::kSuccess;
}

#if BUILDFLAG(IS_WIN)
mojom::ResultCode TestPrintingContext::RenderPage(const PrintedPage& page,
                                                  const PageSetup& page_setup) {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  DVLOG(1) << "Render page " << page.page_number();

  if (render_page_blocked_by_permissions_)
    return mojom::ResultCode::kAccessDenied;

  if (render_page_fail_for_page_number_.has_value() &&
      *render_page_fail_for_page_number_ == page.page_number()) {
    return mojom::ResultCode::kFailed;
  }

  // No-op.
  return mojom::ResultCode::kSuccess;
}
#endif  // BUILDFLAG(IS_WIN)

mojom::ResultCode TestPrintingContext::PrintDocument(
    const MetafilePlayer& metafile,
    const PrintSettings& settings,
    uint32_t num_pages) {
  if (abort_printing_)
    return mojom::ResultCode::kCanceled;
  DCHECK(in_print_job_);
  DVLOG(1) << "Print document";

  if (render_document_blocked_by_permissions_)
    return mojom::ResultCode::kAccessDenied;

  // No-op.
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintingContext::DocumentDone() {
  DCHECK(in_print_job_);
  DVLOG(1) << "Document done";

  if (document_done_blocked_by_permissions_)
    return mojom::ResultCode::kAccessDenied;

  ResetSettings();
  return mojom::ResultCode::kSuccess;
}

void TestPrintingContext::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
  DVLOG(1) << "Canceling print job";
}
void TestPrintingContext::ReleaseContext() {}

printing::NativeDrawingContext TestPrintingContext::context() const {
  // No native context for test.
  return nullptr;
}

#if BUILDFLAG(IS_WIN)
mojom::ResultCode TestPrintingContext::InitWithSettingsForTest(
    std::unique_ptr<PrintSettings> settings) {
  NOTIMPLEMENTED();
  return mojom::ResultCode::kFailed;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace printing
