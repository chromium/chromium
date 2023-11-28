// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
#define PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_

#include <string>

#include "base/component_export.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"

namespace printing {

class COMPONENT_EXPORT(PRINTING) PrintingContextNoSystemDialog
    : public PrintingContext {
 public:
  PrintingContextNoSystemDialog(Delegate* delegate,
                                ProcessBehavior process_behavior);
  PrintingContextNoSystemDialog(const PrintingContextNoSystemDialog&) = delete;
  PrintingContextNoSystemDialog& operator=(
      const PrintingContextNoSystemDialog&) = delete;
  ~PrintingContextNoSystemDialog() override;

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
  mojom::ResultCode PrintDocument(const MetafilePlayer& metafile,
                                  const PrintSettings& settings,
                                  uint32_t num_pages) override;
  mojom::ResultCode DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
