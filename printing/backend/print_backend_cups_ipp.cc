// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_cups_ipp.h"

#include <cups/cups.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_helper.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/mojom/print.mojom.h"
#include "printing/units.h"

namespace printing {

PrintBackendCupsIpp::PrintBackendCupsIpp(
    std::unique_ptr<CupsConnection> cups_connection,
    const std::string& locale)
    : PrintBackend(locale), cups_connection_(std::move(cups_connection)) {}

PrintBackendCupsIpp::~PrintBackendCupsIpp() = default;

mojom::ResultCode PrintBackendCupsIpp::EnumeratePrinters(
    PrinterList* printer_list) {
  DCHECK(printer_list);
  printer_list->clear();

  std::vector<std::unique_ptr<CupsPrinter>> printers =
      cups_connection_->GetDests();
  if (printers.empty()) {
    // No destinations could mean the operation failed or that there are simply
    // no printer drivers installed.  Rely upon CUPS error code to distinguish
    // between these.
    const int last_error = cups_connection_->last_error();
    if (last_error != IPP_STATUS_ERROR_NOT_FOUND) {
      LOG(WARNING) << "CUPS: Error getting printers from CUPS server"
                   << ", server: " << cups_connection_->server_name()
                   << ", error: " << last_error << " - "
                   << cups_connection_->last_error_message();
      return mojom::ResultCode::kFailed;
    }
    VLOG(1) << "CUPS: No printers found for CUPS server: "
            << cups_connection_->server_name();
    return mojom::ResultCode::kSuccess;
  }

  VLOG(1) << "CUPS: found " << printers.size()
          << " printers from CUPS server: " << cups_connection_->server_name();
  for (const auto& printer : printers) {
    PrinterBasicInfo basic_info;
    if (printer->ToPrinterInfo(&basic_info)) {
      printer_list->push_back(basic_info);
    }
  }

  return mojom::ResultCode::kSuccess;
}

std::string PrintBackendCupsIpp::GetDefaultPrinterName() {
  std::vector<std::unique_ptr<CupsPrinter>> printers =
      cups_connection_->GetDests();
  for (const auto& printer : printers) {
    if (printer->is_default()) {
      return printer->GetName();
    }
  }

  return std::string();
}

mojom::ResultCode PrintBackendCupsIpp::GetPrinterBasicInfo(
    const std::string& printer_name,
    PrinterBasicInfo* printer_info) {
  std::unique_ptr<CupsPrinter> printer(
      cups_connection_->GetPrinter(printer_name));
  if (!printer)
    return mojom::ResultCode::kFailed;

  DCHECK_EQ(printer_name, printer->GetName());

  return printer->ToPrinterInfo(printer_info) ? mojom::ResultCode::kSuccess
                                              : mojom::ResultCode::kFailed;
}

mojom::ResultCode PrintBackendCupsIpp::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  NOTREACHED();
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode PrintBackendCupsIpp::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  std::unique_ptr<CupsPrinter> printer(
      cups_connection_->GetPrinter(printer_name));
  if (!printer || !printer->EnsureDestInfo())
    return mojom::ResultCode::kFailed;

  CapsAndDefaultsFromPrinter(*printer, printer_info);

  return mojom::ResultCode::kSuccess;
}

std::string PrintBackendCupsIpp::GetPrinterDriverInfo(
    const std::string& printer_name) {
  std::unique_ptr<CupsPrinter> printer(
      cups_connection_->GetPrinter(printer_name));
  if (!printer)
    return std::string();

  DCHECK_EQ(printer_name, printer->GetName());
  return printer->GetMakeAndModel();
}

bool PrintBackendCupsIpp::IsValidPrinter(const std::string& printer_name) {
  return !!cups_connection_->GetPrinter(printer_name);
}

}  // namespace printing
