// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/test/gtest_util.h"
#include "base/values.h"
#include "mojo/public/cpp/base/values_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/values.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

TEST(ValuesStructTraitsTest, NullValue) {
  base::Value in;
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
  EXPECT_EQ(in, out);
}

TEST(ValuesStructTraitsTest, BoolValue) {
  static constexpr bool kTestCases[] = {true, false};
  for (auto& test_case : kTestCases) {
    base::Value in(test_case);
    base::Value out;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
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
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
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
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
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
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
    EXPECT_EQ(in, out);
  }
}

TEST(ValuesStructTraitsTest, BinaryValue) {
  std::vector<char> kBinaryData = {'\x00', '\x80', '\xff', '\x7f', '\x01'};
  base::Value in(std::move(kBinaryData));
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
  EXPECT_EQ(in, out);
}

TEST(ValuesStructTraitsTest, DictionaryValue) {
  // Note: here and below, it would be nice to use an initializer list, but
  // move-only types and initializer lists don't mix. Initializer lists can't be
  // modified: thus it's not possible to move.
  std::vector<base::Value::DictStorage::value_type> storage;
  storage.emplace_back("null", std::make_unique<base::Value>());
  storage.emplace_back("bool", std::make_unique<base::Value>(false));
  storage.emplace_back("int", std::make_unique<base::Value>(0));
  storage.emplace_back("double", std::make_unique<base::Value>(0.0));
  storage.emplace_back("string", std::make_unique<base::Value>("0"));
  storage.emplace_back(
      "binary", std::make_unique<base::Value>(base::Value::BlobStorage({0})));
  storage.emplace_back(
      "dictionary", std::make_unique<base::Value>(base::Value::DictStorage()));
  storage.emplace_back(
      "list", std::make_unique<base::Value>(base::Value::ListStorage()));

  base::Value in(base::Value::DictStorage(std::move(storage)));
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
  EXPECT_EQ(in, out);

  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DictionaryValue>(&in, &out));
  EXPECT_EQ(in, out);
}

TEST(ValuesStructTraitsTest, SerializeInvalidDictionaryValue) {
  base::Value in;
  ASSERT_FALSE(in.is_dict());

  base::Value out;
  EXPECT_DCHECK_DEATH(
      mojo::test::SerializeAndDeserialize<mojom::DictionaryValue>(&in, &out));
}

TEST(ValuesStructTraitsTest, ListValue) {
  base::Value::ListStorage storage;
  storage.emplace_back();
  storage.emplace_back(false);
  storage.emplace_back(0);
  storage.emplace_back(0.0);
  storage.emplace_back("0");
  storage.emplace_back(base::Value::BlobStorage({0}));
  storage.emplace_back(base::Value::DictStorage());
  storage.emplace_back(base::Value::ListStorage());
  base::Value in(std::move(storage));
  base::Value out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
  EXPECT_EQ(in, out);

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ListValue>(&in, &out));
  EXPECT_EQ(in, out);
}

TEST(ValuesStructTraitsTest, SerializeInvalidListValue) {
  base::Value in;
  ASSERT_FALSE(in.is_dict());

  base::Value out;
  EXPECT_DCHECK_DEATH(
      mojo::test::SerializeAndDeserialize<mojom::ListValue>(&in, &out));
}

// A deeply nested base::Value should trigger a deserialization error.
TEST(ValuesStructTraitsTest, DeeplyNestedValue) {
  base::Value in;
  for (int i = 0; i < 100; ++i) {
    base::Value::ListStorage storage;
    storage.emplace_back(std::move(in));
    in = base::Value(std::move(storage));
  }
  base::Value out;
  ASSERT_FALSE(mojo::test::SerializeAndDeserialize<mojom::Value>(&in, &out));
}

}  // namespace mojo_base
