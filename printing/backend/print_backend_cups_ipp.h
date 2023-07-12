// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_CUPS_IPP_H_
#define PRINTING_BACKEND_PRINT_BACKEND_CUPS_IPP_H_

#include <memory>
#include <string>

#include "printing/backend/cups_connection.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"

namespace printing {

class PrintBackendCupsIpp : public PrintBackend {
 public:
  explicit PrintBackendCupsIpp(std::unique_ptr<CupsConnection> connection);

 private:
  ~PrintBackendCupsIpp() override;

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
  std::vector<std::string> GetPrinterDriverInfo(
      const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

  std::unique_ptr<CupsConnection> cups_connection_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_CUPS_IPP_H_
