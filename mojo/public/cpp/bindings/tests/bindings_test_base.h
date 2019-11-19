// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_BINDINGS_TEST_BASE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_BINDINGS_TEST_BASE_H_

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

// Used to parameterize tests which inherit from BindingsTestBase to exercise
// various message serialization-related code paths for intra-process bindings
// usage.
enum class BindingsTestSerializationMode {
  // Messages should be serialized immediately before sending.
  kSerializeBeforeSend,

  // Messages should be serialized immediately before dispatching.
  kSerializeBeforeDispatch,

  // Messages should never be serialized.
  kNeverSerialize,
};

class BindingsTestBase
    : public testing::Test,
      public testing::WithParamInterface<BindingsTestSerializationMode> {
 public:
  BindingsTestBase();
  ~BindingsTestBase();

  // Helper which other test fixtures can use.
  static void SetupSerializationBehavior(BindingsTestSerializationMode mode);

 protected:
  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace mojo

#define INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(fixture)                  \
  INSTANTIATE_TEST_SUITE_P(                                              \
      , fixture,                                                         \
      testing::Values(                                                   \
          mojo::BindingsTestSerializationMode::kSerializeBeforeSend,     \
          mojo::BindingsTestSerializationMode::kSerializeBeforeDispatch, \
          mojo::BindingsTestSerializationMode::kNeverSerialize))

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_BINDINGS_TEST_BASE_H_
