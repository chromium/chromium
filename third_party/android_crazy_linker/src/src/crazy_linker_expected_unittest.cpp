// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_expected.h"

#include <gtest/gtest.h>

#include <memory>
#include <new>

namespace crazy {

TEST(Expected, ValueConstructor) {
  Expected<int> e{10};
  ASSERT_TRUE(e);
  ASSERT_TRUE(e.has_value());
  ASSERT_FALSE(e.has_error());
  ASSERT_EQ(10, *e);
  ASSERT_EQ(10, e.value());
  ASSERT_EQ(10, e.value_or(20));
}

TEST(Expected, ErrorConstructor) {
  Error error;
  Expected<int> e(&error);
  ASSERT_FALSE(e);
  ASSERT_FALSE(e.has_value());
  ASSERT_TRUE(e.has_error());
  ASSERT_EQ(10, e.value_or(10));
  ASSERT_EQ(20, e.value_or(20));
  ASSERT_EQ(&error, e.error());

  int v = 10;
  Expected<int*> e2(&v);
  ASSERT_TRUE(e2);
  ASSERT_EQ(&v, *e2);
}

TEST(Expectde, ValueMovesRValue) {
  auto create_value = [](int value) {
    return Expected<std::unique_ptr<int>>(std::make_unique<int>(value));
  };

  std::unique_ptr<int> ptr = create_value(10).value();
  ASSERT_TRUE(ptr);
  ASSERT_EQ(10, *ptr);
}

TEST(Expected, DeathInCaseOfInvalidUsage) {
  Expected<int> e{10};
  ASSERT_TRUE(e);
  ASSERT_TRUE(e.has_value());
  ASSERT_FALSE(e.has_error());
  ASSERT_DEATH(e.error(), "No error in Expected<> instance!");

  Error error;
  Expected<int> e2(&error);
  ASSERT_FALSE(e2);
  ASSERT_FALSE(e2.has_value());
  ASSERT_TRUE(e2.has_error());
  ASSERT_DEATH(*e2, "No value in Expected<> instance!");
  ASSERT_DEATH(e2.value(), "No value in Expected<> instance!");
}

}  // namespace crazy
