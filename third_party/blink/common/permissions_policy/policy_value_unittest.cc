// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/policy_value.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom.h"

namespace blink {

class PolicyValueTest : public testing::Test {};

TEST_F(PolicyValueTest, TestCanCreateBoolValues) {
  PolicyValue false_value = PolicyValue::CreateBool(false);
  PolicyValue true_value = PolicyValue::CreateBool(true);
  PolicyValue min_value(
      PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType::kBool));
  PolicyValue max_value(
      PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kBool));
  EXPECT_EQ(false_value.BoolValue(), false);
  EXPECT_EQ(true_value.BoolValue(), true);
  EXPECT_EQ(min_value.BoolValue(), false);
  EXPECT_EQ(max_value.BoolValue(), true);
}

TEST_F(PolicyValueTest, TestCanModifyBoolValues) {
  PolicyValue initially_false_value = PolicyValue::CreateBool(false);
  PolicyValue initially_true_value = PolicyValue::CreateBool(true);
  initially_false_value.SetBoolValue(true);
  initially_true_value.SetBoolValue(false);
  EXPECT_EQ(initially_false_value.BoolValue(), true);
  EXPECT_EQ(initially_true_value.BoolValue(), false);

  initially_true_value.SetToMax();
  EXPECT_EQ(initially_true_value.BoolValue(), true);
  initially_true_value.SetToMin();
  EXPECT_EQ(initially_true_value.BoolValue(), false);
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
  EXPECT_EQ(zero_value.DoubleValue(), 0.0);
  EXPECT_EQ(one_value.DoubleValue(), 1.0);
  EXPECT_EQ(min_value.DoubleValue(), 0.0);
  EXPECT_EQ(max_value.DoubleValue(), std::numeric_limits<double>::infinity());
}

TEST_F(PolicyValueTest, TestCanModifyDoubleValues) {
  PolicyValue initially_zero_value = PolicyValue::CreateDecDouble(0.0);
  initially_zero_value.SetDoubleValue(1.0);
  EXPECT_EQ(initially_zero_value.DoubleValue(), 1.0);
  initially_zero_value.SetToMax();
  EXPECT_EQ(initially_zero_value.DoubleValue(),
            std::numeric_limits<double>::infinity());
  initially_zero_value.SetToMin();
  EXPECT_EQ(initially_zero_value.DoubleValue(), 0.0);
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
  EXPECT_EQ(enum_value_a.IntValue(), 1);
  EXPECT_EQ(enum_value_b.IntValue(), 2);
}

TEST_F(PolicyValueTest, TestCanModifyEnumValues) {
  PolicyValue enum_value_a = PolicyValue::CreateEnum(1);
  enum_value_a.SetIntValue(2);
  EXPECT_EQ(enum_value_a.IntValue(), 2);
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

}  // namespace blink
