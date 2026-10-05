// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/policy_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom.h"

namespace blink {

class PolicyValueTest : public testing::Test {};

TEST_F(PolicyValueTest, TestNullValues) {
  PolicyValue null_value;
  EXPECT_EQ(null_value.Type(), mojom::PolicyValueType::kNull);
  EXPECT_TRUE(null_value == PolicyValue());
  EXPECT_FALSE(null_value == PolicyValue::CreateBool(false));
}

TEST_F(PolicyValueTest, TestCanCreateBoolValues) {
  PolicyValue false_value = PolicyValue::CreateBool(false);
  PolicyValue true_value = PolicyValue::CreateBool(true);
  PolicyValue min_value(
      PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType::kBool));
  PolicyValue max_value(
      PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kBool));
  EXPECT_EQ(false_value.Type(), mojom::PolicyValueType::kBool);
  EXPECT_EQ(true_value.Type(), mojom::PolicyValueType::kBool);
  EXPECT_EQ(false_value.BoolValue(), false);
  EXPECT_EQ(true_value.BoolValue(), true);
  EXPECT_EQ(min_value.BoolValue(), false);
  EXPECT_EQ(max_value.BoolValue(), true);
}


TEST_F(PolicyValueTest, TestCanCompareBoolValues) {
  PolicyValue false_value = PolicyValue::CreateBool(false);
  PolicyValue true_value = PolicyValue::CreateBool(true);

  EXPECT_TRUE(false_value == false_value);
  EXPECT_FALSE(false_value != false_value);
  EXPECT_TRUE(false_value.IsCompatibleWith(false_value));

  EXPECT_FALSE(false_value == true_value);
  EXPECT_TRUE(false_value != true_value);
  EXPECT_TRUE(false_value.IsCompatibleWith(true_value));

  EXPECT_FALSE(true_value == false_value);
  EXPECT_TRUE(true_value != false_value);
  EXPECT_FALSE(true_value.IsCompatibleWith(false_value));

  EXPECT_TRUE(true_value == true_value);
  EXPECT_FALSE(true_value != true_value);
  EXPECT_TRUE(true_value.IsCompatibleWith(true_value));
}

TEST_F(PolicyValueTest, TestCanCreateDoubleValues) {
  PolicyValue zero_value = PolicyValue::CreateDecDouble(0.0);
  PolicyValue one_value = PolicyValue::CreateDecDouble(1.0);
  PolicyValue min_value(
      PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType::kDecDouble));
  PolicyValue max_value(
      PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kDecDouble));
  EXPECT_EQ(zero_value.Type(), mojom::PolicyValueType::kDecDouble);
  EXPECT_EQ(one_value.Type(), mojom::PolicyValueType::kDecDouble);
  EXPECT_EQ(zero_value.DoubleValue(), 0.0);
  EXPECT_EQ(one_value.DoubleValue(), 1.0);
  EXPECT_EQ(min_value.DoubleValue(), 0.0);
  EXPECT_EQ(max_value.DoubleValue(), std::numeric_limits<double>::infinity());
}


TEST_F(PolicyValueTest, TestCanCompareDoubleValues) {
  PolicyValue low_value = PolicyValue::CreateDecDouble(1.0);
  PolicyValue high_value = PolicyValue::CreateDecDouble(2.0);

  EXPECT_TRUE(low_value == low_value);
  EXPECT_FALSE(low_value != low_value);
  EXPECT_TRUE(low_value.IsCompatibleWith(low_value));

  EXPECT_FALSE(low_value == high_value);
  EXPECT_TRUE(low_value != high_value);
  EXPECT_TRUE(low_value.IsCompatibleWith(high_value));

  EXPECT_FALSE(high_value == low_value);
  EXPECT_TRUE(high_value != low_value);
  EXPECT_FALSE(high_value.IsCompatibleWith(low_value));

  EXPECT_TRUE(high_value == high_value);
  EXPECT_FALSE(high_value != high_value);
  EXPECT_TRUE(high_value.IsCompatibleWith(high_value));
}

TEST_F(PolicyValueTest, TestCanCreateEnumValues) {
  PolicyValue enum_value_a = PolicyValue::CreateEnum(1);
  PolicyValue enum_value_b = PolicyValue::CreateEnum(2);
  EXPECT_EQ(enum_value_a.Type(), mojom::PolicyValueType::kEnum);
  EXPECT_EQ(enum_value_b.Type(), mojom::PolicyValueType::kEnum);
  EXPECT_EQ(enum_value_a.EnumValue(), 1);
  EXPECT_EQ(enum_value_b.EnumValue(), 2);
}

TEST_F(PolicyValueTest, TestCanCompareEnumValues) {
  PolicyValue enum_value_a = PolicyValue::CreateEnum(1);
  PolicyValue enum_value_b = PolicyValue::CreateEnum(2);

  EXPECT_TRUE(enum_value_a == enum_value_a);
  EXPECT_FALSE(enum_value_a != enum_value_a);
  EXPECT_TRUE(enum_value_a.IsCompatibleWith(enum_value_a));

  EXPECT_FALSE(enum_value_b == enum_value_a);
  EXPECT_TRUE(enum_value_b != enum_value_a);
  EXPECT_FALSE(enum_value_b.IsCompatibleWith(enum_value_a));

  EXPECT_FALSE(enum_value_a == enum_value_b);
  EXPECT_TRUE(enum_value_a != enum_value_b);
  EXPECT_FALSE(enum_value_a.IsCompatibleWith(enum_value_b));

  EXPECT_TRUE(enum_value_b == enum_value_b);
  EXPECT_FALSE(enum_value_b != enum_value_b);
  EXPECT_TRUE(enum_value_b.IsCompatibleWith(enum_value_b));
}

TEST_F(PolicyValueTest, TestIncompatibleTypesAreNotCompatible) {
  PolicyValue bool_val = PolicyValue::CreateBool(true);
  PolicyValue double_val = PolicyValue::CreateDecDouble(1.0);
  PolicyValue enum_val = PolicyValue::CreateEnum(1);
  PolicyValue null_val;

  EXPECT_FALSE(bool_val.IsCompatibleWith(double_val));
  EXPECT_FALSE(double_val.IsCompatibleWith(enum_val));
  EXPECT_FALSE(enum_val.IsCompatibleWith(bool_val));
  EXPECT_FALSE(null_val.IsCompatibleWith(null_val));
  EXPECT_FALSE(null_val.IsCompatibleWith(bool_val));
  EXPECT_FALSE(bool_val.IsCompatibleWith(null_val));
}

TEST_F(PolicyValueTest, TestGetIf) {
  PolicyValue bool_val = PolicyValue::CreateBool(true);
  PolicyValue double_val = PolicyValue::CreateDecDouble(1.5);
  PolicyValue enum_val = PolicyValue::CreateEnum(42);
  PolicyValue null_val;

  EXPECT_EQ(bool_val.GetIfBool(), true);
  EXPECT_EQ(bool_val.GetIfDouble(), std::nullopt);
  EXPECT_EQ(bool_val.GetIfEnum(), std::nullopt);

  EXPECT_EQ(double_val.GetIfBool(), std::nullopt);
  EXPECT_EQ(double_val.GetIfDouble(), 1.5);
  EXPECT_EQ(double_val.GetIfEnum(), std::nullopt);

  EXPECT_EQ(enum_val.GetIfBool(), std::nullopt);
  EXPECT_EQ(enum_val.GetIfDouble(), std::nullopt);
  EXPECT_EQ(enum_val.GetIfEnum(), 42);

  EXPECT_EQ(null_val.GetIfBool(), std::nullopt);
  EXPECT_EQ(null_val.GetIfDouble(), std::nullopt);
  EXPECT_EQ(null_val.GetIfEnum(), std::nullopt);
}

}  // namespace blink
