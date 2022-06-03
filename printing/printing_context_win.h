// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

class MetafileSkia;
class PrintSettings;

class COMPONENT_EXPORT(PRINTING) PrintingContextWin : public PrintingContext {
 public:
  explicit PrintingContextWin(Delegate* delegate);
  PrintingContextWin(const PrintingContextWin&) = delete;
  PrintingContextWin& operator=(const PrintingContextWin&) = delete;
  ~PrintingContextWin() override;

  // Prints the document contained in `metafile`.
  void PrintDocument(const std::wstring& device_name,
                     const MetafileSkia& metafile);

  // PrintingContext implementation.
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
