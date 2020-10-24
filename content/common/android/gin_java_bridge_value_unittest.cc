// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/gin_java_bridge_value.h"

#include <stdint.h>

#include <cmath>
#include <memory>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class GinJavaBridgeValueTest : public testing::Test {
};

TEST_F(GinJavaBridgeValueTest, BasicValues) {
  float native_float;
  int32_t native_object_id;

  std::unique_ptr<base::Value> undefined(
      GinJavaBridgeValue::CreateUndefinedValue());
  ASSERT_TRUE(undefined.get());
  EXPECT_TRUE(GinJavaBridgeValue::ContainsGinJavaBridgeValue(undefined.get()));
  std::unique_ptr<const GinJavaBridgeValue> undefined_value(
      GinJavaBridgeValue::FromValue(undefined.get()));
  ASSERT_TRUE(undefined_value.get());
  EXPECT_TRUE(undefined_value->IsType(GinJavaBridgeValue::TYPE_UNDEFINED));

  EXPECT_FALSE(undefined_value->GetAsNonFinite(&native_float));
  EXPECT_FALSE(undefined_value->GetAsObjectID(&native_object_id));

  std::unique_ptr<base::Value> float_infinity(
      GinJavaBridgeValue::CreateNonFiniteValue(
          std::numeric_limits<float>::infinity()));
  ASSERT_TRUE(float_infinity.get());
  EXPECT_TRUE(
      GinJavaBridgeValue::ContainsGinJavaBridgeValue(float_infinity.get()));
  std::unique_ptr<const GinJavaBridgeValue> float_infinity_value(
      GinJavaBridgeValue::FromValue(float_infinity.get()));
  ASSERT_TRUE(float_infinity_value.get());
  EXPECT_TRUE(float_infinity_value->IsType(GinJavaBridgeValue::TYPE_NONFINITE));
  EXPECT_TRUE(float_infinity_value->GetAsNonFinite(&native_float));
  EXPECT_TRUE(std::isinf(native_float));

  EXPECT_FALSE(undefined_value->GetAsObjectID(&native_object_id));

  std::unique_ptr<base::Value> double_infinity(
      GinJavaBridgeValue::CreateNonFiniteValue(
          std::numeric_limits<double>::infinity()));
  ASSERT_TRUE(double_infinity.get());
  EXPECT_TRUE(
      GinJavaBridgeValue::ContainsGinJavaBridgeValue(double_infinity.get()));
  std::unique_ptr<const GinJavaBridgeValue> double_infinity_value(
      GinJavaBridgeValue::FromValue(double_infinity.get()));
  ASSERT_TRUE(double_infinity_value.get());
  EXPECT_TRUE(
      double_infinity_value->IsType(GinJavaBridgeValue::TYPE_NONFINITE));
  EXPECT_TRUE(double_infinity_value->GetAsNonFinite(&native_float));
  EXPECT_TRUE(std::isinf(native_float));

  EXPECT_FALSE(undefined_value->GetAsObjectID(&native_object_id));

  std::unique_ptr<base::Value> object_id(
      GinJavaBridgeValue::CreateObjectIDValue(42));
  ASSERT_TRUE(object_id.get());
  EXPECT_TRUE(GinJavaBridgeValue::ContainsGinJavaBridgeValue(object_id.get()));
  std::unique_ptr<const GinJavaBridgeValue> object_id_value(
      GinJavaBridgeValue::FromValue(object_id.get()));
  ASSERT_TRUE(object_id_value.get());
  EXPECT_TRUE(object_id_value->IsType(GinJavaBridgeValue::TYPE_OBJECT_ID));
  EXPECT_TRUE(object_id_value->GetAsObjectID(&native_object_id));
  EXPECT_EQ(42, native_object_id);

  EXPECT_FALSE(undefined_value->GetAsNonFinite(&native_float));
}

TEST_F(GinJavaBridgeValueTest, BrokenValues) {
  std::unique_ptr<base::Value> non_binary(new base::Value(42));
  EXPECT_FALSE(
      GinJavaBridgeValue::ContainsGinJavaBridgeValue(non_binary.get()));

  const char dummy_data[] = "\000\001\002\003\004\005\006\007\010\011\012\013";
  base::Value broken_binary(base::as_bytes(base::make_span(dummy_data)));
  EXPECT_FALSE(GinJavaBridgeValue::ContainsGinJavaBridgeValue(&broken_binary));
}

}  // namespace
