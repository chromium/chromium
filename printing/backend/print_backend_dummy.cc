// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is dummy implementation for all configurations where there is no
// print backend.
#if !defined(PRINT_BACKEND_AVAILABLE)

#include "printing/backend/print_backend.h"

#include "printing/mojom/print.mojom.h"

namespace printing {

class DummyPrintBackend : public PrintBackend {
 public:
  DummyPrintBackend() = default;
  DummyPrintBackend(const DummyPrintBackend&) = delete;
  DummyPrintBackend& operator=(const DummyPrintBackend&) = delete;

  mojom::ResultCode EnumeratePrinters(PrinterList& printer_list) override {
    return mojom::ResultCode::kFailed;
  }

  mojom::ResultCode GetDefaultPrinterName(
      std::string& default_printer) override {
    default_printer = std::string();
    return mojom::ResultCode::kSuccess;
  }

  mojom::ResultCode GetPrinterBasicInfo(
      const std::string& printer_name,
      PrinterBasicInfo* printer_info) override {
    return mojom::ResultCode::kFailed;
  }

  mojom::ResultCode GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      PrinterSemanticCapsAndDefaults* printer_info) override {
    return mojom::ResultCode::kFailed;
  }

  std::vector<std::string> GetPrinterDriverInfo(
      const std::string& printer_name) override {
    return std::vector<std::string>();
  }

  bool IsValidPrinter(const std::string& printer_name) override {
    return false;
  }

 private:
  ~DummyPrintBackend() override = default;
};

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
    const std::string& /*locale*/) {
  return base::MakeRefCounted<DummyPrintBackend>();
}

}  // namespace printing

#endif  // PRINT_BACKEND_AVAILABLE
