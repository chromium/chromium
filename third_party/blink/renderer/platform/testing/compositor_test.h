// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_COMPOSITOR_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_COMPOSITOR_TEST_H_

#include "base/memory/scoped_refptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class CompositorTest : public testing::Test {
 public:
  CompositorTest();
  CompositorTest(const CompositorTest&) = delete;
  CompositorTest& operator=(const CompositorTest&) = delete;
  ~CompositorTest() override;

 protected:
  // cc::LayerTreeHost requires a task runner, so we use a mock task runner
  // and bind it as the current ThreadTaskRunnerHandle for this thread.
  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_COMPOSITOR_TEST_H_
