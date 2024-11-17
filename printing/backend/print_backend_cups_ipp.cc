// Copyright 2016 The Chromium Authors
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
#include "printing/backend/cups_weak_functions.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/mojom/print.mojom.h"
#include "printing/units.h"

namespace printing {

PrintBackendCupsIpp::PrintBackendCupsIpp(
    std::unique_ptr<CupsConnection> cups_connection)
    : cups_connection_(std::move(cups_connection)) {}

PrintBackendCupsIpp::~PrintBackendCupsIpp() = default;

mojom::ResultCode PrintBackendCupsIpp::EnumeratePrinters(
    PrinterList& printer_list) {
  DCHECK(printer_list.empty());

  std::vector<std::unique_ptr<CupsPrinter>> printers;
  if (!cups_connection_->GetDests(printers)) {
    LOG(WARNING) << "CUPS: Error getting printers from CUPS server"
                 << ", error: " << cups_connection_->last_error() << " - "
                 << cups_connection_->last_error_message();
    return mojom::ResultCode::kFailed;
  }

  VLOG(1) << "CUPS: found " << printers.size() << " printers from CUPS server.";
  for (const auto& printer : printers) {
    PrinterBasicInfo basic_info;
    if (printer->ToPrinterInfo(&basic_info)) {
      printer_list.push_back(basic_info);
    }
  }

  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode PrintBackendCupsIpp::GetDefaultPrinterName(
    std::string& default_printer) {
  std::vector<std::unique_ptr<CupsPrinter>> printers;
  if (!cups_connection_->GetDests(printers)) {
    LOG(ERROR) << "CUPS: unable to get default printer: "
               << cupsLastErrorString();
    return mojom::ResultCode::kFailed;
  }

  for (const auto& printer : printers) {
    if (printer->is_default()) {
      default_printer = printer->GetName();
      return mojom::ResultCode::kSuccess;
    }
  }

  default_printer = std::string();
  return mojom::ResultCode::kSuccess;
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

std::vector<std::string> PrintBackendCupsIpp::GetPrinterDriverInfo(
    const std::string& printer_name) {
  std::vector<std::string> result;
  std::unique_ptr<CupsPrinter> printer(
      cups_connection_->GetPrinter(printer_name));
  if (!printer)
    return result;

  DCHECK_EQ(printer_name, printer->GetName());
  result.emplace_back(printer->GetInfo());
  result.emplace_back(printer->GetMakeAndModel());
  result.emplace_back(cupsUserAgent());
  return result;
}

bool PrintBackendCupsIpp::IsValidPrinter(const std::string& printer_name) {
  return !!cups_connection_->GetPrinter(printer_name);
}

}  // namespace printing
