// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/test/gtest_util.h"
#include "base/values.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/values.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

TEST(ValuesStructTraitsTest, NullValue) {
  base::Value in;
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
  EXPECT_EQ(in, out);
}

TEST(ValuesStructTraitsTest, BoolValue) {
  static constexpr bool kTestCases[] = {true, false};
  for (auto& test_case : kTestCases) {
    base::Value in(test_case);
    base::Value out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
    EXPECT_EQ(in, out);
  }
}

TEST(ValuesStructTraitsTest, IntValue) {
  static constexpr int kTestCases[] = {0, -1, 1,
                                       std::numeric_limits<int>::min(),
                                       std::numeric_limits<int>::max()};
  for (auto& test_case : kTestCases) {
    base::Value in(test_case);
    base::Value out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
    EXPECT_EQ(in, out);
  }
}

TEST(ValuesStructTraitsTest, DoubleValue) {
  static constexpr double kTestCases[] = {-0.0,
                                          +0.0,
                                          -1.0,
                                          +1.0,
                                          std::numeric_limits<double>::min(),
                                          std::numeric_limits<double>::max()};
  for (auto& test_case : kTestCases) {
    base::Value in(test_case);
    base::Value out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
    EXPECT_EQ(in, out);
  }
}

TEST(ValuesStructTraitsTest, StringValue) {
  static constexpr const char* kTestCases[] = {
      "", "ascii",
      // 🎆: Unicode FIREWORKS
      "\xf0\x9f\x8e\x86",
  };
  for (auto* test_case : kTestCases) {
    base::Value in(test_case);
    base::Value out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
    EXPECT_EQ(in, out);
  }
}

TEST(ValuesStructTraitsTest, BinaryValue) {
  std::vector<char> kBinaryData = {'\x00', '\x80', '\xff', '\x7f', '\x01'};
  base::Value in(std::move(kBinaryData));
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
  EXPECT_EQ(in, out);
}

TEST(ValuesStructTraitsTest, DictionaryValue) {
  // Note: here and below, it would be nice to use an initializer list, but
  // move-only types and initializer lists don't mix. Initializer lists can't be
  // modified: thus it's not possible to move.
  base::Value::Dict dict;
  dict.Set("null", base::Value());
  dict.Set("bool", false);
  dict.Set("int", 0);
  dict.Set("double", 0.0);
  dict.Set("string", "0");
  dict.Set("binary", base::Value::BlobStorage({0}));
  dict.Set("dictionary", base::Value::Dict());
  dict.Set("list", base::Value::List());

  base::Value in(std::move(dict));
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
  EXPECT_EQ(in, out);

  base::Value::Dict in_dict = in.GetDict().Clone();
  base::Value::Dict out_dict;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DictionaryValue>(
      in_dict, out_dict));
  EXPECT_EQ(in_dict, out_dict);
}

TEST(ValuesStructTraitsTest, ListValue) {
  base::Value::List list;
  list.Append(base::Value());
  list.Append(false);
  list.Append(0);
  list.Append(0.0);
  list.Append("0");
  list.Append(base::Value::BlobStorage({0}));
  list.Append(base::Value::Dict());
  list.Append(base::Value::List());
  base::Value in(std::move(list));
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
  EXPECT_EQ(in, out);

  base::Value::List in_list = in.GetList().Clone();
  base::Value::List out_list;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::ListValue>(in_list, out_list));
  EXPECT_EQ(in_list, out_list);
}

// A deeply nested base::Value should trigger a deserialization error.
TEST(ValuesStructTraitsTest, DeeplyNestedValue) {
  base::Value in;
  for (int i = 0; i < kMaxRecursionDepth; ++i) {
    base::Value::List list;
    list.Append(std::move(in));
    in = base::Value(std::move(list));
  }

  // It should work if the depth is less than kMaxRecursionDepth.
  {
    mojo::internal::ValidationErrorObserverForTesting warning_observer{
        base::DoNothing()};
    base::Value out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
    EXPECT_EQ(mojo::internal::VALIDATION_ERROR_NONE,
              warning_observer.last_error());
  }

  // Add one more depth.
  base::Value::List list;
  list.Append(std::move(in));
  in = base::Value(std::move(list));

  // It gets VALIDATION_ERROR_MAX_RECURSION_DEPTH error.
  {
    mojo::internal::ValidationErrorObserverForTesting warning_observer{
        base::DoNothing()};
    base::Value out;
    ASSERT_FALSE(mojo::test::SerializeAndDeserialize<mojom::Value>(in, out));
    EXPECT_EQ(mojo::internal::VALIDATION_ERROR_MAX_RECURSION_DEPTH,
              warning_observer.last_error());
  }
}

}  // namespace mojo_base
