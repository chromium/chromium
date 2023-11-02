// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/ipp_handlers.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "printing/backend/cups_printer.h"

namespace printing {

void NoOpHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities* capabilities) {}

void TextHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities* capabilities) {
  capabilities->emplace_back(attribute_name, AdvancedCapability::Type::kString);
  // TODO(crbug.com/964919): Set defaults.
}

void NumberHandler(const CupsOptionProvider& printer,
                   const char* attribute_name,
                   AdvancedCapabilities* capabilities) {
  // TODO(crbug.com/964919): Add better number handling.
  TextHandler(printer, attribute_name, capabilities);
}

void BooleanHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities) {
  capabilities->emplace_back(attribute_name,
                             AdvancedCapability::Type::kBoolean);
  AdvancedCapability& capability = capabilities->back();
  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  capability.default_value = attr_default && ippGetBoolean(attr_default, 0);
}

void KeywordHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities) {
  ipp_attribute_t* attr = printer.GetSupportedOptionValues(attribute_name);
  if (!attr)
    return;

  capabilities->emplace_back(attribute_name, AdvancedCapability::Type::kString);
  AdvancedCapability& capability = capabilities->back();
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

  capabilities->emplace_back(attribute_name, AdvancedCapability::Type::kString);
  AdvancedCapability& capability = capabilities->back();
  ipp_attribute_t* attr_default = printer.GetDefaultOptionValue(attribute_name);
  capability.default_value =
      base::NumberToString(attr_default ? ippGetInteger(attr_default, 0) : 0);
  int num_values = ippGetCount(attr);
  for (int i = 0; i < num_values; i++) {
    int value = ippGetInteger(attr, i);
    // ippGetInteger() returns 0 on error as per RFC8011 (5.1.5)
    if (value == 0)
      continue;

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
    // Check for 'none' value or error (0 as per RFC8011 (5.1.5)).
    if (value == none_value || value == 0)
      continue;

    capabilities->emplace_back(
        base::StrCat({attribute_name, "/", base::NumberToString(value)}),
        AdvancedCapability::Type::kBoolean);
    // TODO(crbug.com/964919): Set defaults.
  }
}

}  // namespace printing
