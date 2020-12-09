// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/test_print_backend.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "printing/backend/print_backend.h"

namespace printing {

TestPrintBackend::TestPrintBackend() : PrintBackend(/*locale=*/std::string()) {}

TestPrintBackend::~TestPrintBackend() = default;

bool TestPrintBackend::EnumeratePrinters(PrinterList* printer_list) {
  if (printer_map_.empty())
    return false;

  for (const auto& entry : printer_map_) {
    const std::unique_ptr<PrinterData>& data = entry.second;

    // Can only return basic info for printers which have registered info.
    if (data->info)
      printer_list->emplace_back(*data->info);
  }
  return true;
}

std::string TestPrintBackend::GetDefaultPrinterName() {
  return default_printer_name_;
}

bool TestPrintBackend::GetPrinterBasicInfo(const std::string& printer_name,
                                           PrinterBasicInfo* printer_info) {
  auto found = printer_map_.find(printer_name);
  if (found == printer_map_.end())
    return false;  // Matching entry not found.

  // Basic info might not have been provided.
  const std::unique_ptr<PrinterData>& data = found->second;
  if (!data->info)
    return false;

  *printer_info = *data->info;
  return true;
}

bool TestPrintBackend::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_caps) {
  auto found = printer_map_.find(printer_name);
  if (found == printer_map_.end())
    return false;

  // Capabilities might not have been provided.
  const std::unique_ptr<PrinterData>& data = found->second;
  if (!data->caps)
    return false;

  *printer_caps = *data->caps;
  return true;
}

bool TestPrintBackend::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_caps) {
  // not implemented
  return false;
}

std::string TestPrintBackend::GetPrinterDriverInfo(
    const std::string& printer_name) {
  // not implemented
  return "";
}

bool TestPrintBackend::IsValidPrinter(const std::string& printer_name) {
  return base::Contains(printer_map_, printer_name);
}

void TestPrintBackend::SetDefaultPrinterName(const std::string& printer_name) {
  if (default_printer_name_ == printer_name)
    return;

  auto found = printer_map_.find(printer_name);
  if (found == printer_map_.end()) {
    DLOG(ERROR) << "Unable to set an unknown printer as the default.  Unknown "
                << "printer name: " << printer_name;
    return;
  }

  // Previous default printer is no longer the default.
  const std::unique_ptr<PrinterData>& new_default_data = found->second;
  if (!default_printer_name_.empty())
    printer_map_[default_printer_name_]->info->is_default = false;

  // Now update new printer as default.
  default_printer_name_ = printer_name;
  if (!default_printer_name_.empty())
    new_default_data->info->is_default = true;
}

void TestPrintBackend::AddValidPrinter(
    const std::string& printer_name,
    std::unique_ptr<PrinterSemanticCapsAndDefaults> caps,
    std::unique_ptr<PrinterBasicInfo> info) {
  DCHECK(!printer_name.empty());

  const bool is_default = info && info->is_default;
  printer_map_[printer_name] =
      std::make_unique<PrinterData>(std::move(caps), std::move(info));

  // Ensure that default settings are honored if more than one is attempted to
  // be marked as default or if this prior default should no longer be so.
  if (is_default)
    SetDefaultPrinterName(printer_name);
  else if (default_printer_name_ == printer_name)
    default_printer_name_.clear();
}

TestPrintBackend::PrinterData::PrinterData(
    std::unique_ptr<PrinterSemanticCapsAndDefaults> caps,
    std::unique_ptr<PrinterBasicInfo> info)
    : caps(std::move(caps)), info(std::move(info)) {}

TestPrintBackend::PrinterData::~PrinterData() = default;

}  // namespace printing
