// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/rust_gtest_interop/rust_gtest_interop.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace rust_gtest_interop {

namespace {

// The C++ test fixture used for all Rust unit tests. It provides nothing except
// the test body which calls thr Rust function.
class RustTest : public testing::Test {
 public:
  explicit RustTest(void (&test_fn)()) : test_fn_(test_fn) {}
  void TestBody() override { test_fn_(); }

 private:
  void (&test_fn_)();
};

}  // namespace

void rust_gtest_add_test(void (*test_fn)(),
                         const char* test_suite_name,
                         const char* test_name,
                         const char* file,
                         int32_t line) {
  auto factory = [=]() -> testing::Test* { return new RustTest(*test_fn); };
  testing::RegisterTest(test_suite_name, test_name, nullptr, nullptr, file,
                        line, factory);
}

void rust_gtest_add_failure_at(const unsigned char* file,
                               int32_t line,
                               rust::Str message) {
  ADD_FAILURE_AT(reinterpret_cast<const char*>(file), line) << message;
}

}  // namespace rust_gtest_interop
