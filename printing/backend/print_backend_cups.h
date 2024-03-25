// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_CUPS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_CUPS_H_

#include <cups/cups.h>

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "printing/backend/cups_deleters.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "url/gurl.h"

namespace printing {

class PrintBackendCUPS : public PrintBackend {
 public:
  PrintBackendCUPS(const GURL& print_server_url,
                   http_encryption_t encryption,
                   bool blocking,
                   const std::string& locale);

  // These static functions are exposed here for use in the tests.
  COMPONENT_EXPORT(PRINT_BACKEND)
  static mojom::ResultCode PrinterBasicInfoFromCUPS(
      const cups_dest_t& printer,
      PrinterBasicInfo* printer_info);
  COMPONENT_EXPORT(PRINT_BACKEND)
  static std::string PrinterDriverInfoFromCUPS(const cups_dest_t& printer);

 private:
  ~PrintBackendCUPS() override;

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

  std::string GetPrinterCapabilities(const std::string& printer_name);

  // The following functions are wrappers around corresponding CUPS functions.
  // <functions>2() are called when print server is specified, and plain version
  // in another case. There is an issue specifying CUPS_HTTP_DEFAULT in the
  // functions>2(), it does not work in CUPS prior to 1.4.
  int GetDests(cups_dest_t** dests);
  base::FilePath GetPPD(const char* name);

  // Wrapper around cupsGetNamedDest().
  ScopedDestination GetNamedDest(const std::string& printer_name);

  const std::string locale_;
  const GURL print_server_url_;
  http_encryption_t cups_encryption_;
  const bool blocking_;
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_CUPS_H_
