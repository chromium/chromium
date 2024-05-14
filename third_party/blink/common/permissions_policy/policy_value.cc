// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/policy_value.h"

#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom.h"

namespace blink {

PolicyValue::PolicyValue() : type_(mojom::PolicyValueType::kNull) {}

PolicyValue::PolicyValue(const PolicyValue&) = default;

PolicyValue& PolicyValue::operator=(const PolicyValue&) = default;

// static
PolicyValue PolicyValue::CreateBool(bool value) {
  return PolicyValue(value);
}

// static
PolicyValue PolicyValue::CreateDecDouble(double value) {
  return PolicyValue(value, mojom::PolicyValueType::kDecDouble);
}

// static
PolicyValue PolicyValue::CreateEnum(int32_t value) {
  return PolicyValue(value, mojom::PolicyValueType::kEnum);
}

PolicyValue::PolicyValue(bool bool_value)
    : type_(mojom::PolicyValueType::kBool), bool_value_(bool_value) {}

PolicyValue::PolicyValue(double double_value, mojom::PolicyValueType type)
    : type_(type), double_value_(double_value) {
  DCHECK_EQ(type, mojom::PolicyValueType::kDecDouble);
}

PolicyValue::PolicyValue(int32_t int_value, mojom::PolicyValueType type)
    : type_(type), int_value_(int_value) {
  DCHECK_EQ(type, mojom::PolicyValueType::kEnum);
}

PolicyValue PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType type) {
  PolicyValue value;
  value.SetType(type);
  value.SetToMax();
  return value;
}

PolicyValue PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType type) {
  PolicyValue value;
  value.SetType(type);
  value.SetToMin();
  return value;
}

bool PolicyValue::BoolValue() const {
  DCHECK_EQ(type_, mojom::PolicyValueType::kBool);
  return bool_value_;
}

double PolicyValue::DoubleValue() const {
  DCHECK_EQ(type_, mojom::PolicyValueType::kDecDouble);
  return double_value_;
}

int32_t PolicyValue::IntValue() const {
  DCHECK_EQ(type_, mojom::PolicyValueType::kEnum);
  return int_value_;
}

void PolicyValue::SetBoolValue(bool bool_value) {
  DCHECK_EQ(mojom::PolicyValueType::kBool, type_);
  bool_value_ = bool_value;
}

void PolicyValue::SetDoubleValue(double double_value) {
  DCHECK_EQ(mojom::PolicyValueType::kDecDouble, type_);
  double_value_ = double_value;
}

void PolicyValue::SetIntValue(int32_t int_value) {
  DCHECK_EQ(mojom::PolicyValueType::kEnum, type_);
  int_value_ = int_value;
}

bool operator==(const PolicyValue& lhs, const PolicyValue& rhs) {
  if (lhs.Type() != rhs.Type())
    return false;
  switch (lhs.Type()) {
    case mojom::PolicyValueType::kBool:
      return lhs.BoolValue() == rhs.BoolValue();
    case mojom::PolicyValueType::kDecDouble:
      return lhs.DoubleValue() == rhs.DoubleValue();
    case mojom::PolicyValueType::kEnum:
      return lhs.IntValue() == rhs.IntValue();
    case mojom::PolicyValueType::kNull:
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool operator!=(const PolicyValue& lhs, const PolicyValue& rhs) {
  return !(lhs == rhs);
}

bool PolicyValue::IsCompatibleWith(const PolicyValue& required) const {
  DCHECK_EQ(type_, required.Type());
  switch (type_) {
    case mojom::PolicyValueType::kBool:
      return !bool_value_ || required.bool_value_;
    case mojom::PolicyValueType::kDecDouble:
      return double_value_ <= required.double_value_;
    case mojom::PolicyValueType::kEnum:
      return int_value_ == required.int_value_;
    case mojom::PolicyValueType::kNull:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return false;
}

void PolicyValue::SetToMax() {
  switch (type_) {
    case mojom::PolicyValueType::kBool:
      bool_value_ = true;
      break;
    case mojom::PolicyValueType::kDecDouble:
      double_value_ = std::numeric_limits<double>::infinity();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return;
}

void PolicyValue::SetToMin() {
  switch (type_) {
    case mojom::PolicyValueType::kBool:
      bool_value_ = false;
      break;
    case mojom::PolicyValueType::kDecDouble:
      double_value_ = 0.0;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return;
}

}  // namespace blink
