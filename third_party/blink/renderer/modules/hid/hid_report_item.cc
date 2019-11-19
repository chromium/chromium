// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_report_item.h"

#include "services/device/public/mojom/hid.mojom-blink.h"

namespace blink {

namespace {

// The HID specification defines four canonical unit systems. Each unit system
// corresponds to a set of units for length, mass, time, temperature, current,
// and luminous intensity. The vendor-defined unit system can be used for
// devices which produce measurements that cannot be adequately described by
// these unit systems.
//
// See the Units table in section 6.2.2.7 of the Device Class Definition for
// HID v1.11.
// https://www.usb.org/document-library/device-class-definition-hid-111
enum HidUnitSystem {
  // none: No unit system
  kUnitSystemNone = 0x00,
  // si-linear: Centimeter, Gram, Seconds, Kelvin, Ampere, Candela
  kUnitSystemSILinear = 0x01,
  // si-rotation: Radians, Gram, Seconds, Kelvin, Ampere, Candela
  kUnitSystemSIRotation = 0x02,
  // english-linear: Inch, Slug, Seconds, Fahrenheit, Ampere, Candela
  kUnitSystemEnglishLinear = 0x03,
  // english-linear: Degrees, Slug, Seconds, Fahrenheit, Ampere, Candela
  kUnitSystemEnglishRotation = 0x04,
  // vendor-defined unit system
  kUnitSystemVendorDefined = 0x0f,
};

uint32_t ConvertHidUsageAndPageToUint32(
    const device::mojom::blink::HidUsageAndPage& usage) {
  return (usage.usage_page) << 16 | usage.usage;
}

String UnitSystemToString(uint8_t unit) {
  DCHECK_LE(unit, 0x0f);
  switch (unit) {
    case kUnitSystemNone:
      return "none";
    case kUnitSystemSILinear:
      return "si-linear";
    case kUnitSystemSIRotation:
      return "si-rotation";
    case kUnitSystemEnglishLinear:
      return "english-linear";
    case kUnitSystemEnglishRotation:
      return "english-rotation";
    case kUnitSystemVendorDefined:
      return "vendor-defined";
    default:
      break;
  }
  // Values other than those defined in HidUnitSystem are reserved by the spec.
  return "reserved";
}

// Convert |unit_factor_exponent| from its coded representation to a signed
// integer type.
int8_t UnitFactorExponentToInt(uint8_t unit_factor_exponent) {
  DCHECK_LE(unit_factor_exponent, 0x0f);
  // Values from 0x08 to 0x0f encode negative exponents.
  if (unit_factor_exponent > 0x08)
    return int8_t{unit_factor_exponent} - 16;
  return unit_factor_exponent;
}

// Unpack the 32-bit unit definition value |unit| into each of its components.
// The unit definition value includes the unit system as well as unit factor
// exponents for each of the 6 units defined by the unit system.
void UnpackUnitValues(uint32_t unit,
                      String& unit_system,
                      int8_t& length_exponent,
                      int8_t& mass_exponent,
                      int8_t& time_exponent,
                      int8_t& temperature_exponent,
                      int8_t& current_exponent,
                      int8_t& luminous_intensity_exponent) {
  unit_system = UnitSystemToString(unit & 0x0f);
  length_exponent = UnitFactorExponentToInt((unit >> 4) & 0x0f);
  mass_exponent = UnitFactorExponentToInt((unit >> 8) & 0x0f);
  time_exponent = UnitFactorExponentToInt((unit >> 12) & 0x0f);
  temperature_exponent = UnitFactorExponentToInt((unit >> 16) & 0x0f);
  current_exponent = UnitFactorExponentToInt((unit >> 20) & 0x0f);
  luminous_intensity_exponent = UnitFactorExponentToInt((unit >> 24) & 0x0f);
}

}  // namespace

HIDReportItem::HIDReportItem(const device::mojom::blink::HidReportItem& item)
    : is_absolute_(!item.is_relative),
      is_array_(!item.is_variable),
      is_range_(item.is_range),
      has_null_(item.has_null_position),
      report_size_(item.report_size),
      report_count_(item.report_count),
      unit_exponent_(UnitFactorExponentToInt(item.unit_exponent & 0x0f)),
      logical_minimum_(item.logical_minimum),
      logical_maximum_(item.logical_maximum),
      physical_minimum_(item.physical_minimum),
      physical_maximum_(item.physical_maximum) {
  for (const auto& usage : item.usages)
    usages_.push_back(ConvertHidUsageAndPageToUint32(*usage));
  usage_minimum_ = ConvertHidUsageAndPageToUint32(*item.usage_minimum);
  usage_maximum_ = ConvertHidUsageAndPageToUint32(*item.usage_maximum);
  UnpackUnitValues(item.unit, unit_system_, unit_factor_length_exponent_,
                   unit_factor_mass_exponent_, unit_factor_time_exponent_,
                   unit_factor_temperature_exponent_,
                   unit_factor_current_exponent_,
                   unit_factor_luminous_intensity_exponent_);

  // TODO(mattreynolds): Set |strings_|.
}

HIDReportItem::~HIDReportItem() = default;

}  // namespace blink
