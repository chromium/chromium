// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_WIN_H_
#define PRINTING_PRINTING_CONTEXT_WIN_H_

#include <windows.h>

#include <memory>
#include <string>

#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class PrintSettings;

class COMPONENT_EXPORT(PRINTING) PrintingContextWin : public PrintingContext {
 public:
  PrintingContextWin(Delegate* delegate, ProcessBehavior process_behavior);
  PrintingContextWin(const PrintingContextWin&) = delete;
  PrintingContextWin& operator=(const PrintingContextWin&) = delete;
  ~PrintingContextWin() override;

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  mojom::ResultCode UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  mojom::ResultCode UpdatePrinterSettings(
      const PrinterSettings& printer_settings) override;
  mojom::ResultCode NewDocument(const std::u16string& document_name) override;
  mojom::ResultCode RenderPage(const PrintedPage& page,
                               const PageSetup& page_setup) override;
  mojom::ResultCode PrintDocument(const MetafilePlayer& metafile,
                                  const PrintSettings& settings,
                                  uint32_t num_pages) override;
  mojom::ResultCode DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;
  mojom::ResultCode InitWithSettingsForTest(
      std::unique_ptr<PrintSettings> settings) override;

 protected:
  static HWND GetRootWindow(gfx::NativeView view);

  // Reads the settings from the selected device context. Updates settings_ and
  // its margins.
  virtual mojom::ResultCode InitializeSettings(const std::wstring& device_name,
                                               DEVMODE* dev_mode);

  // PrintingContext implementation.
  mojom::ResultCode OnError() override;

  void set_context(HDC context) { context_ = context; }

 private:
  // Used in response to the user canceling the printing.
  static BOOL CALLBACK AbortProc(HDC hdc, int nCode);

  // The selected printer context.
  HDC context_;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_WIN_H_
