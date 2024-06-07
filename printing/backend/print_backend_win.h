// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_WIN_H_
#define PRINTING_BACKEND_PRINT_BACKEND_WIN_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "printing/backend/print_backend.h"

namespace printing {

class COMPONENT_EXPORT(PRINT_BACKEND) PrintBackendWin : public PrintBackend {
 public:
  PrintBackendWin();

  // PrintBackend implementation.
  mojom::ResultCode EnumeratePrinters(PrinterList& printer_list) override;
  mojom::ResultCode GetDefaultPrinterName(
      std::string& default_printer) override;
  mojom::ResultCode GetPrinterBasicInfo(
      const std::string& printer_name,
      PrinterBasicInfo* printer_info) override;
  mojom::ResultCode GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override;
  mojom::ResultCode GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) override;
  std::optional<gfx::Rect> GetPaperPrintableArea(
      const std::string& printer_name,
      const std::string& paper_vendor_id,
      const gfx::Size& paper_size_um) override;
  std::vector<std::string> GetPrinterDriverInfo(
      const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

  // Allow testing to install a hook to be notified whenever the printable area
  // is requested from the operating system.
  void SetPrintableAreaLoadedCallbackForTesting(
      base::RepeatingClosure callback);

 protected:
  ~PrintBackendWin() override;

 private:
  base::RepeatingClosure printable_area_loaded_callback_for_test_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_WIN_H_
