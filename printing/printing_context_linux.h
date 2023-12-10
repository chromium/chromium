// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_LINUX_H_
#define PRINTING_PRINTING_CONTEXT_LINUX_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"

namespace printing {

class MetafilePlayer;
class PrintDialogLinuxInterface;

// PrintingContext with optional native UI for print dialog and pdf_paper_size.
class COMPONENT_EXPORT(PRINTING) PrintingContextLinux : public PrintingContext {
 public:
  PrintingContextLinux(Delegate* delegate, ProcessBehavior process_behavior);
  PrintingContextLinux(const PrintingContextLinux&) = delete;
  PrintingContextLinux& operator=(const PrintingContextLinux&) = delete;
  ~PrintingContextLinux() override;

  // Initializes with predefined settings.
  void InitWithSettings(std::unique_ptr<PrintSettings> settings);

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  mojom::ResultCode UseDefaultSettings() override;
  mojom::ResultCode UpdatePrinterSettings(
      const PrinterSettings& printer_settings) override;
  mojom::ResultCode NewDocument(const std::u16string& document_name) override;
  mojom::ResultCode PrintDocument(const MetafilePlayer& metafile,
                                  const PrintSettings& settings,
                                  uint32_t num_pages) override;
  mojom::ResultCode DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;

 private:
  std::u16string document_name_;
  raw_ptr<PrintDialogLinuxInterface> print_dialog_;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_LINUX_H_
