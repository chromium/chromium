// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_TEST_PRINTING_CONTEXT_H_
#define PRINTING_TEST_PRINTING_CONTEXT_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
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

// Note that TestPrintingContext uses a PrintBackend internally, so tests that
// want to avoid using a real PrintingContext will also want to use
// PrintBackend::SetPrintBackendForTesting() to avoid using a real PrintBackend.
class TestPrintingContext : public PrintingContext {
 public:
  using OnNewDocumentCallback = base::RepeatingCallback<void(
#if BUILDFLAG(IS_MAC)
      bool destination_is_preview,
#endif
      const PrintSettings&)>;

  TestPrintingContext(Delegate* delegate, ProcessBehavior process_behavior);
  TestPrintingContext(const TestPrintingContext&) = delete;
  TestPrintingContext& operator=(const TestPrintingContext&) = delete;
  ~TestPrintingContext() override;

  // Methods for test setup:

  // Provide settings that will be used as the current settings for the
  // indicated device.
  void SetDeviceSettings(const std::string& device_name,
                         std::unique_ptr<PrintSettings> settings);

  // Provide the job ID which should be used once a new document is created.
  // Only applicable for process behaviors that can make system calls.
  void SetNewDocumentJobId(int job_id);

  // Provide the settings which should be applied to mimic a user's choices
  // during AskUserForSettings().
  void SetUserSettings(const PrintSettings& settings);

  // Enables tests to fail with an access-denied error.
  void SetNewDocumentBlockedByPermissions() {
    new_document_blocked_by_permissions_ = true;
  }
#if BUILDFLAG(IS_WIN)
  void SetOnRenderPageBlockedByPermissions() {
    render_page_blocked_by_permissions_ = true;
  }
  void SetOnRenderPageFailsForPage(uint32_t page_number) {
    render_page_fail_for_page_number_ = page_number;
  }
#endif
  void SetOnRenderDocumentBlockedByPermissions() {
    render_document_blocked_by_permissions_ = true;
  }
  void SetDocumentDoneBlockedByPermissions() {
    document_done_blocked_by_permissions_ = true;
  }

  // Enables tests to fail with a failed error.
  void SetNewDocumentFails() { new_document_fails_ = true; }
  void SetUpdatePrinterSettingsFails() {
    update_printer_settings_fails_ = true;
  }
  void SetUseDefaultSettingsFails() { use_default_settings_fails_ = true; }
  void SetAskUserForSettingsFails() { ask_user_for_settings_fails_ = true; }

  // Enables tests to fail with a canceled error.
  void SetNewDocumentCancels() { new_document_cancels_ = true; }
  void SetAskUserForSettingsCanceled() { ask_user_for_settings_cancel_ = true; }

  void SetOnNewDocumentCallback(OnNewDocumentCallback callback) {
    on_new_document_callback_ = std::move(callback);
  }

  // PrintingContext overrides:
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  mojom::ResultCode UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  mojom::ResultCode UpdatePrinterSettings(
      const PrinterSettings& printer_settings) override;
  mojom::ResultCode NewDocument(const std::u16string& document_name) override;
#if BUILDFLAG(IS_WIN)
  mojom::ResultCode RenderPage(const PrintedPage& page,
                               const PageSetup& page_setup) override;
#endif
  mojom::ResultCode PrintDocument(const MetafilePlayer& metafile,
                                  const PrintSettings& settings,
                                  uint32_t num_pages) override;
  mojom::ResultCode DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  NativeDrawingContext context() const override;
#if BUILDFLAG(IS_WIN)
  mojom::ResultCode InitWithSettingsForTest(
      std::unique_ptr<PrintSettings> settings) override;
#endif

 private:
  mojom::ResultCode AskUserForSettingsImpl(int max_pages,
                                           bool has_selection,
                                           bool is_scripted);

  // Simulation of platform drivers' default settings.
  base::flat_map<std::string, std::unique_ptr<PrintSettings>> device_settings_;

  // Settings to apply to mimic a user's choices in `AskUserForSettings()`.
  std::optional<PrintSettings> user_settings_;

  // Platform implementations of `PrintingContext` apply PrintSettings to the
  // respective device contexts.  Once the upper printing layers call
  // `TakeAndResetSettings()`, the values in `settings_` no longer reflect
  // what the printer driver's device context are set for.
  // Simulate this by capturing what the settings are whenever `settings_` is
  // applied to a device context.
  PrintSettings applied_settings_;

#if BUILDFLAG(IS_MAC)
  // When printing a new document, Preview app is a special macOS destination.
  // This member is used to track when this was indicated as the destination to
  // use in `UpdatePrinterSettings()`.
  bool destination_is_preview_ = false;
#endif

  bool update_printer_settings_fails_ = false;
  bool use_default_settings_fails_ = false;
  bool ask_user_for_settings_cancel_ = false;
  bool ask_user_for_settings_fails_ = false;
  bool new_document_cancels_ = false;
  bool new_document_fails_ = false;
  bool new_document_blocked_by_permissions_ = false;
#if BUILDFLAG(IS_WIN)
  bool render_page_blocked_by_permissions_ = false;
  std::optional<uint32_t> render_page_fail_for_page_number_;
#endif
  bool render_document_blocked_by_permissions_ = false;
  bool document_done_blocked_by_permissions_ = false;

  // Called every time `NewDocument()` is called.  Provides a copy of the
  // effective device context settings.
  OnNewDocumentCallback on_new_document_callback_;

  // The job ID to assign once `NewDocument()` is called, if the process
  // behavior allows for system calls to be made.
  std::optional<int> new_document_job_id_;
};

}  // namespace printing

#endif  // PRINTING_TEST_PRINTING_CONTEXT_H_
