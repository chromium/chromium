// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"

namespace {

// PrintBackend override for testing.
printing::PrintBackend* g_print_backend_for_test = nullptr;

}  // namespace

namespace printing {

PrinterBasicInfo::PrinterBasicInfo() = default;

PrinterBasicInfo::PrinterBasicInfo(const std::string& printer_name,
                                   const std::string& display_name,
                                   const std::string& printer_description,
                                   int printer_status,
                                   bool is_default,
                                   const PrinterBasicInfoOptions& options)
    : printer_name(printer_name),
      display_name(display_name),
      printer_description(printer_description),
      printer_status(printer_status),
      is_default(is_default),
      options(options) {}

PrinterBasicInfo::PrinterBasicInfo(const PrinterBasicInfo& other) = default;

PrinterBasicInfo::~PrinterBasicInfo() = default;

bool PrinterBasicInfo::operator==(const PrinterBasicInfo& other) const {
  return printer_name == other.printer_name &&
         display_name == other.display_name &&
         printer_description == other.printer_description &&
         printer_status == other.printer_status &&
         is_default == other.is_default && options == other.options;
}

#if BUILDFLAG(IS_CHROMEOS)

AdvancedCapabilityValue::AdvancedCapabilityValue() = default;

AdvancedCapabilityValue::AdvancedCapabilityValue(
    const std::string& name,
    const std::string& display_name)
    : name(name), display_name(display_name) {}

AdvancedCapabilityValue::AdvancedCapabilityValue(
    const AdvancedCapabilityValue& other) = default;

AdvancedCapabilityValue::~AdvancedCapabilityValue() = default;

bool AdvancedCapabilityValue::operator==(
    const AdvancedCapabilityValue& other) const {
  return name == other.name && display_name == other.display_name;
}

AdvancedCapability::AdvancedCapability() = default;

AdvancedCapability::AdvancedCapability(const std::string& name,
                                       AdvancedCapability::Type type)
    : name(name), type(type) {}

AdvancedCapability::AdvancedCapability(
    const std::string& name,
    const std::string& display_name,
    AdvancedCapability::Type type,
    const std::string& default_value,
    const std::vector<AdvancedCapabilityValue>& values)
    : name(name),
      display_name(display_name),
      type(type),
      default_value(default_value),
      values(values) {}

AdvancedCapability::AdvancedCapability(const AdvancedCapability& other) =
    default;

AdvancedCapability::~AdvancedCapability() = default;

bool AdvancedCapability::operator==(const AdvancedCapability& other) const {
  return name == other.name && display_name == other.display_name &&
         type == other.type && default_value == other.default_value &&
         values == other.values;
}

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)

PageOutputQualityAttribute::PageOutputQualityAttribute() = default;

PageOutputQualityAttribute::PageOutputQualityAttribute(
    const std::string& display_name,
    const std::string& name)
    : display_name(display_name), name(name) {}

PageOutputQualityAttribute::~PageOutputQualityAttribute() = default;

bool PageOutputQualityAttribute::operator==(
    const PageOutputQualityAttribute& other) const {
  return display_name == other.display_name && name == other.name;
}

bool PageOutputQualityAttribute::operator<(
    const ::printing::PageOutputQualityAttribute& other) const {
  return std::tie(name, display_name) <
         std::tie(other.name, other.display_name);
}

PageOutputQuality::PageOutputQuality() = default;

PageOutputQuality::PageOutputQuality(
    PageOutputQualityAttributes qualities,
    absl::optional<std::string> default_quality)
    : qualities(std::move(qualities)),
      default_quality(std::move(default_quality)) {}

PageOutputQuality::PageOutputQuality(const PageOutputQuality& other) = default;

PageOutputQuality::~PageOutputQuality() = default;

#endif  // BUILDFLAG(IS_WIN)

bool PrinterSemanticCapsAndDefaults::Paper::operator==(
    const PrinterSemanticCapsAndDefaults::Paper& other) const {
  return display_name == other.display_name && vendor_id == other.vendor_id &&
         size_um == other.size_um;
}

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults() = default;

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults(
    const PrinterSemanticCapsAndDefaults& other) = default;

PrinterSemanticCapsAndDefaults::~PrinterSemanticCapsAndDefaults() = default;

PrinterCapsAndDefaults::PrinterCapsAndDefaults() = default;

PrinterCapsAndDefaults::PrinterCapsAndDefaults(
    const PrinterCapsAndDefaults& other) = default;

PrinterCapsAndDefaults::~PrinterCapsAndDefaults() = default;

PrintBackend::PrintBackend() = default;

PrintBackend::~PrintBackend() = default;

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const std::string& locale) {
  return g_print_backend_for_test
             ? g_print_backend_for_test
             : PrintBackend::CreateInstanceImpl(
                   /*print_backend_settings=*/nullptr, locale);
}

// static
void PrintBackend::SetPrintBackendForTesting(PrintBackend* backend) {
  g_print_backend_for_test = backend;
}

}  // namespace printing
