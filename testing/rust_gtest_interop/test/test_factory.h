// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_RUST_GTEST_INTEROP_TEST_TEST_FACTORY_H_
#define TESTING_RUST_GTEST_INTEROP_TEST_TEST_FACTORY_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/rust_gtest_interop/rust_gtest_interop.h"

namespace rust_gtest_interop {

class TestSubclass : public testing::Test {
 public:
  TestSubclass();

  static size_t num_created();
};

// A custom Gtest factory that returns a `TestSubclass`.
extern "C" testing::Test* test_subclass_factory(void (*body)()) {
  return rust_gtest_interop::rust_gtest_factory_for_subclass<TestSubclass>(
      body);
}

// Returns how many times the test_subclass_factory() function was called.
extern "C" size_t num_subclass_created() {
  return TestSubclass::num_created();
}

}  // namespace rust_gtest_interop

#endif  // TESTING_RUST_GTEST_INTEROP_TEST_TEST_FACTORY_H_
