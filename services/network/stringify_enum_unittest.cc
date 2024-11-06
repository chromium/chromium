// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/stringify_enum.h"

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

enum class EnumClass {
  kValue0 = 0,
  kValue1 = 1,
  kMaxValue = kValue1,
};

enum ShoutyClassicEnum {
  SHOUTY_VALUE0 = 0,
  SHOUTY_VALUE1 = 1,
  MAX = SHOUTY_VALUE1,
};

enum class UsesMax {
  kValue0,
  kMax = kValue0,
};

enum class UsesCount {
  kValue0,
  kValue1,
  kCount,
};

enum ClassicUsingCount {
  CLASSIC_VALUE0,
  COUNT,
};

enum class MissingMaxValue {
  kValue0,
  kValue1,
};

enum class SparseSmall {
  kSmall = 10,
  kMedium = 20,
  kLarge = 30,
  kMaxValue = kLarge,
};

enum class SparseBig {
  kSmall = 100,
  kMedium = 200,
  kLarge = 300,
  kMaxValue = kLarge,
};

enum class HasNegativeValues {
  kMinusTwo = -2,
  kMinusOne = -1,
  kOne = 1,
  kMaxValue = kOne,
};

enum class HasNegativeMaxValue {
  kMinusTwo = -2,
  kMinusOne = -1,
  kMaxValue = kMinusOne,
};

enum class TooSmallMaxValue {
  kValue0,
  kValue1,
  kMax = kValue1,
  kValue2,
};

enum class TooBigMaxValue {
  kValue0,
  kValue1,
  kMax = 25,
};

// This verifies that a user-defined operator<< is correctly detected for
// unscoped enums even though the operator<< found by built-in decay to integer
// is ignored. The wrapping struct just stops kMaxValue leaking into the
// surrounding namespace and causing compiler errors.
struct NoLeak {
  enum HasStreamOperator {
    kStreaming0,
    kMaxValue = kStreaming0,
  };
};

std::ostream& operator<<(std::ostream& os,
                         const NoLeak::HasStreamOperator& value) {
  return os << (value == NoLeak::kStreaming0 ? "Streaming 0" : "Invalid");
}

enum class HasConstRefStreamOperator {
  kStreaming0,
  kMaxValue = kStreaming0,
};

std::ostream& operator<<(std::ostream& os,
                         const HasConstRefStreamOperator& value) {
  const bool zero = value == HasConstRefStreamOperator::kStreaming0;
  return os << "const ref value is " << (zero ? "0" : "not recognized");
}

struct AnonymousEnum {
  enum {
    kHasNoName,
    kMaxValue = kHasNoName,
  };
};

template <typename UnderlyingType>
struct WithUnderlying {
  enum Enum : UnderlyingType {
    kValue = 0,
    kMaxValue = kValue,
  };
};

// Utility to call StreamEnumValueTo() and return the output as a string.
template <typename T>
std::string Stream(T&& value) {
  std::ostringstream oss;
  StreamEnumValueTo(oss, std::forward<T>(value));
  return oss.str();
}

TEST(StringifyEnumTest, TestMany) {
  struct TestResult {
    std::string_view const_name;
    std::string streamed_name;
  };
  struct TestCase {
    TestResult result;
    std::string_view expected_const_name;
    std::string_view expected_streamed_name;
  };
  auto test = [](auto value) {
    // We use the "ForGenericCode" version of the function with relaxed
    // constraints to make it easier to test failure cases.
    return TestResult(GetEnumValueNameForGenericCode(value), Stream(value));
  };

  const TestCase cases[] = {
      {test(EnumClass::kValue0), "EnumClass::kValue0", "EnumClass::kValue0"},
      {test(EnumClass::kValue1), "EnumClass::kValue1", "EnumClass::kValue1"},
      {test(SHOUTY_VALUE0), "SHOUTY_VALUE0", "SHOUTY_VALUE0"},
      {test(SHOUTY_VALUE1), "SHOUTY_VALUE1", "SHOUTY_VALUE1"},
      {test(UsesMax::kValue0), "UsesMax::kValue0", "UsesMax::kValue0"},
      {test(UsesCount::kValue1), "UsesCount::kValue1", "UsesCount::kValue1"},
      {test(CLASSIC_VALUE0), "CLASSIC_VALUE0", "CLASSIC_VALUE0"},
      {test(MissingMaxValue::kValue0), "", "Unknown (0)"},
      {test(MissingMaxValue::kValue1), "", "Unknown (1)"},
      {test(SparseSmall::kLarge), "SparseSmall::kLarge", "SparseSmall::kLarge"},
      {test(static_cast<SparseSmall>(4)), "", "Unknown (4)"},
      {test(SparseBig::kLarge), "", "Unknown (300)"},
      {test(HasNegativeValues::kMinusOne), "", "Unknown (-1)"},
      {test(HasNegativeMaxValue::kMinusTwo), "", "Unknown (-2)"},
      {test(TooSmallMaxValue::kValue1), "TooSmallMaxValue::kValue1",
       "TooSmallMaxValue::kValue1"},
      {test(TooSmallMaxValue::kValue2), "", "Unknown (2)"},
      {test(TooBigMaxValue::kValue1), "TooBigMaxValue::kValue1",
       "TooBigMaxValue::kValue1"},
      {test(NoLeak::kStreaming0), "kStreaming0", "Streaming 0"},
      {test(HasConstRefStreamOperator::kStreaming0),
       "HasConstRefStreamOperator::kStreaming0", "const ref value is 0"},
      {test(AnonymousEnum::kHasNoName), "kHasNoName", "kHasNoName"},
  };
  for (const auto& [result, expected_const_name, expected_streamed_name] :
       cases) {
    SCOPED_TRACE(expected_streamed_name);
    EXPECT_EQ(result.const_name, expected_const_name);
    EXPECT_EQ(result.streamed_name, expected_streamed_name);
  }
}

enum class Streamable {
  kValue,
  kMaxValue = kValue,
};

// This weird templated operator<< lets us detect what type of value we were
// called with.
template <typename T>
  requires(std::same_as<std::remove_cvref_t<T>, Streamable>)
std::ostream& operator<<(std::ostream& os, T&& streamable) {
  if constexpr (std::is_lvalue_reference_v<T>) {
    if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
      return os << "const lvalue ref";
    }
    return os << "mutable lvalue ref";
  }

  return os << "rvalue";
}

TEST(StringifyEnumTest, TestStreamingRefTypes) {
  EXPECT_EQ(Stream(Streamable::kValue), "rvalue");

  Streamable value = Streamable::kValue;
  EXPECT_EQ(Stream(value), "mutable lvalue ref");

  const Streamable const_value = value;
  EXPECT_EQ(Stream(const_value), "const lvalue ref");

  const Streamable& const_ref = value;
  EXPECT_EQ(Stream(const_ref), "const lvalue ref");

  Streamable& mutable_ref = value;
  EXPECT_EQ(Stream(mutable_ref), "mutable lvalue ref");

  EXPECT_EQ(Stream(std::move(value)), "rvalue");
}

// This is a typed test. See
// https://github.com/google/googletest/blob/main/docs/advanced.md#typed-tests.
template <typename T>
class StreamingEnumNoImplicitConversionsTest : public ::testing::Test {};

using IntegerTypes = ::testing::Types<uint8_t,
                                      int8_t,
                                      uint16_t,
                                      int16_t,
                                      uint32_t,
                                      int32_t,
                                      int,
                                      uint64_t,
                                      int64_t>;

TYPED_TEST_SUITE(StreamingEnumNoImplicitConversionsTest, IntegerTypes);

TYPED_TEST(StreamingEnumNoImplicitConversionsTest, NoImplicitConversions) {
  std::ostringstream oss;
  StreamEnumValueTo(oss, WithUnderlying<TypeParam>::kValue);
  EXPECT_EQ(oss.str(), "kValue");
}

TEST(StringifyEnumTest, GetEnumValueName) {
  // Just verify that GetEnumValueName is actually callable.
  EXPECT_EQ(GetEnumValueName(EnumClass::kValue0), "EnumClass::kValue0");
}

}  // namespace

}  // namespace network
