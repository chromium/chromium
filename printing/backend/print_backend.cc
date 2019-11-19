// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

namespace {

// PrintBackend override for testing.
printing::PrintBackend* g_print_backend_for_test = nullptr;

}  // namespace

namespace printing {

PrinterBasicInfo::PrinterBasicInfo() = default;

PrinterBasicInfo::PrinterBasicInfo(const PrinterBasicInfo& other) = default;

PrinterBasicInfo::~PrinterBasicInfo() = default;

#if defined(OS_CHROMEOS)

AdvancedCapabilityValue::AdvancedCapabilityValue() = default;

AdvancedCapabilityValue::AdvancedCapabilityValue(
    const AdvancedCapabilityValue& other) = default;

AdvancedCapabilityValue::~AdvancedCapabilityValue() = default;

AdvancedCapability::AdvancedCapability() = default;

AdvancedCapability::AdvancedCapability(const AdvancedCapability& other) =
    default;

AdvancedCapability::~AdvancedCapability() = default;

#endif  // defined(OS_CHROMEOS)

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults() = default;

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults(
    const PrinterSemanticCapsAndDefaults& other) = default;

PrinterSemanticCapsAndDefaults::~PrinterSemanticCapsAndDefaults() = default;

PrinterCapsAndDefaults::PrinterCapsAndDefaults() = default;

PrinterCapsAndDefaults::PrinterCapsAndDefaults(
    const PrinterCapsAndDefaults& other) = default;

PrinterCapsAndDefaults::~PrinterCapsAndDefaults() = default;

PrintBackend::~PrintBackend() = default;

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const base::DictionaryValue* print_backend_settings) {
  return g_print_backend_for_test
             ? g_print_backend_for_test
             : PrintBackend::CreateInstanceImpl(print_backend_settings);
}

// static
void PrintBackend::SetPrintBackendForTesting(PrintBackend* backend) {
  g_print_backend_for_test = backend;
}

}  // namespace printing
