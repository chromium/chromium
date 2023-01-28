// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "extensions/common/value_counter.h"
#include "testing/gtest/include/gtest/gtest.h"

using ValueCounterTest = testing::Test;

namespace extensions {

TEST_F(ValueCounterTest, TestAddingSameValue) {
  ValueCounter vc;
  base::Value value(base::Value::Type::LIST);
  ASSERT_TRUE(vc.Add(value));
  ASSERT_FALSE(vc.Add(value));
}

TEST_F(ValueCounterTest, TestAddingDifferentValue) {
  ValueCounter vc;
  base::Value value1(base::Value::Type::LIST);
  base::Value value2(base::Value::Type::DICT);
  ASSERT_TRUE(vc.Add(value1));
  ASSERT_TRUE(vc.Add(value2));
}

TEST_F(ValueCounterTest, TestRemovingSameValue) {
  ValueCounter vc;
  base::Value value(base::Value::Type::LIST);
  vc.Add(value);
  vc.Add(value);
  ASSERT_FALSE(vc.Remove(value));
  ASSERT_TRUE(vc.Remove(value));
  ASSERT_FALSE(vc.Remove(value));
}

TEST_F(ValueCounterTest, TestReAddingSameValue) {
  ValueCounter vc;
  base::Value value(base::Value::Type::LIST);
  ASSERT_FALSE(vc.Remove(value));
  ASSERT_TRUE(vc.Add(value));
  ASSERT_TRUE(vc.Remove(value));
  ASSERT_TRUE(vc.Add(value));
  ASSERT_TRUE(vc.Remove(value));
  ASSERT_FALSE(vc.Remove(value));
}

TEST_F(ValueCounterTest, TestIsEmpty) {
  ValueCounter vc;
  base::Value value1(base::Value::Type::LIST);
  base::Value value2(base::Value::Type::DICT);
  ASSERT_TRUE(vc.is_empty());
  vc.Add(value1);
  ASSERT_FALSE(vc.is_empty());
  vc.Remove(value1);
  ASSERT_TRUE(vc.is_empty());
  vc.Add(value1);
  vc.Add(value2);
  ASSERT_FALSE(vc.is_empty());
  vc.Remove(value1);
  ASSERT_FALSE(vc.is_empty());
  vc.Remove(value2);
  ASSERT_TRUE(vc.is_empty());
}

}  // namespace extensions
