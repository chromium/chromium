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

PageOutputQuality::PageOutputQuality(PageOutputQualityAttributes qualities,
                                     std::optional<std::string> default_quality)
    : qualities(std::move(qualities)),
      default_quality(std::move(default_quality)) {}

PageOutputQuality::PageOutputQuality(const PageOutputQuality& other) = default;

PageOutputQuality::~PageOutputQuality() = default;

// This function is only supposed to be used in tests. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that they can be used
// by tests but not non-test code. However, this .cc file is compiled as part of
// "backend" where "UNIT_TEST" is not defined. So we need to specify
// "COMPONENT_EXPORT(PRINT_BACKEND)" here again so that they are visible to
// tests.
COMPONENT_EXPORT(PRINT_BACKEND)
bool operator==(const PageOutputQuality& quality1,
                const PageOutputQuality& quality2) {
  return quality1.qualities == quality2.qualities &&
         quality1.default_quality == quality2.default_quality;
}

XpsCapabilities::XpsCapabilities() = default;

XpsCapabilities::XpsCapabilities(XpsCapabilities&& other) noexcept = default;

XpsCapabilities& XpsCapabilities::operator=(XpsCapabilities&& other) noexcept =
    default;

XpsCapabilities::~XpsCapabilities() = default;

#endif  // BUILDFLAG(IS_WIN)

PrinterSemanticCapsAndDefaults::Paper::Paper() = default;

PrinterSemanticCapsAndDefaults::Paper::Paper(const std::string& display_name,
                                             const std::string& vendor_id,
                                             const gfx::Size& size_um)
    : Paper(display_name, vendor_id, size_um, gfx::Rect(size_um)) {}

PrinterSemanticCapsAndDefaults::Paper::Paper(const std::string& display_name,
                                             const std::string& vendor_id,
                                             const gfx::Size& size_um,
                                             const gfx::Rect& printable_area_um)
    : Paper(display_name, vendor_id, size_um, printable_area_um, 0) {}

PrinterSemanticCapsAndDefaults::Paper::Paper(const std::string& display_name,
                                             const std::string& vendor_id,
                                             const gfx::Size& size_um,
                                             const gfx::Rect& printable_area_um,
                                             int max_height_um)
    : Paper(display_name,
            vendor_id,
            size_um,
            printable_area_um,
            max_height_um,
            false) {}

PrinterSemanticCapsAndDefaults::Paper::Paper(const std::string& display_name,
                                             const std::string& vendor_id,
                                             const gfx::Size& size_um,
                                             const gfx::Rect& printable_area_um,
                                             int max_height_um,
                                             bool has_borderless_variant)
    : display_name_(display_name),
      vendor_id_(vendor_id),
      size_um_(size_um),
      printable_area_um_(printable_area_um),
      max_height_um_(max_height_um),
      has_borderless_variant_(has_borderless_variant) {}

PrinterSemanticCapsAndDefaults::Paper::Paper(const Paper& other) = default;

PrinterSemanticCapsAndDefaults::Paper&
PrinterSemanticCapsAndDefaults::Paper::operator=(const Paper& other) = default;

bool PrinterSemanticCapsAndDefaults::Paper::operator==(
    const PrinterSemanticCapsAndDefaults::Paper& other) const {
  return display_name_ == other.display_name_ &&
         vendor_id_ == other.vendor_id_ && size_um_ == other.size_um_ &&
         printable_area_um_ == other.printable_area_um_ &&
         max_height_um_ == other.max_height_um_ &&
         has_borderless_variant_ == other.has_borderless_variant_;
}

bool PrinterSemanticCapsAndDefaults::Paper::SupportsCustomSize() const {
  return max_height_um_ > 0;
}

bool PrinterSemanticCapsAndDefaults::Paper::IsSizeWithinBounds(
    const gfx::Size& other_um) const {
  if (other_um == size_um_) {
    return true;
  }

  if (!SupportsCustomSize()) {
    return false;
  }

  return size_um_.width() == other_um.width() &&
         size_um_.height() <= other_um.height() &&
         other_um.height() <= max_height_um_;
}

bool PrinterSemanticCapsAndDefaults::MediaType::operator==(
    const PrinterSemanticCapsAndDefaults::MediaType& other) const {
  return display_name == other.display_name && vendor_id == other.vendor_id;
}

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults() = default;

PrinterSemanticCapsAndDefaults::PrinterSemanticCapsAndDefaults(
    const PrinterSemanticCapsAndDefaults& other) = default;

PrinterSemanticCapsAndDefaults::~PrinterSemanticCapsAndDefaults() = default;

// This function is only supposed to be used in tests. The declaration in the
// header file is guarded by "#if defined(UNIT_TEST)" so that they can be used
// by tests but not non-test code. However, this .cc file is compiled as part of
// "backend" where "UNIT_TEST" is not defined. So we need to specify
// "COMPONENT_EXPORT(PRINT_BACKEND)" here again so that they are visible to
// tests.
COMPONENT_EXPORT(PRINT_BACKEND)
bool operator==(const PrinterSemanticCapsAndDefaults& caps1,
                const PrinterSemanticCapsAndDefaults& caps2) {
  return caps1.collate_capable == caps2.collate_capable &&
         caps1.collate_default == caps2.collate_default &&
         caps1.copies_max == caps2.copies_max &&
         caps1.duplex_modes == caps2.duplex_modes &&
         caps1.duplex_default == caps2.duplex_default &&
         caps1.color_changeable == caps2.color_changeable &&
         caps1.color_default == caps2.color_default &&
         caps1.color_model == caps2.color_model &&
         caps1.bw_model == caps2.bw_model && caps1.papers == caps2.papers &&
         caps1.user_defined_papers == caps2.user_defined_papers &&
         caps1.default_paper == caps2.default_paper &&
         caps1.dpis == caps2.dpis && caps1.default_dpi == caps2.default_dpi
#if BUILDFLAG(IS_CHROMEOS)
         && caps1.pin_supported == caps2.pin_supported &&
         caps1.advanced_capabilities == caps2.advanced_capabilities
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_WIN)
         && caps1.page_output_quality == caps2.page_output_quality
#endif  // BUILDFLAG(IS_WIN)
      ;
}

PrinterCapsAndDefaults::PrinterCapsAndDefaults() = default;

PrinterCapsAndDefaults::PrinterCapsAndDefaults(
    const PrinterCapsAndDefaults& other) = default;

PrinterCapsAndDefaults::~PrinterCapsAndDefaults() = default;

PrintBackend::PrintBackend() = default;

PrintBackend::~PrintBackend() = default;

// static
scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const std::string& locale) {
  return g_print_backend_for_test ? g_print_backend_for_test
                                  : PrintBackend::CreateInstanceImpl(locale);
}

// static
void PrintBackend::SetPrintBackendForTesting(PrintBackend* backend) {
  g_print_backend_for_test = backend;
}

}  // namespace printing
