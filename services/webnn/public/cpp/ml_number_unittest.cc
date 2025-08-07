// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/ml_number.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fp16/src/include/fp16.h"

namespace webnn {

TEST(MLNumberTest, AsFloat16FromFloat64) {
  MLNumber number = MLNumber::FromFloat64(123);
  EXPECT_EQ(number.AsFloat16(), fp16_ieee_from_fp32_value(123));
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsFloat32FromFloat64) {
  MLNumber number = MLNumber::FromFloat64(3.14f);
  EXPECT_EQ(number.AsFloat32(), 3.14f);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsFloat64FromFloat64) {
  MLNumber number = MLNumber::FromFloat64(2.71828);
  EXPECT_EQ(number.AsFloat64(), 2.71828);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsUint8FromFloat64) {
  MLNumber number1 = MLNumber::FromFloat64(255);
  EXPECT_EQ(number1.AsUint8(), 255);
  EXPECT_EQ(number1.GetBaseType(), MLNumber::BaseType::kFloatingPoint);

  MLNumber number2 = MLNumber::FromFloat64(254.8);
  EXPECT_EQ(number2.AsUint8(), 254);
  EXPECT_EQ(number2.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsInt8FromFloat64) {
  MLNumber number1 = MLNumber::FromFloat64(-128);
  EXPECT_EQ(number1.AsInt8(), -128);
  EXPECT_EQ(number1.GetBaseType(), MLNumber::BaseType::kFloatingPoint);

  MLNumber number2 = MLNumber::FromFloat64(-127.8);
  EXPECT_EQ(number2.AsInt8(), -127);
  EXPECT_EQ(number2.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsUint32FromFloat64) {
  MLNumber number = MLNumber::FromFloat64(4294967295U);
  EXPECT_EQ(number.AsUint32(), 4294967295U);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsInt32FromFloat64) {
  MLNumber number = MLNumber::FromFloat64(-2147483648);
  EXPECT_EQ(number.AsInt32(), -2147483648);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsUint64FromFloat64) {
  MLNumber number = MLNumber::FromFloat64(4503599627370496);
  EXPECT_EQ(number.AsUint64(), 4503599627370496ULL);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, AsInt64FromFloat64) {
  MLNumber number = MLNumber::FromFloat64(-4503599627370496);
  EXPECT_EQ(number.AsInt64(), -4503599627370496LL);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, Asfloat64FromInt64) {
  MLNumber number = MLNumber::FromInt64(-4503599627370496LL);
  EXPECT_EQ(number.AsFloat64(), -4503599627370496);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kSignedInteger);

  MLNumber number2 = MLNumber::FromInt64(-9223372036854775807LL);
  EXPECT_EQ(number2.AsFloat64(), -9.2233720368547758e+18);
  EXPECT_EQ(number2.GetBaseType(), MLNumber::BaseType::kSignedInteger);
}

TEST(MLNumberTest, AsUint64FromInt64) {
  MLNumber number = MLNumber::FromInt64(-1);
  EXPECT_EQ(number.AsUint64(), 0U);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kSignedInteger);
}

TEST(MLNumberTest, AsInt64FromInt64) {
  MLNumber number = MLNumber::FromInt64(-9223372036854775807LL);
  EXPECT_EQ(number.AsInt64(), -9223372036854775807LL);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kSignedInteger);
}

TEST(MLNumberTest, Asfloat64FromUint64) {
  MLNumber number1 = MLNumber::FromUint64(4503599627370496LL);
  EXPECT_EQ(number1.AsFloat64(), 4503599627370496);
  EXPECT_EQ(number1.GetBaseType(), MLNumber::BaseType::kUnsignedInteger);

  MLNumber number2 = MLNumber::FromUint64(18446744073709551615ULL);
  EXPECT_EQ(number2.AsFloat64(), 1.8446744073709552e+19);
  EXPECT_EQ(number2.GetBaseType(), MLNumber::BaseType::kUnsignedInteger);
}

TEST(MLNumberTest, AsInt64FromUint64) {
  MLNumber number = MLNumber::FromUint64(18446744073709551615ULL);
  EXPECT_EQ(number.AsInt64(), 9223372036854775807LL);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kUnsignedInteger);
}

TEST(MLNumberTest, CreateFromUint64) {
  MLNumber number = MLNumber::FromUint64(18446744073709551615ULL);
  EXPECT_EQ(number.AsUint64(), 18446744073709551615ULL);
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kUnsignedInteger);
}

TEST(MLNumberTest, Infinity) {
  MLNumber number = MLNumber::Infinity();
  EXPECT_EQ(number.AsFloat64(), std::numeric_limits<double>::infinity());
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, NegativeInfinity) {
  MLNumber number = MLNumber::NegativeInfinity();
  EXPECT_EQ(number.AsFloat64(), -std::numeric_limits<double>::infinity());
  EXPECT_EQ(number.GetBaseType(), MLNumber::BaseType::kFloatingPoint);
}

TEST(MLNumberTest, IsGreaterThanForInt8) {
  MLNumber a = MLNumber::FromFloat64(123);
  MLNumber b = MLNumber::FromFloat64(-123);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kInt8));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kInt8));

  MLNumber c = MLNumber::FromFloat64(1000);
  MLNumber d = MLNumber::Infinity();
  // Both values should saturate to the max int8.
  EXPECT_FALSE(c.IsGreaterThan(d, OperandDataType::kInt8));
  EXPECT_FALSE(d.IsGreaterThan(c, OperandDataType::kInt8));
}

TEST(MLNumberTest, IsGreaterThanForUint8) {
  MLNumber a = MLNumber::FromFloat64(243);
  MLNumber b = MLNumber::FromFloat64(123);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kUint8));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kUint8));
}

TEST(MLNumberTest, IsGreaterThanForInt32) {
  MLNumber a = MLNumber::FromFloat64(1234);
  MLNumber b = MLNumber::FromFloat64(-1234);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kInt32));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kInt32));
}

TEST(MLNumberTest, IsGreaterThanForUint32) {
  MLNumber a = MLNumber::FromFloat64(1234);
  MLNumber b = MLNumber::FromFloat64(123);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kUint32));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kUint32));
}

TEST(MLNumberTest, IsGreaterThanForInt64) {
  MLNumber a = MLNumber::FromFloat64(124324342);
  MLNumber b = MLNumber::FromFloat64(-2344343);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kInt64));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kInt64));
}

TEST(MLNumberTest, IsGreaterThanForUint64) {
  MLNumber a = MLNumber::FromFloat64(124324342);
  MLNumber b = MLNumber::FromFloat64(13435);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kInt64));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kInt64));
}

TEST(MLNumberTest, IsGreaterThanForFloat32) {
  MLNumber a = MLNumber::FromFloat64(124.23432);
  MLNumber b = MLNumber::FromFloat64(-324.2434);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kFloat32));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kFloat32));
}

TEST(MLNumberTest, IsGreaterThanForFloat16) {
  MLNumber a = MLNumber::FromFloat64(12324);
  MLNumber b = MLNumber::FromFloat64(-3421);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kFloat16));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kFloat16));
}

TEST(MLNumberTest, CompareFloatPointWithInteger) {
  MLNumber a = MLNumber::FromFloat64(325.23);
  MLNumber b = MLNumber::FromFloat64(324);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kInt32));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kInt32));
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kFloat32));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kFloat32));
}

TEST(MLNumberTest, ComparedInfinityWithMaxUint64Value) {
  MLNumber a = MLNumber::Infinity();
  MLNumber b = MLNumber::FromUint64(std::numeric_limits<uint64_t>::max());
  // a == b
  EXPECT_FALSE(a.IsGreaterThan(b, OperandDataType::kUint64));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kUint64));
}

TEST(MLNumberTest, ComparedInfinityWithUint64Value) {
  MLNumber a = MLNumber::Infinity();
  MLNumber b = MLNumber::FromUint64(18446744073709551614ULL);
  EXPECT_TRUE(a.IsGreaterThan(b, OperandDataType::kUint64));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kUint64));
}

TEST(MLNumberTest, ComparedNegativeInfinityWithMinInt64Value) {
  MLNumber a = MLNumber::NegativeInfinity();
  MLNumber b = MLNumber::FromInt64(std::numeric_limits<int64_t>::min());
  // a == b
  EXPECT_FALSE(a.IsGreaterThan(b, OperandDataType::kInt64));
  EXPECT_FALSE(b.IsGreaterThan(a, OperandDataType::kInt64));
}

TEST(MLNumberTest, ComparedNegativeInfinityWithInt64Value) {
  MLNumber a = MLNumber::NegativeInfinity();
  MLNumber b = MLNumber::FromInt64(-9223372036854775805LL);
  EXPECT_TRUE(b.IsGreaterThan(a, OperandDataType::kInt64));
  EXPECT_FALSE(a.IsGreaterThan(b, OperandDataType::kInt64));
}

TEST(MLNumberTest, InitializedFromNAN) {
  MLNumber a = MLNumber::FromFloat64(NAN);
  EXPECT_TRUE(a.IsNaN());
  EXPECT_TRUE(std::isnan(a.AsFloat64()));
  EXPECT_EQ(a.AsUint64(), 0ULL);
  EXPECT_EQ(a.AsInt64(), 0LL);
}

}  // namespace webnn
