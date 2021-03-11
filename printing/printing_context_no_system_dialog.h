// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
#define PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_

#include <string>

#include "printing/printing_context.h"

namespace printing {

class PRINTING_EXPORT PrintingContextNoSystemDialog : public PrintingContext {
 public:
  explicit PrintingContextNoSystemDialog(Delegate* delegate);
  PrintingContextNoSystemDialog(const PrintingContextNoSystemDialog&) = delete;
  PrintingContextNoSystemDialog& operator=(
      const PrintingContextNoSystemDialog&) = delete;
  ~PrintingContextNoSystemDialog() override;

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  Result UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  Result UpdatePrinterSettings(bool external_preview,
                               bool show_system_dialog,
                               int page_count) override;
  Result NewDocument(const std::u16string& document_name) override;
  Result NewPage() override;
  Result PageDone() override;
  Result DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
