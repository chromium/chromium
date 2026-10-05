// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_POLICY_VALUE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_POLICY_VALUE_H_

#include <stdint.h>

#include <optional>
#include <utility>
#include <variant>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom-shared.h"

namespace blink {

// PolicyValue
// -----------
// PolicyValue is a sum type (variant of bool / double / int32_t) that can be
// used to specify the parameter of a policy.
class BLINK_COMMON_EXPORT PolicyValue {
 public:
  PolicyValue() = default;
  PolicyValue(const PolicyValue&) = default;
  PolicyValue& operator=(const PolicyValue&) = default;
  PolicyValue(PolicyValue&&) = default;
  PolicyValue& operator=(PolicyValue&&) = default;
  ~PolicyValue() = default;

  static PolicyValue CreateBool(bool);
  static PolicyValue CreateDecDouble(double);
  static PolicyValue CreateEnum(int32_t);

  // A 'max' PolicyValue is the most permissive value for the policy.
  // Will crash unless `type` is `kDecDouble` or `kBool`.
  static PolicyValue CreateMaxPolicyValue(mojom::PolicyValueType type);
  // A 'min' PolicyValue is the most restrictive value for the policy.
  // Will crash unless `type` is `kDecDouble` or `kBool`.
  static PolicyValue CreateMinPolicyValue(mojom::PolicyValueType type);

  mojom::PolicyValueType Type() const;

  // These will crash if called on the wrong type.
  bool BoolValue() const;
  double DoubleValue() const;
  int32_t EnumValue() const;

  std::optional<bool> GetIfBool() const;
  std::optional<double> GetIfDouble() const;
  std::optional<int32_t> GetIfEnum() const;

  template <typename Visitor>
  auto Visit(Visitor&& visitor) const {
    return std::visit(std::forward<Visitor>(visitor), value_);
  }

  // Test whether this policy value is compatible with required policy value.
  // Note: a.IsCompatibleWith(b) == true does not necessarily indicate
  // b.IsCompatibleWith(a) == false, because not all policy value types support
  // strictness comparison, e.g. enum.
  bool IsCompatibleWith(const PolicyValue& required) const;

  friend BLINK_COMMON_EXPORT bool operator==(const PolicyValue&,
                                             const PolicyValue&);

 private:
  explicit PolicyValue(bool bool_value);
  explicit PolicyValue(int32_t int_value);
  explicit PolicyValue(double double_value);

  using Value = std::variant<std::monostate, bool, double, int32_t>;
  Value value_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_POLICY_VALUE_H_
