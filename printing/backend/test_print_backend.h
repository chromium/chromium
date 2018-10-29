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
#if !defined(OS_CHROMEOS)
  bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                 PrinterCapsAndDefaults* printer_info) override;
#endif  // !defined(OS_CHROMEOS)
  std::string GetPrinterDriverInfo(const std::string& printer_name) override;
  bool IsValidPrinter(const std::string& printer_name) override;

  // Methods for test setup.
  // Add printers to the list returned by EnumeratePrinters.
  void PopulatePrinterList(const PrinterList& printer_list);

  // Set a default printer.  The default is the empty string.
  void SetDefaultPrinterName(const std::string& printer_name);

  // Add a printer to satisfy IsValidPrinter and
  // GetPrinterSemanticCapsAndDefualts.
  void AddValidPrinter(const std::string& printer_name,
                       std::unique_ptr<PrinterSemanticCapsAndDefaults> caps);

 private:
  ~TestPrintBackend() override;

  std::string default_printer_name_;
  PrinterList printer_list_;
  std::map<std::string, std::unique_ptr<PrinterSemanticCapsAndDefaults>>
      valid_printers_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_TEST_PRINT_BACKEND_H_
