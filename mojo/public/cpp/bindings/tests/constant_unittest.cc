// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <string_view>

#include "mojo/public/interfaces/bindings/tests/test_constants.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

TEST(ConstantTest, GlobalConstants) {
  // Compile-time constants.
  static_assert(kBoolValue == true, "");
  static_assert(kInt8Value == -2, "");
  static_assert(kUint8Value == 128U, "");
  static_assert(kInt16Value == -233, "");
  static_assert(kUint16Value == 44204U, "");
  static_assert(kInt32Value == -44204, "");
  static_assert(kUint32Value == 4294967295U, "");
  static_assert(kInt64Value == -9223372036854775807, "");
  static_assert(kUint64Value == 9999999999999999999ULL, "");
  static_assert(kDoubleValue == 3.14159, "");
  static_assert(kFloatValue == 2.71828f, "");

  EXPECT_EQ(std::string_view(kStringValue), "test string contents");
  EXPECT_TRUE(std::isnan(kDoubleNaN));
  EXPECT_TRUE(std::isinf(kDoubleInfinity));
  EXPECT_TRUE(std::isinf(kDoubleNegativeInfinity));
  EXPECT_NE(kDoubleInfinity, kDoubleNegativeInfinity);
  EXPECT_TRUE(std::isnan(kFloatNaN));
  EXPECT_TRUE(std::isinf(kFloatInfinity));
  EXPECT_TRUE(std::isinf(kFloatNegativeInfinity));
  EXPECT_NE(kFloatInfinity, kFloatNegativeInfinity);
}

TEST(ConstantTest, StructConstants) {
  // Compile-time constants.
  static_assert(StructWithConstants::kInt8Value == 5U, "");
  static_assert(StructWithConstants::kFloatValue == 765.432f, "");

  EXPECT_EQ(std::string_view(StructWithConstants::kStringValue),
            "struct test string contents");
}

TEST(ConstantTest, InterfaceConstants) {
  // Compile-time constants.
  static_assert(InterfaceWithConstants::kUint32Value == 20100722, "");
  static_assert(InterfaceWithConstants::kDoubleValue == 12.34567, "");

  EXPECT_EQ(std::string_view(InterfaceWithConstants::kStringValue),
            "interface test string contents");
  EXPECT_EQ(std::string_view(InterfaceWithConstants::Name_),
            "mojo.test.InterfaceWithConstants");
}

}  // namespace test
}  // namespace mojo
