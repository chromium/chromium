// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is dummy implementation for all configurations where there is no
// print backend.
#if !defined(PRINT_BACKEND_AVAILABLE)

#include "printing/backend/print_backend.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"

namespace printing {

class DummyPrintBackend : public PrintBackend {
 public:
  DummyPrintBackend() {}

  bool EnumeratePrinters(PrinterList* printer_list) override { return false; }

  std::string GetDefaultPrinterName() override { return std::string(); }

  bool GetPrinterBasicInfo(const std::string& printer_name,
                           PrinterBasicInfo* printer_info) override {
    return false;
  }

  bool GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override {
    return false;
  }

  bool GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) override {
    return false;
  }

  std::string GetPrinterDriverInfo(const std::string& printer_name) override {
    return std::string();
  }

  bool IsValidPrinter(const std::string& printer_name) override {
    return false;
  }

 private:
  ~DummyPrintBackend() override {}

  DISALLOW_COPY_AND_ASSIGN(DummyPrintBackend);
};

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
    const base::DictionaryValue* print_backend_settings) {
  return new DummyPrintBackend();
}

}  // namespace printing

#endif  // PRINT_BACKEND_AVAILABLE
