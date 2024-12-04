// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/callback_helpers.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {

void SetBool(bool* var, bool val) {
  *var = val;
}

void SetBoolFromRawPtr(bool* var, bool* val) {
  *var = *val;
}

void SetIntegers(int* a_var, int* b_var, int a_val, int b_val) {
  *a_var = a_val;
  *b_var = b_val;
}

void SetIntegerFromUniquePtr(int* var, std::unique_ptr<int> val) {
  *var = *val;
}

void SetString(std::string* var, const std::string val) {
  *var = val;
}

void CallClosure(base::OnceClosure cl) {
  std::move(cl).Run();
}

}  // namespace

TEST(CallbackWithDeleteTest, SetIntegers_Run) {
  int a = 0;
  int b = 0;
  auto cb =
      WrapCallbackWithDropHandler(base::BindOnce(&SetIntegers, &a, &b),
                                  base::BindOnce(&SetIntegers, &a, &b, 3, 4));
  std::move(cb).Run(1, 2);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2);
}

TEST(CallbackWithDeleteTest, SetIntegers_Destruction) {
  int a = 0;
  int b = 0;
  {
    auto cb =
        WrapCallbackWithDropHandler(base::BindOnce(&SetIntegers, &a, &b),
                                    base::BindOnce(&SetIntegers, &a, &b, 3, 4));
  }
  EXPECT_EQ(a, 3);
  EXPECT_EQ(b, 4);
}

TEST(CallbackWithDefaultTest, CallClosure_Run) {
  int a = 0;
  int b = 0;
  auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&CallClosure), base::BindOnce(&SetIntegers, &a, &b, 3, 4));
  std::move(cb).Run(base::BindOnce(&SetIntegers, &a, &b, 1, 2));
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2);
}

TEST(CallbackWithDefaultTest, CallClosure_Destruction) {
  int a = 0;
  int b = 0;
  {
    auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&CallClosure),
        base::BindOnce(&SetIntegers, &a, &b, 3, 4));
  }
  EXPECT_EQ(a, 3);
  EXPECT_EQ(b, 4);
}

TEST(CallbackWithDefaultTest, Closure_Run) {
  bool a = false;
  auto cb =
      WrapCallbackWithDefaultInvokeIfNotRun(base::BindOnce(&SetBool, &a, true));
  std::move(cb).Run();
  EXPECT_TRUE(a);
}

TEST(CallbackWithDefaultTest, Closure_Destruction) {
  bool a = false;
  {
    auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&SetBool, &a, true));
  }
  EXPECT_TRUE(a);
}

TEST(CallbackWithDefaultTest, SetBool_Run) {
  bool a = false;
  auto cb =
      WrapCallbackWithDefaultInvokeIfNotRun(base::BindOnce(&SetBool, &a), true);
  std::move(cb).Run(true);
  EXPECT_TRUE(a);
}

TEST(CallbackWithDefaultTest, SetBoolFromRawPtr_Run) {
  bool a = false;
  bool* b = new bool(false);
  bool c = true;
  auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&SetBoolFromRawPtr, &a), base::Owned(b));
  std::move(cb).Run(&c);
  EXPECT_TRUE(a);
}

TEST(CallbackWithDefaultTest, SetBoolFromRawPtr_Destruction) {
  bool a = false;
  bool* b = new bool(true);
  {
    auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&SetBoolFromRawPtr, &a), base::Owned(b));
  }
  EXPECT_TRUE(a);
}

TEST(CallbackWithDefaultTest, SetBool_Destruction) {
  bool a = false;
  {
    auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&SetBool, &a), true);
  }
  EXPECT_TRUE(a);
}

TEST(CallbackWithDefaultTest, SetIntegers_Run) {
  int a = 0;
  int b = 0;
  auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&SetIntegers, &a, &b), 3, 4);
  std::move(cb).Run(1, 2);
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, 2);
}

TEST(CallbackWithDefaultTest, SetIntegers_Destruction) {
  int a = 0;
  int b = 0;
  {
    auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&SetIntegers, &a, &b), 3, 4);
  }
  EXPECT_EQ(a, 3);
  EXPECT_EQ(b, 4);
}

TEST(CallbackWithDefaultTest, SetIntegerFromUniquePtr_Run) {
  int a = 0;
  auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&SetIntegerFromUniquePtr, &a), std::make_unique<int>(1));
  std::move(cb).Run(std::make_unique<int>(2));
  EXPECT_EQ(a, 2);
}

TEST(CallbackWithDefaultTest, SetIntegerFromUniquePtr_Destruction) {
  int a = 0;
  {
    auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&SetIntegerFromUniquePtr, &a), std::make_unique<int>(1));
  }
  EXPECT_EQ(a, 1);
}

TEST(CallbackWithDefaultTest, SetString_Run) {
  std::string a;
  auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&SetString, &a), "hello");
  std::move(cb).Run("world");
  EXPECT_EQ(a, "world");
}

TEST(CallbackWithDefaultTest, SetString_Destruction) {
  std::string a;
  {
    auto cb = WrapCallbackWithDefaultInvokeIfNotRun(
        base::BindOnce(&SetString, &a), "hello");
  }
  EXPECT_EQ(a, "hello");
}

}  // namespace mojo
