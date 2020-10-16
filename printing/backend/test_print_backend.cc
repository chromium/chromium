// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/test_print_backend.h"

#include <memory>
#include <utility>

#include "base/stl_util.h"
#include "printing/backend/print_backend.h"

namespace printing {

TestPrintBackend::TestPrintBackend() : PrintBackend(/*locale=*/std::string()) {}

TestPrintBackend::~TestPrintBackend() = default;

bool TestPrintBackend::EnumeratePrinters(PrinterList* printer_list) {
  // TODO(crbug.com/809738) There was never a call to provide a list of
  // printers for the environment, so this would always be empty.
  // This needs to be updated as part of a larger cleanup to make
  // TestPrintBackend have consistent behavior across its APIs.
  return false;
}

std::string TestPrintBackend::GetDefaultPrinterName() {
  return default_printer_name_;
}

bool TestPrintBackend::GetPrinterBasicInfo(const std::string& printer_name,
                                           PrinterBasicInfo* printer_info) {
  NOTREACHED() << "Not implemented";
  return false;
}

bool TestPrintBackend::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  auto found = valid_printers_.find(printer_name);
  if (found == valid_printers_.end())
    return false;

  const std::unique_ptr<PrinterSemanticCapsAndDefaults>& caps = found->second;
  if (!caps)
    return false;

  *printer_info = *(found->second);
  return true;
}

bool TestPrintBackend::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  // not implemented
  return false;
}

std::string TestPrintBackend::GetPrinterDriverInfo(
    const std::string& printr_name) {
  // not implemented
  return "";
}

bool TestPrintBackend::IsValidPrinter(const std::string& printer_name) {
  return base::Contains(valid_printers_, printer_name);
}

void TestPrintBackend::SetDefaultPrinterName(const std::string& printer_name) {
  default_printer_name_ = printer_name;
}

void TestPrintBackend::AddValidPrinter(
    const std::string& printer_name,
    std::unique_ptr<PrinterSemanticCapsAndDefaults> caps,
    std::unique_ptr<PrinterBasicInfo> info) {
  // TODO(crbug.com/809738) Utilize the extra `info` parameter to improve
  // TestPrintBackend internal consistency.
  valid_printers_[printer_name] = std::move(caps);
}

}  // namespace printing
