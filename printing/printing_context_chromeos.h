// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_CHROMEOS_H_
#define PRINTING_PRINTING_CONTEXT_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_helper.h"
#include "printing/backend/cups_printer.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_context.h"

namespace printing {

class COMPONENT_EXPORT(PRINTING) PrintingContextChromeos
    : public PrintingContext {
 public:
  static std::unique_ptr<PrintingContextChromeos> CreateForTesting(
      Delegate* delegate,
      ProcessBehavior process_behavior,
      std::unique_ptr<CupsConnection> connection);

  PrintingContextChromeos(Delegate* delegate, ProcessBehavior process_behavior);
  PrintingContextChromeos(const PrintingContextChromeos&) = delete;
  PrintingContextChromeos& operator=(const PrintingContextChromeos&) = delete;
  ~PrintingContextChromeos() override;

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

  mojom::ResultCode StreamData(const std::vector<char>& buffer);

 private:
  // For testing. Use CreateForTesting() to create.
  PrintingContextChromeos(Delegate* delegate,
                          ProcessBehavior process_behavior,
                          std::unique_ptr<CupsConnection> connection);

  // Lazily initializes `printer_`.
  mojom::ResultCode InitializeDevice(const std::string& device);

  const std::unique_ptr<CupsConnection> connection_;
  std::unique_ptr<CupsPrinter> printer_;
  ScopedIppPtr ipp_options_;
  bool send_user_info_ = false;
  std::string username_;
};

COMPONENT_EXPORT(PRINTING)
ScopedIppPtr SettingsToIPPOptions(const PrintSettings& settings,
                                  gfx::Rect printable_area_um);

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_CHROMEOS_H_
