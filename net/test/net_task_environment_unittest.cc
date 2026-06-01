// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/task/task_runner.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class NetTaskEnvironmentTest : public ::testing::TestWithParam<bool> {
 public:
  NetTaskEnvironmentTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(features::kNetTaskScheduler);
    } else {
      feature_list_.InitAndDisableFeature(features::kNetTaskScheduler);
    }
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(NetTaskEnvironmentTest, Basic) {
  NetTaskEnvironment task_environment;

  // The main thread should have a task runner.
  EXPECT_TRUE(base::SingleThreadTaskRunner::HasCurrentDefault());

  if (GetParam()) {
    // When enabled, net::GetTaskRunner should return a valid runner.
    EXPECT_TRUE(GetTaskRunner(RequestPriority::HIGHEST));
    EXPECT_TRUE(GetTaskRunner(RequestPriority::LOWEST));

    // Verify that the global task runner is redirected.
    // net::GetTaskRunner(DEFAULT_PRIORITY) should match the current default.
    EXPECT_EQ(GetTaskRunner(RequestPriority::DEFAULT_PRIORITY),
              base::SingleThreadTaskRunner::GetCurrentDefault());

    // net::GetTaskRunner(HIGHEST) should be different from the default (it has
    // its own queue).
    EXPECT_NE(GetTaskRunner(RequestPriority::HIGHEST),
              base::SingleThreadTaskRunner::GetCurrentDefault());
  } else {
    // When disabled, net::GetTaskRunner should still return the current default
    // (fallback).
    EXPECT_EQ(GetTaskRunner(RequestPriority::HIGHEST),
              base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

INSTANTIATE_TEST_SUITE_P(All, NetTaskEnvironmentTest, ::testing::Bool());



}  // namespace net
