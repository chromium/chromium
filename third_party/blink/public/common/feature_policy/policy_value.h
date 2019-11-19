// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_POLICY_VALUE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_POLICY_VALUE_H_

#include "base/macros.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/feature_policy/policy_value.mojom-forward.h"

namespace blink {

// ListValue (PolicyValue)
// ----------------------
// PolicyValue is a union of types (int / double / set<int> / bool ) that can be
// used to specify the parameter of a policy.

// TODO(loonybear): Add the following types: enum, inc/dec int, inc double, set.
class BLINK_COMMON_EXPORT PolicyValue {
 public:
  PolicyValue();
  ~PolicyValue();

  explicit PolicyValue(mojom::PolicyValueType);

  explicit PolicyValue(bool bool_value);
  explicit PolicyValue(double double_value);
  PolicyValue(double double_value, mojom::PolicyValueType type);

  // A 'max' PolicyValue is the most permissive value for the policy.
  static PolicyValue CreateMaxPolicyValue(mojom::PolicyValueType type);
  // A 'min' PolicyValue is the most restrictive value for the policy.
  static PolicyValue CreateMinPolicyValue(mojom::PolicyValueType type);

  mojom::PolicyValueType Type() const { return type_; }
  void SetType(mojom::PolicyValueType type) { type_ = type; }

  // PolicyValue getters.
  // Note the getters also DCHECKs that the type is correct.
  bool BoolValue() const;
  double DoubleValue() const;

  // PolicyValue setters.
  // Note the getters also DCHECKs that the type is correct.
  void SetBoolValue(bool bool_value);
  void SetDoubleValue(double double_value);
  void SetDoubleValue(double double_value, mojom::PolicyValueType type);

  // Operater overrides
  PolicyValue& operator=(const PolicyValue& rhs);
  // Combine a new PolicyValue to self, by taking the stricter value of the two.
  void Combine(const PolicyValue& value);
  // Combine two PolicyValue_s together by taking the stricter value of the two.
  static PolicyValue Combine(const PolicyValue& lhs, const PolicyValue& rhs);

  void SetToMax();
  void SetToMin();

 private:
  mojom::PolicyValueType type_;
  bool bool_value_ = false;
  double double_value_ = 0.0;
};

bool BLINK_COMMON_EXPORT operator==(const PolicyValue& lhs,
                                    const PolicyValue& rhs);
bool BLINK_COMMON_EXPORT operator!=(const PolicyValue& lhs,
                                    const PolicyValue& rhs);
bool BLINK_COMMON_EXPORT operator>(const PolicyValue& lhs,
                                   const PolicyValue& rhs);
bool BLINK_COMMON_EXPORT operator>=(const PolicyValue& lhs,
                                    const PolicyValue& rhs);
bool BLINK_COMMON_EXPORT operator<(const PolicyValue& lhs,
                                   const PolicyValue& rhs);
bool BLINK_COMMON_EXPORT operator<=(const PolicyValue& lhs,
                                    const PolicyValue& rhs);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FEATURE_POLICY_POLICY_VALUE_H_
