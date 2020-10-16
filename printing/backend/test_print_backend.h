// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_TEST_PRINT_BACKEND_H_
#define PRINTING_BACKEND_TEST_PRINT_BACKEND_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "printing/backend/print_backend.h"

namespace printing {

// PrintBackend which doesn't interact with the OS and responses
// can be overridden as necessary.
class TestPrintBackend : public PrintBackend {
 public:
  TestPrintBackend();

  // PrintBackend overrides
  bool EnumeratePrinters(PrinterList* printer_list) override;
  std::string GetDefaultPrinterName() override;
  bool GetPrinterBasicInfo(const std::string& printer_name,
                           PrinterBasicInfo* printer_info) override;
  bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override;
  bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                 PrinterCapsAndDefaults* printer_info) override;
  std::string GetPrinterDriverInfo(const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

  // Set a default printer.  The default is the empty string.
  void SetDefaultPrinterName(const std::string& printer_name);

  // Add a printer to satisfy IsValidPrinter(), EnumeratePrinters(),
  // GetPrinterBasicInfo(), and GetPrinterSemanticCapsAndDefaults().
  // While `caps` can be null, it will cause queries for the capabilities to
  // fail, and thus is likely not of interest for most tests.  IsValidPrinter()
  // will still show true even if `caps` is null, which provides the benefit of
  // simulating a printer that exists in the system but cannot be queried.
  // `info` can be null, which will result in empty information being provided
  // for any queries.
  // Calling EnumeratePrinters() will include the identified `printer_name`
  // even if either parameter is null.
  void AddValidPrinter(const std::string& printer_name,
                       std::unique_ptr<PrinterSemanticCapsAndDefaults> caps,
                       std::unique_ptr<PrinterBasicInfo> info);

 protected:
  ~TestPrintBackend() override;

 private:
  std::string default_printer_name_;
  std::map<std::string, std::unique_ptr<PrinterSemanticCapsAndDefaults>>
      valid_printers_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_TEST_PRINT_BACKEND_H_
