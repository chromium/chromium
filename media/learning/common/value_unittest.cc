// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/learning/common/value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace learning {

class LearnerValueTest : public testing::Test {};

TEST_F(LearnerValueTest, EqualStringsCompareAsEqual) {
  const char* value = "value";
  Value v1(value);
  Value v2(value);
  EXPECT_TRUE(v1 == v2);
}

TEST_F(LearnerValueTest, UnequalStringsDoNotCompareAsEqual) {
  Value v1("value");
  Value v2("nonvalue");
  EXPECT_TRUE(v1 != v2);
}

TEST_F(LearnerValueTest, ConstCharAndStdStringsCompareAsEqual) {
  const char* value_as_char = "value";
  std::string value_as_str(value_as_char);
  Value v1(value_as_char);
  Value v2(value_as_str);
  EXPECT_TRUE(v1 == v2);
}

TEST_F(LearnerValueTest, IntsCompareCorrectly) {
  const int i1 = 12345;
  const int i2 = 67890;
  Value v1(i1);
  Value v2(i1);
  Value v3(i2);
  EXPECT_TRUE(v1 == v2);
  EXPECT_TRUE(v1 != v3);
  EXPECT_TRUE(v1 < v3);
  EXPECT_FALSE(v3 < v1);
  EXPECT_FALSE(v3 < v3);
  EXPECT_FALSE(v1 < v1);
  EXPECT_TRUE(v3 > v1);
  EXPECT_FALSE(v1 > v3);
  EXPECT_FALSE(v1 > v1);
  EXPECT_FALSE(v3 > v3);
}

TEST_F(LearnerValueTest, VariousTypesWork) {
  EXPECT_EQ(Value(1.2).value(), 1.2);
  EXPECT_EQ(Value(10).value(), 10);
  EXPECT_EQ(Value(static_cast<int64_t>(-10)).value(), -10);
  EXPECT_EQ(Value(static_cast<uint64_t>(10)).value(), 10);
  EXPECT_EQ(Value(true).value(), 1.0);
  EXPECT_EQ(Value(false).value(), 0.0);
}

}  // namespace learning
}  // namespace media
