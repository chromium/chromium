// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

#include <utility>
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

// Tests that "currying" CrossThreadFunction and CrossThreadOnceFunction works,
// as it does with the base counterparts.

struct SomeFunctor;

static_assert(std::is_same<decltype(internal::CoerceFunctorForCrossThreadBind(
                               std::declval<SomeFunctor&>())),
                           SomeFunctor&>(),
              "functor coercion should not affect Functor lvalue ref type");
static_assert(std::is_same<decltype(internal::CoerceFunctorForCrossThreadBind(
                               std::declval<SomeFunctor>())),
                           SomeFunctor&&>(),
              "functor coercion should not affect Functor rvalue ref type");

TEST(CrossThreadFunctionalTest, CrossThreadBindRepeating_CrossThreadFunction) {
  auto adder = CrossThreadBindRepeating([](int x, int y) { return x + y; });
  auto five_adder = CrossThreadBindRepeating(std::move(adder), 5);
  EXPECT_EQ(five_adder.Run(7), 12);
}

TEST(CrossThreadFunctionalTest, CrossThreadBindOnce_CrossThreadOnceFunction) {
  auto adder = CrossThreadBindOnce([](int x, int y) { return x + y; });
  auto five_adder = CrossThreadBindOnce(std::move(adder), 5);
  EXPECT_EQ(std::move(five_adder).Run(7), 12);
}

TEST(CrossThreadFunctionalTest, CrossThreadBindOnce_CrossThreadFunction) {
  auto adder = CrossThreadBindRepeating([](int x, int y) { return x + y; });
  auto five_adder = CrossThreadBindOnce(std::move(adder), 5);
  EXPECT_EQ(std::move(five_adder).Run(7), 12);
}

}  // namespace
}  // namespace WTF
