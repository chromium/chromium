// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/task_queue_factory.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/field_trials_view.h"
#include "third_party/webrtc/api/task_queue/task_queue_test.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace blink {

namespace {

using ::webrtc::TaskQueueTest;

// Test-only factory needed for the TaskQueueTest suite.
class TestTaskQueueFactory final : public webrtc::TaskQueueFactory {
 public:
  TestTaskQueueFactory() : factory_(CreateWebRtcTaskQueueFactory()) {}

  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>
  CreateTaskQueue(std::string_view name, Priority priority) const override {
    return factory_->CreateTaskQueue(name, priority);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<webrtc::TaskQueueFactory> factory_;
};

std::unique_ptr<webrtc::TaskQueueFactory> CreateTaskQueueFactory(
    const webrtc::FieldTrialsView*) {
  return std::make_unique<TestTaskQueueFactory>();
}

// Instantiate suite to run all tests defined in
// third_party/webrtc/api/task_queue/task_queue_test.h.
INSTANTIATE_TEST_SUITE_P(WebRtcTaskQueue,
                         TaskQueueTest,
                         ::testing::Values(CreateTaskQueueFactory));

// Provider needed for the MetronomeLikeTaskQueueTest suite.
class TaskQueueProvider : public MetronomeLikeTaskQueueProvider {
 public:
  void Initialize() override {
    task_queue_ = CreateWebRtcTaskQueueFactory()->CreateTaskQueue(
        "TestTaskQueue", webrtc::TaskQueueFactory::Priority::NORMAL);
  }

  base::TimeDelta DeltaToNextTick() const override {
    base::TimeTicks now = base::TimeTicks::Now();
    return TimerBasedTickProvider::TimeSnappedToNextTick(
               now, TimerBasedTickProvider::kDefaultPeriod) -
           now;
  }
  base::TimeDelta MetronomeTick() const override {
    return TimerBasedTickProvider::kDefaultPeriod;
  }
  webrtc::TaskQueueBase* TaskQueue() const override {
    return task_queue_.get();
  }

 private:
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> task_queue_;
};

// Instantiate suite to run all tests defined in
// third_party/webrtc_overrides/test/metronome_like_task_queue_test.h
INSTANTIATE_TEST_SUITE_P(
    WebRtcTaskQueue,
    MetronomeLikeTaskQueueTest,
    ::testing::Values(std::make_unique<TaskQueueProvider>));

}  // namespace

}  // namespace blink
