// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_TEST_PRINTING_CONTEXT_H_
#define PRINTING_TEST_PRINTING_CONTEXT_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printing_context.h"

namespace printing {

class TestPrintingContextDelegate : public PrintingContext::Delegate {
 public:
  TestPrintingContextDelegate();
  TestPrintingContextDelegate(const TestPrintingContextDelegate&) = delete;
  TestPrintingContextDelegate& operator=(const TestPrintingContextDelegate&) =
      delete;
  ~TestPrintingContextDelegate() override;

  // PrintingContext::Delegate overrides:
  gfx::NativeView GetParentView() override;
  std::string GetAppLocale() override;
};

class TestPrintingContext : public PrintingContext {
 public:
  explicit TestPrintingContext(Delegate* delegate);
  TestPrintingContext(const TestPrintingContext&) = delete;
  TestPrintingContext& operator=(const TestPrintingContext&) = delete;
  ~TestPrintingContext() override;

  // Methods for test setup:

  // Provide settings that will be used as the current settings for the
  // indicated device.
  void SetDeviceSettings(const std::string& device_name,
                         std::unique_ptr<PrintSettings> settings);

  // PrintingContext overrides:
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  mojom::ResultCode UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  mojom::ResultCode UpdatePrinterSettings(bool external_preview,
                                          bool show_system_dialog,
                                          int page_count) override;
  mojom::ResultCode NewDocument(const std::u16string& document_name) override;
  mojom::ResultCode NewPage() override;
  mojom::ResultCode PageDone() override;
  mojom::ResultCode DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  NativeDrawingContext context() const override;
#if defined(OS_WIN)
  mojom::ResultCode InitWithSettingsForTest(
      std::unique_ptr<PrintSettings> settings) override;
#endif

 private:
  base::flat_map<std::string, std::unique_ptr<PrintSettings>> device_settings_;
};

}  // namespace printing

#endif  // PRINTING_TEST_PRINTING_CONTEXT_H_
