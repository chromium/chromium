// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_LINUX_H_
#define PRINTING_PRINTING_CONTEXT_LINUX_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "printing/printing_context.h"

namespace printing {

class MetafilePlayer;
class PrintDialogGtkInterface;

// PrintingContext with optional native UI for print dialog and pdf_paper_size.
class PRINTING_EXPORT PrintingContextLinux : public PrintingContext {
 public:
  explicit PrintingContextLinux(Delegate* delegate);
  ~PrintingContextLinux() override;

  // Sets the function that creates the print dialog.
  static void SetCreatePrintDialogFunction(PrintDialogGtkInterface* (
      *create_dialog_func)(PrintingContextLinux* context));

  // Sets the function that returns pdf paper size through the native API.
  static void SetPdfPaperSizeFunction(
      gfx::Size (*get_pdf_paper_size)(PrintingContextLinux* context));

  // Prints the document contained in |metafile|.
  void PrintDocument(const MetafilePlayer& metafile);

  // Initializes with predefined settings.
  void InitWithSettings(std::unique_ptr<PrintSettings> settings);

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  Result UseDefaultSettings() override;
  Result UpdatePrinterSettings(bool external_preview,
                               bool show_system_dialog,
                               int page_count) override;
  Result NewDocument(const base::string16& document_name) override;
  Result NewPage() override;
  Result PageDone() override;
  Result DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;

 private:
  base::string16 document_name_;
  PrintDialogGtkInterface* print_dialog_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextLinux);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_LINUX_H_
