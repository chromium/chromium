// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"

#if BUILDFLAG(USE_CUPS)
#include "printing/backend/cups_connection.h"
#include "printing/backend/print_backend_cups_ipp.h"
#endif  // BUILDFLAG(USE_CUPS)

namespace printing {

// Provides either a stubbed out PrintBackend implementation or a CUPS IPP
// implementation for use on ChromeOS.
class PrintBackendChromeOS : public PrintBackend {
 public:
  PrintBackendChromeOS() = default;

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

 protected:
  ~PrintBackendChromeOS() override = default;
};

mojom::ResultCode PrintBackendChromeOS::EnumeratePrinters(
    PrinterList& printer_list) {
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendChromeOS::GetPrinterBasicInfo(
    const std::string& printer_name,
    PrinterBasicInfo* printer_info) {
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode PrintBackendChromeOS::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  NOTREACHED();
}

std::vector<std::string> PrintBackendChromeOS::GetPrinterDriverInfo(
    const std::string& printer_name) {
  NOTREACHED();
}

mojom::ResultCode PrintBackendChromeOS::GetDefaultPrinterName(
    std::string& default_printer) {
  default_printer = std::string();
  return mojom::ResultCode::kSuccess;
}

bool PrintBackendChromeOS::IsValidPrinter(const std::string& printer_name) {
  NOTREACHED();
}

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
    const std::string& /*locale*/) {
#if BUILDFLAG(USE_CUPS)
  return base::MakeRefCounted<PrintBackendCupsIpp>(CupsConnection::Create());
#else
  return base::MakeRefCounted<PrintBackendChromeOS>();
#endif  // BUILDFLAG(USE_CUPS)
}

}  // namespace printing
