// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/policy_value.h"

#include <limits>

#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom.h"

namespace blink {

// static
PolicyValue PolicyValue::CreateBool(bool value) {
  return PolicyValue(value);
}

// static
PolicyValue PolicyValue::CreateDecDouble(double value) {
  return PolicyValue(value);
}

// static
PolicyValue PolicyValue::CreateEnum(int32_t value) {
  return PolicyValue(value);
}

PolicyValue::PolicyValue(bool bool_value) : value_(bool_value) {}

PolicyValue::PolicyValue(double double_value) : value_(double_value) {}

PolicyValue::PolicyValue(int32_t int_value) : value_(int_value) {}

PolicyValue PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType type) {
  switch (type) {
    case mojom::PolicyValueType::kBool:
      return CreateBool(true);
    case mojom::PolicyValueType::kDecDouble:
      return CreateDecDouble(std::numeric_limits<double>::infinity());
    default:
      NOTREACHED();
  }
}

PolicyValue PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType type) {
  switch (type) {
    case mojom::PolicyValueType::kBool:
      return CreateBool(false);
    case mojom::PolicyValueType::kDecDouble:
      return CreateDecDouble(0.0);
    default:
      NOTREACHED();
  }
}

mojom::PolicyValueType PolicyValue::Type() const {
  return Visit(absl::Overload{
      [](std::monostate) { return mojom::PolicyValueType::kNull; },
      [](bool) { return mojom::PolicyValueType::kBool; },
      [](double) { return mojom::PolicyValueType::kDecDouble; },
      [](int32_t) { return mojom::PolicyValueType::kEnum; },
  });
}

bool PolicyValue::BoolValue() const {
  return std::get<bool>(value_);
}

double PolicyValue::DoubleValue() const {
  return std::get<double>(value_);
}

int32_t PolicyValue::EnumValue() const {
  return std::get<int32_t>(value_);
}

std::optional<bool> PolicyValue::GetIfBool() const {
  if (const auto* v = std::get_if<bool>(&value_)) {
    return *v;
  }
  return std::nullopt;
}

std::optional<double> PolicyValue::GetIfDouble() const {
  if (const auto* v = std::get_if<double>(&value_)) {
    return *v;
  }
  return std::nullopt;
}

std::optional<int32_t> PolicyValue::GetIfEnum() const {
  if (const auto* v = std::get_if<int32_t>(&value_)) {
    return *v;
  }
  return std::nullopt;
}

bool operator==(const PolicyValue&, const PolicyValue&) = default;

bool PolicyValue::IsCompatibleWith(const PolicyValue& required) const {
  return std::visit(absl::Overload{
                        [](bool b, bool req_b) { return !b || req_b; },
                        [](double d, double req_d) { return d <= req_d; },
                        [](int32_t i, int32_t req_i) { return i == req_i; },
                        [](const auto&, const auto&) { return false; },
                    },
                    value_, required.value_);
}

}  // namespace blink
