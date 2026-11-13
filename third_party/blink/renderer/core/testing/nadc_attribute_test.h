// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NADC_ATTRIBUTE_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NADC_ATTRIBUTE_TEST_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class NADCAttributeTest final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NADCAttributeTest() = default;
  ~NADCAttributeTest() override = default;

  void setFloat64Value(double value) { float64_value_ = value; }
  double float64Value() const { return float64_value_; }

  void setFloat32Value(float value) { float32_value_ = value; }
  float float32Value() const { return float32_value_; }

  void setUnrestrictedFloat64Value(double value) {
    unrestricted_float64_value_ = value;
  }
  double unrestrictedFloat64Value() const {
    return unrestricted_float64_value_;
  }

  void setEnforceRangeOctetValue(uint8_t value) {
    enforce_range_octet_value_ = value;
  }
  uint8_t enforceRangeOctetValue() const { return enforce_range_octet_value_; }

  void setEnforceRangeLongValue(int32_t value) {
    enforce_range_long_value_ = value;
  }
  int32_t enforceRangeLongValue() const { return enforce_range_long_value_; }

  void setEnforceRangeUnsignedLongValue(uint32_t value) {
    enforce_range_unsigned_long_value_ = value;
  }
  uint32_t enforceRangeUnsignedLongValue() const {
    return enforce_range_unsigned_long_value_;
  }
  uint32_t enforceRangeUnsignedLongSecondValue() const {
    return enforce_range_unsigned_long_second_value_;
  }

  void setEnforceRangeLongLongValue(int64_t value) {
    enforce_range_long_long_value_ = value;
  }
  int64_t enforceRangeLongLongValue() const {
    return enforce_range_long_long_value_;
  }

  void setEnforceRangeUnsignedLongLongValue(uint64_t value) {
    enforce_range_unsigned_long_long_value_ = value;
  }
  uint64_t enforceRangeUnsignedLongLongValue() const {
    return enforce_range_unsigned_long_long_value_;
  }

  void setClampLongValue(int64_t value) { clamp_long_value_ = value; }
  int64_t clampLongValue() const { return clamp_long_value_; }

  void setEnforceRangeLong(int32_t value) { enforce_range_long_value_ = value; }
  void setEnforceRangeUnsignedLongPair(uint32_t first, uint32_t second) {
    enforce_range_unsigned_long_value_ = first;
    enforce_range_unsigned_long_second_value_ = second;
  }
  void setEnforceRangeLongLong(int64_t value) {
    enforce_range_long_long_value_ = value;
  }
  void setEnforceRangeUnsignedLongLong(uint64_t value) {
    enforce_range_unsigned_long_long_value_ = value;
  }
  void setRestrictedFloat(float value) { float32_value_ = value; }
  void setRestrictedDouble(double value) { float64_value_ = value; }
  int32_t addEnforceRangeLong(int32_t a, int32_t b) {
    return static_cast<int32_t>(static_cast<uint32_t>(a) +
                                static_cast<uint32_t>(b));
  }

  void Trace(Visitor* visitor) const override {
    blink::ScriptWrappable::Trace(visitor);
  }

 private:
  double float64_value_ = 0.0;
  float float32_value_ = 0.0f;
  double unrestricted_float64_value_ = 0.0;
  uint8_t enforce_range_octet_value_ = 0;
  int32_t enforce_range_long_value_ = 0;
  uint32_t enforce_range_unsigned_long_value_ = 0;
  uint32_t enforce_range_unsigned_long_second_value_ = 0;
  int64_t enforce_range_long_long_value_ = 0;
  uint64_t enforce_range_unsigned_long_long_value_ = 0;
  int64_t clamp_long_value_ = 0;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_NADC_ATTRIBUTE_TEST_H_
