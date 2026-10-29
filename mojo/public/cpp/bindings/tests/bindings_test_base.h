// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_BINDINGS_TEST_BASE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_BINDINGS_TEST_BASE_H_

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

class BindingsTestBase : public testing::Test {
 public:
  BindingsTestBase();
  ~BindingsTestBase() override;

 protected:
  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_BINDINGS_TEST_BASE_H_
