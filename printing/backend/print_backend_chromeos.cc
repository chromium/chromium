// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "printing/backend/print_backend_consts.h"
#include "url/gurl.h"

#if defined(USE_CUPS)
#include "printing/backend/print_backend_cups_ipp.h"
#endif  // defined(USE_CUPS)

namespace printing {
namespace {

#if defined(USE_CUPS)
std::unique_ptr<CupsConnection> CreateConnection(
    const base::DictionaryValue* print_backend_settings) {
  std::string print_server_url_str;
  std::string cups_blocking;
  int encryption = HTTP_ENCRYPT_NEVER;
  if (print_backend_settings) {
    print_backend_settings->GetString(kCUPSPrintServerURL,
                                      &print_server_url_str);
    print_backend_settings->GetString(kCUPSBlocking, &cups_blocking);
    print_backend_settings->GetInteger(kCUPSEncryption, &encryption);
  }
  GURL print_server_url(print_server_url_str);

  return std::make_unique<CupsConnection>(
      print_server_url, static_cast<http_encryption_t>(encryption),
      cups_blocking == kValueTrue);
}
#endif  // defined(USE_CUPS)

}  // namespace

// Provides either a stubbed out PrintBackend implementation or a CUPS IPP
// implementation for use on ChromeOS.
class PrintBackendChromeOS : public PrintBackend {
 public:
  PrintBackendChromeOS();

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

 protected:
  ~PrintBackendChromeOS() override = default;
};

PrintBackendChromeOS::PrintBackendChromeOS() = default;

bool PrintBackendChromeOS::EnumeratePrinters(PrinterList* printer_list) {
  return true;
}

bool PrintBackendChromeOS::GetPrinterBasicInfo(const std::string& printer_name,
                                               PrinterBasicInfo* printer_info) {
  return false;
}

bool PrintBackendChromeOS::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  NOTREACHED();
  return false;
}

std::string PrintBackendChromeOS::GetPrinterDriverInfo(
    const std::string& printer_name) {
  NOTREACHED();
  return std::string();
}

std::string PrintBackendChromeOS::GetDefaultPrinterName() {
  return std::string();
}

bool PrintBackendChromeOS::IsValidPrinter(const std::string& printer_name) {
  NOTREACHED();
  return true;
}

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceImpl(
    const base::DictionaryValue* print_backend_settings) {
#if defined(USE_CUPS)
  return base::WrapRefCounted(
      new PrintBackendCupsIpp(CreateConnection(print_backend_settings)));
#else
  return base::MakeRefCounted<PrintBackendChromeOS>();
#endif  // defined(USE_CUPS)
}

}  // namespace printing
