// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_CUPS_IPP_H_
#define PRINTING_BACKEND_PRINT_BACKEND_CUPS_IPP_H_

#include <memory>
#include <string>

#include "printing/backend/cups_connection.h"
#include "printing/backend/print_backend.h"

namespace printing {

class PrintBackendCupsIpp : public PrintBackend {
 public:
  explicit PrintBackendCupsIpp(std::unique_ptr<CupsConnection> connection);

 private:
  ~PrintBackendCupsIpp() override;

  // PrintBackend implementation.
  bool EnumeratePrinters(PrinterList* printer_list) override;
  std::string GetDefaultPrinterName() override;
  bool GetPrinterBasicInfo(const std::string& printer_name,
                           PrinterBasicInfo* printer_info) override;
  bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override;
  std::string GetPrinterDriverInfo(const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

  std::unique_ptr<CupsConnection> cups_connection_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_CUPS_IPP_H_
