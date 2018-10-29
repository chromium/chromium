// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/test_print_backend.h"

#include <memory>
#include <utility>

#include "base/stl_util.h"
#include "printing/backend/print_backend.h"

namespace printing {

TestPrintBackend::TestPrintBackend() = default;

TestPrintBackend::~TestPrintBackend() = default;

bool TestPrintBackend::EnumeratePrinters(PrinterList* printer_list) {
  if (printer_list_.empty())
    return false;

  printer_list->insert(printer_list->end(), printer_list_.begin(),
                       printer_list_.end());
  return true;
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

#if !defined(OS_CHROMEOS)
bool TestPrintBackend::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  // not implemented
  return false;
}
#endif  // !defined(OS_CHROMEOS)

std::string TestPrintBackend::GetPrinterDriverInfo(
    const std::string& printr_name) {
  // not implemented
  return "";
}

bool TestPrintBackend::IsValidPrinter(const std::string& printer_name) {
  return base::ContainsKey(valid_printers_, printer_name);
}

void TestPrintBackend::PopulatePrinterList(const PrinterList& printer_list) {
  printer_list_.insert(printer_list_.end(), printer_list.begin(),
                       printer_list.end());
}

void TestPrintBackend::SetDefaultPrinterName(const std::string& printer_name) {
  default_printer_name_ = printer_name;
}

void TestPrintBackend::AddValidPrinter(
    const std::string& printer_name,
    std::unique_ptr<PrinterSemanticCapsAndDefaults> caps) {
  valid_printers_[printer_name] = std::move(caps);
}

}  // namespace printing
