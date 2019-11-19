// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_advanced_caps.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"

namespace printing {

namespace {

using AdvancedCapabilities = std::vector<AdvancedCapability>;

// Handles IPP attribute, usually by adding 1 or more items to |caps|.
using AttributeHandler =
    base::RepeatingCallback<void(const CupsOptionProvider& printer,
                                 const char* name,
                                 AdvancedCapabilities* caps)>;

void NoOpHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities*) {}

void ScalarHandler(const CupsOptionProvider& printer,
                   const char* attribute_name,
                   AdvancedCapabilities* capabilities) {
  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
  capability.type = base::Value::Type::STRING;
  // TODO(crbug.com/964919) Set defaults.
}

void BooleanHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities) {
  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
  capability.type = base::Value::Type::BOOLEAN;
  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  capability.default_value = attr_default && ippGetBoolean(attr_default, 0);
}

void KeywordHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  if (attr_default) {
    const char* value = ippGetString(attr_default, 0, nullptr);
    if (value)
      capability.default_value = value;
  }
  int num_values = ippGetCount(attr);
  for (int i = 0; i < num_values; i++) {
    const char* value = ippGetString(attr, i, nullptr);
    if (!value)
      continue;

    capability.values.emplace_back();
    capability.values.back().name = value;
  }
}

void EnumHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  capabilities->emplace_back();
  AdvancedCapability& capability = capabilities->back();
  capability.name = attribute_name;
  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  capability.default_value =
      base::NumberToString(attr_default ? ippGetInteger(attr_default, 0) : 0);
  int num_values = ippGetCount(attr);
  for (int i = 0; i < num_values; i++) {
    int value = ippGetInteger(attr, i);
    capability.values.emplace_back();
    capability.values.back().name = base::NumberToString(value);
  }
}

void MultivalueEnumHandler(int none_value,
                           const CupsOptionProvider& printer,
                           const char* attribute_name,
                           AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  int num_values = ippGetCount(attr);
  for (int i = 0; i < num_values; i++) {
    int value = ippGetInteger(attr, i);
    // Check for 'none' value;
    if (value == none_value)
      continue;

    capabilities->emplace_back();
    AdvancedCapability& capability = capabilities->back();
    capability.name =
        std::string(attribute_name) + "/" + base::NumberToString(value);
    capability.type = base::Value::Type::BOOLEAN;
    // TODO(crbug.com/964919) Set defaults.
  }
}

using HandlerMap = std::map<base::StringPiece, AttributeHandler>;

HandlerMap GenerateHandlers() {
  // TODO(crbug.com/964919) Generate from csv.
  HandlerMap result;
  result.emplace("confirmation-sheet-print",
                 base::BindRepeating(&BooleanHandler));
  result.emplace("copies", base::BindRepeating(&NoOpHandler));
  result.emplace("finishings", base::BindRepeating(&MultivalueEnumHandler, 3));
  result.emplace("ipp-attribute-fidelity", base::BindRepeating(BooleanHandler));
  // We don't have a way to release jobs yet.
  result.emplace("job-hold-until", base::BindRepeating(&NoOpHandler));
  result.emplace("job-name", base::BindRepeating(&ScalarHandler));
  result.emplace("job-password", base::BindRepeating(&NoOpHandler));
  result.emplace("job-password-encryption", base::BindRepeating(&NoOpHandler));
  // TODO(crbug.com/964919) Add validation for an int in 1..100 range.
  result.emplace("job-priority", base::BindRepeating(&ScalarHandler));
  // CUPS thinks "job-sheets" is multivalue. RFC 8011 says it isn't.
  result.emplace("job-sheets", base::BindRepeating(&KeywordHandler));
  result.emplace("media", base::BindRepeating(&NoOpHandler));
  result.emplace("media-col", base::BindRepeating(&NoOpHandler));
  result.emplace("multiple-document-handling",
                 base::BindRepeating(&KeywordHandler));
  result.emplace("number-up", base::BindRepeating(&NoOpHandler));
  result.emplace("orientation-requested", base::BindRepeating(&EnumHandler));
  result.emplace("output-bin", base::BindRepeating(&KeywordHandler));
  result.emplace("page-ranges", base::BindRepeating(&NoOpHandler));
  result.emplace("print-color-mode", base::BindRepeating(&NoOpHandler));
  result.emplace("print-quality", base::BindRepeating(&EnumHandler));
  result.emplace("printer-resolution", base::BindRepeating(&NoOpHandler));
  result.emplace("sheet-collate", base::BindRepeating(&NoOpHandler));
  result.emplace("sides", base::BindRepeating(&NoOpHandler));
  return result;
}

// Returns the number of IPP attributes added to |caps| (not necessarily in
// 1-to-1 correspondence).
size_t AddAttributes(const CupsOptionProvider& printer,
                     const char* attr_group_name,
                     AdvancedCapabilities* caps) {
  static const base::NoDestructor<HandlerMap> handlers(GenerateHandlers());
  size_t attr_count = 0;

  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attr_group_name);
  if (!attr)
    return 0;

  int num_options = ippGetCount(attr);
  for (int i = 0; i < num_options; i++) {
    const char* option_name = ippGetString(attr, i, nullptr);
    auto it = handlers->find(option_name);
    if (it == handlers->end()) {
      LOG(WARNING) << "Unknown IPP option: " << option_name;
      continue;
    }

    size_t previous_size = caps->size();
    it->second.Run(printer, option_name, caps);
    if (caps->size() > previous_size)
      attr_count++;
  }
  return attr_count;
}

}  // namespace

void ExtractAdvancedCapabilities(const CupsOptionProvider& printer,
                                 PrinterSemanticCapsAndDefaults* printer_info) {
  AdvancedCapabilities* options = &printer_info->advanced_capabilities;
  size_t attr_count = AddAttributes(printer, kIppJobAttributes, options);
  attr_count += AddAttributes(printer, kIppDocumentAttributes, options);
  base::UmaHistogramCounts1000("Printing.CUPS.IppAttributesCount", attr_count);
}

}  // namespace printing
