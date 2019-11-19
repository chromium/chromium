// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/compositor_thread_scheduler.h"
#include <algorithm>
#include <memory>
#include "base/bind.h"
#include "base/macros.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

using testing::ElementsAre;
using testing::ElementsAreArray;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace compositor_thread_scheduler_unittest {

class CompositorThreadSchedulerTest : public testing::Test {
 public:
  CompositorThreadSchedulerTest(
      std::vector<base::Feature> features_to_enable,
      std::vector<base::Feature> features_to_disable) {
    feature_list_.InitWithFeatures(features_to_enable, features_to_disable);
  }

  CompositorThreadSchedulerTest() : CompositorThreadSchedulerTest({}, {}) {}

  ~CompositorThreadSchedulerTest() override = default;

  void SetUp() override {
    mock_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    mock_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMicroseconds(5000));
    sequence_manager_ = base::sequence_manager::SequenceManagerForTest::Create(
        nullptr, mock_task_runner_, mock_task_runner_->GetMockTickClock());
    scheduler_ =
        std::make_unique<CompositorThreadScheduler>(sequence_manager_.get());
    scheduler_->Init();
  }

  void TearDown() override {}

 protected:
  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  std::unique_ptr<base::sequence_manager::SequenceManagerForTest>
      sequence_manager_;
  std::unique_ptr<CompositorThreadScheduler> scheduler_;

  DISALLOW_COPY_AND_ASSIGN(CompositorThreadSchedulerTest);
};

class CompositorThreadInputPriorityTest : public CompositorThreadSchedulerTest {
 public:
  CompositorThreadInputPriorityTest()
      : CompositorThreadSchedulerTest(
            {kHighPriorityInputOnCompositorThread} /* features_to_enable */,
            {} /* features_to_disable */) {}
  ~CompositorThreadInputPriorityTest() override = default;
};

namespace {

void RunTestTask(String name, Vector<String>* log) {
  log->push_back(name);
}

}  // namespace

TEST_F(CompositorThreadInputPriorityTest, HighestPriorityInput) {
  Vector<String> run_order;

  scheduler_->DefaultTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunTestTask, "default", base::Unretained(&run_order)));
  scheduler_->InputTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunTestTask, "input", base::Unretained(&run_order)));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("input", "default"));
}

class CompositorThreadNoInputPriorityTest
    : public CompositorThreadSchedulerTest {
 public:
  CompositorThreadNoInputPriorityTest()
      : CompositorThreadSchedulerTest(
            {} /* features_to_enable */,
            {kHighPriorityInputOnCompositorThread} /* features_to_disable */) {}
  ~CompositorThreadNoInputPriorityTest() override = default;
};

TEST_F(CompositorThreadNoInputPriorityTest, InputNotPrioritized) {
  Vector<String> run_order;

  scheduler_->DefaultTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunTestTask, "default", base::Unretained(&run_order)));
  scheduler_->InputTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RunTestTask, "input", base::Unretained(&run_order)));

  mock_task_runner_->RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("default", "input"));
}
}  // namespace compositor_thread_scheduler_unittest
}  // namespace scheduler
}  // namespace blink
