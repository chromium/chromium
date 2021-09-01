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

PrintingContext::Result TestPrintingContext::UseDefaultSettings() {
  NOTIMPLEMENTED();
  return PrintingContext::FAILED;
}

gfx::Size TestPrintingContext::GetPdfPaperSizeDeviceUnits() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

PrintingContext::Result TestPrintingContext::UpdatePrinterSettings(
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
    return PrintingContext::FAILED;
  }

  DVLOG(1) << "Updating context settings for device `" << device_name << "`";
  PrintSettings* settings = found->second.get();
  settings_ = std::make_unique<PrintSettings>(*settings);
  return PrintingContext::OK;
}

PrintingContext::Result TestPrintingContext::NewDocument(
    const std::u16string& document_name) {
  // No-op.
  return PrintingContext::Result::OK;
}

PrintingContext::Result TestPrintingContext::NewPage() {
  NOTIMPLEMENTED();
  return PrintingContext::Result::FAILED;
}

PrintingContext::Result TestPrintingContext::PageDone() {
  NOTIMPLEMENTED();
  return PrintingContext::Result::FAILED;
}

PrintingContext::Result TestPrintingContext::DocumentDone() {
  NOTIMPLEMENTED();
  return PrintingContext::Result::FAILED;
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
PrintingContext::Result TestPrintingContext::InitWithSettingsForTest(
    std::unique_ptr<PrintSettings> settings) {
  NOTIMPLEMENTED();
  return PrintingContext::Result::FAILED;
}
#endif  // defined(OS_WIN)

}  // namespace printing
