// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_task_queue_factory.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_test.h"
#include "third_party/webrtc/rtc_base/task_utils/to_queued_task.h"
#include "third_party/webrtc_overrides/metronome_source.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::webrtc::TaskQueueTest;

constexpr base::TimeDelta kMetronomeTick = base::Hertz(64);

// Wrapper needed for the TaskQueueTest suite.
class TestMetronomeTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  TestMetronomeTaskQueueFactory()
      : metronome_source_(
            base::MakeRefCounted<blink::MetronomeSource>(kMetronomeTick)),
        factory_(CreateWebRtcMetronomeTaskQueueFactory(metronome_source_)) {}

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(absl::string_view name, Priority priority) const override {
    return factory_->CreateTaskQueue(name, priority);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<blink::MetronomeSource> metronome_source_;
  std::unique_ptr<webrtc::TaskQueueFactory> factory_;
};

// Instantiate suite to run all tests defined in
// /third_party/webrtc/api/task_queue/task_queue_test.h.
INSTANTIATE_TEST_SUITE_P(
    WebRtcMetronomeTaskQueue,
    TaskQueueTest,
    ::testing::Values(std::make_unique<TestMetronomeTaskQueueFactory>));

class MetronomeTaskQueueFactoryTest : public ::testing::Test {
 public:
  MetronomeTaskQueueFactoryTest()
      : task_environment_(
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        metronome_source_(
            base::MakeRefCounted<MetronomeSource>(kMetronomeTick)),
        task_queue_factory_(
            CreateWebRtcMetronomeTaskQueueFactory(metronome_source_)) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MetronomeSource> metronome_source_;
  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
};

TEST_F(MetronomeTaskQueueFactoryTest, PostTaskRunsPriorToTick) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  bool did_run = false;
  task_queue->PostTask(webrtc::ToQueuedTask([&did_run]() { did_run = true; }));

  EXPECT_FALSE(did_run);
  task_environment_.FastForwardBy(base::Nanoseconds(1));
  EXPECT_TRUE(did_run);
}

TEST_F(MetronomeTaskQueueFactoryTest, NormalPriorityDelayedTasksRunOnTicks) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  int task_run_counter = 0;

  // Delay task until next tick.
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&task_run_counter]() { ++task_run_counter; }),
      kMetronomeTick.InMilliseconds());
  EXPECT_EQ(task_run_counter, 0);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(task_run_counter, 1);

  // Delay half a tick. A full tick must pass before it runs.
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&task_run_counter]() { ++task_run_counter; }),
      (kMetronomeTick / 2).InMilliseconds());
  task_environment_.FastForwardBy(kMetronomeTick - base::Milliseconds(1));
  EXPECT_EQ(task_run_counter, 1);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(task_run_counter, 2);

  // Delay several ticks.
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&task_run_counter]() { ++task_run_counter; }),
      (kMetronomeTick * 3).InMilliseconds());
  task_environment_.FastForwardBy(kMetronomeTick * 2);
  EXPECT_EQ(task_run_counter, 2);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(task_run_counter, 3);
}

TEST_F(MetronomeTaskQueueFactoryTest, HighPriorityDelayedTasksRunOffTicks) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::HIGH);

  bool did_run = false;
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&did_run]() { did_run = true; }),
      /*milliseconds=*/1);

  EXPECT_FALSE(did_run);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(did_run);
}

TEST_F(MetronomeTaskQueueFactoryTest, DelayedTasksRunInOrder) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  constexpr uint32_t kTime0Ms = 0;
  constexpr uint32_t kTime1Ms = 1;
  std::vector<std::string> run_tasks;

  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time0_First");
                              }),
                              kTime0Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time1_First");
                              }),
                              kTime1Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time1_Second");
                              }),
                              kTime1Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time0_Second");
                              }),
                              kTime0Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time0_Third");
                              }),
                              kTime0Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time1_Third");
                              }),
                              kTime1Ms);
  task_environment_.FastForwardBy(kMetronomeTick);

  EXPECT_THAT(run_tasks,
              ElementsAre("Time0_First", "Time0_Second", "Time0_Third",
                          "Time1_First", "Time1_Second", "Time1_Third"));
}

TEST_F(MetronomeTaskQueueFactoryTest, DelayedTaskCanPostDelayedTask) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  bool task0_ran = false;
  bool task1_ran = false;

  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&task_queue, &task0_ran, &task1_ran]() {
        task0_ran = true;
        // Inception!
        task_queue->PostDelayedTask(
            webrtc::ToQueuedTask([&task1_ran]() { task1_ran = true; }),
            kMetronomeTick.InMilliseconds());
      }),
      kMetronomeTick.InMilliseconds());

  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_TRUE(task0_ran);
  EXPECT_FALSE(task1_ran);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_TRUE(task0_ran);
  EXPECT_TRUE(task1_ran);
}

}  // namespace

}  // namespace blink
