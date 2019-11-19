// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_

#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MODULES_EXPORT HIDReportItem : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HIDReportItem(const device::mojom::blink::HidReportItem& item);
  ~HIDReportItem() override;

  bool isAbsolute() const { return is_absolute_; }
  bool isArray() const { return is_array_; }
  bool isRange() const { return is_range_; }
  bool hasNull() const { return has_null_; }
  const Vector<uint32_t>& usages() const { return usages_; }
  uint32_t usageMinimum() const { return usage_minimum_; }
  uint32_t usageMaximum() const { return usage_maximum_; }
  const Vector<String>& strings() const { return strings_; }
  uint16_t reportSize() const { return report_size_; }
  uint16_t reportCount() const { return report_count_; }
  int8_t unitExponent() const { return unit_exponent_; }
  String unitSystem() const { return unit_system_; }
  int8_t unitFactorLengthExponent() const {
    return unit_factor_length_exponent_;
  }
  int8_t unitFactorMassExponent() const { return unit_factor_mass_exponent_; }
  int8_t unitFactorTimeExponent() const { return unit_factor_time_exponent_; }
  int8_t unitFactorTemperatureExponent() const {
    return unit_factor_temperature_exponent_;
  }
  int8_t unitFactorCurrentExponent() const {
    return unit_factor_current_exponent_;
  }
  int8_t unitFactorLuminousIntensityExponent() const {
    return unit_factor_luminous_intensity_exponent_;
  }
  int32_t logicalMinimum() const { return logical_minimum_; }
  int32_t logicalMaximum() const { return logical_maximum_; }
  int32_t physicalMinimum() const { return physical_minimum_; }
  int32_t physicalMaximum() const { return physical_maximum_; }

 private:
  bool is_absolute_;
  bool is_array_;
  bool is_range_;
  bool has_null_;
  Vector<uint32_t> usages_;
  Vector<String> strings_;
  uint32_t usage_minimum_;
  uint32_t usage_maximum_;
  uint16_t report_size_;
  uint16_t report_count_;
  int8_t unit_exponent_;
  String unit_system_;
  int8_t unit_factor_length_exponent_;
  int8_t unit_factor_mass_exponent_;
  int8_t unit_factor_time_exponent_;
  int8_t unit_factor_temperature_exponent_;
  int8_t unit_factor_current_exponent_;
  int8_t unit_factor_luminous_intensity_exponent_;
  int32_t logical_minimum_;
  int32_t logical_maximum_;
  int32_t physical_minimum_;
  int32_t physical_maximum_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_REPORT_ITEM_H_
