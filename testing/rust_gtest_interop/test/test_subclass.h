// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_RUST_GTEST_INTEROP_TEST_TEST_SUBCLASS_H_
#define TESTING_RUST_GTEST_INTEROP_TEST_TEST_SUBCLASS_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/rust_gtest_interop/rust_gtest_interop.h"

namespace rust_gtest_interop {

class TestSubclass : public testing::Test {
 public:
  TestSubclass();

  bool get_true() {
    ++calls_;
    return true;
  }
  bool get_false() {
    ++calls_;
    return false;
  }
  int32_t num_calls() const { return calls_; }

 private:
  int32_t calls_ = 0;
};

}  // namespace rust_gtest_interop

#endif  // TESTING_RUST_GTEST_INTEROP_TEST_TEST_SUBCLASS_H_
