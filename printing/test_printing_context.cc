// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

TestPrintingContextDelegate::TestPrintingContextDelegate() = default;

TestPrintingContextDelegate::~TestPrintingContextDelegate() = default;

gfx::NativeView TestPrintingContextDelegate::GetParentView() {
  return nullptr;
}

std::string TestPrintingContextDelegate::GetAppLocale() {
  return std::string();
}

TestPrintingContext::TestPrintingContext(Delegate* delegate)
    : PrintingContext(delegate) {}

TestPrintingContext::~TestPrintingContext() = default;

void TestPrintingContext::SetDeviceSettings(
    const std::string& device_name,
    std::unique_ptr<PrintSettings> settings) {
  device_settings_.emplace(device_name, std::move(settings));
}

void TestPrintingContext::AskUserForSettings(int max_pages,
                                             bool has_selection,
                                             bool is_scripted,
                                             PrintSettingsCallback callback) {
  NOTIMPLEMENTED();
}

mojom::ResultCode TestPrintingContext::UseDefaultSettings() {
  NOTIMPLEMENTED();
  return mojom::ResultCode::kFailed;
}

gfx::Size TestPrintingContext::GetPdfPaperSizeDeviceUnits() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

mojom::ResultCode TestPrintingContext::UpdatePrinterSettings(
    bool external_preview,
    bool show_system_dialog,
    int page_count) {
  DCHECK(!in_print_job_);
  DCHECK(!external_preview) << "Not implemented";
  DCHECK(!show_system_dialog) << "Not implemented";

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
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  for (const auto& item : existing_settings->advanced_settings())
    settings_->advanced_settings().emplace(item.first, item.second.Clone());
#endif

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintingContext::NewDocument(
    const std::u16string& document_name) {
  // No-op.
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintingContext::NewPage() {
  NOTIMPLEMENTED();
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode TestPrintingContext::PageDone() {
  NOTIMPLEMENTED();
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode TestPrintingContext::DocumentDone() {
  NOTIMPLEMENTED();
  return mojom::ResultCode::kFailed;
}

void TestPrintingContext::Cancel() {
  NOTIMPLEMENTED();
}
void TestPrintingContext::ReleaseContext() {}

printing::NativeDrawingContext TestPrintingContext::context() const {
  NOTIMPLEMENTED();
  return nullptr;
}

#if defined(OS_WIN)
mojom::ResultCode TestPrintingContext::InitWithSettingsForTest(
    std::unique_ptr<PrintSettings> settings) {
  NOTIMPLEMENTED();
  return mojom::ResultCode::kFailed;
}
#endif  // defined(OS_WIN)

}  // namespace printing
