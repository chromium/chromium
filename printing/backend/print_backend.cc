// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include "build/chromeos_buildflags.h"

namespace {

// PrintBackend override for testing.
printing::PrintBackend* g_print_backend_for_test = nullptr;

}  // namespace

namespace printing {

PrinterBasicInfo::PrinterBasicInfo() = default;

PrinterBasicInfo::PrinterBasicInfo(const PrinterBasicInfo& other) = default;

PrinterBasicInfo::~PrinterBasicInfo() = default;

#if BUILDFLAG(IS_ASH)

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

#endif  // BUILDFLAG(IS_ASH)

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

PrintBackend::PrintBackend(const std::string& locale) : locale_(locale) {}

PrintBackend::~PrintBackend() = default;

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const std::string& locale) {
  return g_print_backend_for_test
             ? g_print_backend_for_test
             : PrintBackend::CreateInstanceImpl(
                   /*print_backend_settings=*/nullptr, locale,
                   /*for_cloud_print=*/false);
}

#if defined(USE_CUPS)
// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstanceForCloudPrint(
    const base::DictionaryValue* print_backend_settings) {
  return PrintBackend::CreateInstanceImpl(print_backend_settings,
                                          /*locale=*/std::string(),
                                          /*for_cloud_print=*/true);
}
#endif  // defined(USE_CUPS)

// static
void PrintBackend::SetPrintBackendForTesting(PrintBackend* backend) {
  g_print_backend_for_test = backend;
}

}  // namespace printing
