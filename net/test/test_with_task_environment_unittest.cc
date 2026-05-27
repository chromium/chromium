// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_with_task_environment.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/task/task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// A local test-only subclass that exposes the protected WithTaskEnvironment
// constructor as a public API.
//
// Direct instantiation of WithTaskEnvironment is normally blocked (constructor
// is protected) to enforce the Test Fixture inheritance pattern.
//
// However, because C++ base classes are constructed before member variables,
// we cannot inherit from WithTaskEnvironment as a fixture here; the mock
// ScopedFeatureList must be initialized BEFORE the task environment is created.
// Thus, we must instantiate the environment locally inside each test body using
// this dummy subclass.
class DummyWithTaskEnvironment : public WithTaskEnvironment {
 public:
  DummyWithTaskEnvironment() = default;
};

class WithTaskEnvironmentTest : public ::testing::TestWithParam<bool> {
 public:
  WithTaskEnvironmentTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kNetTaskScheduler);
    } else {
      feature_list_.InitAndDisableFeature(features::kNetTaskScheduler);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that enabling the primary `kNetTaskScheduler` feature flag actively
// redirects net task runners (specifically, the HIGHEST priority queue) away
// from the default single thread task runner fallback.
TEST_P(WithTaskEnvironmentTest, SchedulerEnabled) {
  DummyWithTaskEnvironment task_environment;

  if (GetParam()) {
    // When kNetTaskScheduler is enabled, HIGHEST should be different from
    // default.
    EXPECT_NE(GetTaskRunner(RequestPriority::HIGHEST),
              base::SingleThreadTaskRunner::GetCurrentDefault());
  } else {
    // When disabled, it should be the same (fallback).
    EXPECT_EQ(GetTaskRunner(RequestPriority::HIGHEST),
              base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

INSTANTIATE_TEST_SUITE_P(All, WithTaskEnvironmentTest, ::testing::Bool());

class WithTaskEnvironmentSchedulerInTestsTest : public ::testing::Test {
 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies the global test-only kill-switch (`kNetTaskSchedulerInTests`).
// Even if the primary `kNetTaskScheduler` feature is enabled, disabling the
// test-only switch must completely bypass the NetTaskScheduler inside unit
// tests, falling back to the standard default task runner.
TEST_F(WithTaskEnvironmentSchedulerInTestsTest, DisabledInTests) {
  feature_list_.InitWithFeatures({features::kNetTaskScheduler},
                                 {features::kNetTaskSchedulerInTests});

  DummyWithTaskEnvironment task_environment;

  EXPECT_EQ(GetTaskRunner(RequestPriority::HIGHEST),
            base::SingleThreadTaskRunner::GetCurrentDefault());
}

// Verifies that the NetTaskScheduler is successfully initialized in tests
// when both the primary `kNetTaskScheduler` feature flag and the test-only
// switch (`kNetTaskSchedulerInTests`) are enabled.
TEST_F(WithTaskEnvironmentSchedulerInTestsTest, EnabledInTests) {
  feature_list_.InitWithFeatures(
      {features::kNetTaskScheduler, features::kNetTaskSchedulerInTests}, {});

  DummyWithTaskEnvironment task_environment;

  EXPECT_NE(GetTaskRunner(RequestPriority::HIGHEST),
            base::SingleThreadTaskRunner::GetCurrentDefault());
}

}  // namespace
}  // namespace net
