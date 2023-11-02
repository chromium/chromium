// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/typed_value.h"

#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

struct ValueA : TypedValue<ValueA, int> {
  using ValueA::TypedValue::TypedValue;
};

struct ValueB : TypedValue<ValueB, int> {
  using ValueB::TypedValue::TypedValue;
};

TEST(TypedIdTest, Value) {
  EXPECT_EQ(42, ValueA(42).value());
  EXPECT_EQ(42, static_cast<int>(ValueA(42)));  // explicit cast
}

TEST(TypedIdTest, Comparison) {
  EXPECT_TRUE(ValueA(0) == ValueA(0));
  EXPECT_FALSE(ValueA(0) == ValueA(42));
  EXPECT_FALSE(ValueA(0) != ValueA(0));
  EXPECT_TRUE(ValueA(0) != ValueA(42));
}

TEST(TypedIdTest, StrongType) {
  static_assert(!std::is_convertible<ValueA, ValueB>::value,
                "ValueA should not be convertible to ValueB");
  static_assert(!std::is_convertible<ValueB, ValueA>::value,
                "ValueB should not be convertible to ValueA");
}

}  // namespace zucchini
