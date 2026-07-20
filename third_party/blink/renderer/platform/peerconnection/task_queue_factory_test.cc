// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/task_queue_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
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

TEST(WebRtcTaskQueueTest, DestructorDeleteDeadlock) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<webrtc::TaskQueueFactory> factory =
      CreateWebRtcTaskQueueFactory();
  std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> queue =
      factory->CreateTaskQueue("TestQueue",
                               webrtc::TaskQueueFactory::Priority::NORMAL);

  // Event signaled when the TaskQueue is successfully destroyed.
  base::TestWaitableEvent destroyed_event;
  // Event used to block the task completion until the main thread has
  // released its reference to the trigger.
  base::TestWaitableEvent task_can_complete_event;
  // Event signaled when the task has started running on the worker thread.
  base::TestWaitableEvent task_started_event;

  // Helper class that owns the task queue and deletes it in its destructor.
  class DeadlockTrigger : public base::RefCountedThreadSafe<DeadlockTrigger> {
   public:
    explicit DeadlockTrigger(
        std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> queue,
        base::TestWaitableEvent* destroyed_event)
        : queue_(std::move(queue)), destroyed_event_(destroyed_event) {}

   private:
    friend class base::RefCountedThreadSafe<DeadlockTrigger>;
    ~DeadlockTrigger() {
      // This will call WebRtcTaskQueue::Delete().
      queue_.reset();
      destroyed_event_->Signal();
    }

    std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter> queue_;
    raw_ptr<base::TestWaitableEvent> destroyed_event_;
  };

  webrtc::TaskQueueBase* raw_queue = queue.get();
  scoped_refptr<DeadlockTrigger> trigger =
      base::MakeRefCounted<DeadlockTrigger>(std::move(queue), &destroyed_event);

  // Post a task that captures the trigger.
  raw_queue->PostTask(
      [trigger, &task_started_event, &task_can_complete_event]() {
        task_started_event.Signal();
        // Block here to ensure the task holds its reference to `trigger` and
        // keeps the queue's `alive_lock_` held on the worker thread.
        task_can_complete_event.Wait();
      });

  // Wait for the task to start running and acquire the lock.
  task_started_event.Wait();

  // Release the main thread's reference. The only remaining reference
  // is now held by the blocked task on the worker thread.
  trigger.reset();

  // Allow the task to finish, which will destroy the captured trigger
  // and invoke the destructor (calling Delete()) under the lock.
  task_can_complete_event.Signal();

  // Wait for the queue to be destroyed. If it deadlocks, this will hang.
  destroyed_event.Wait();
}

}  // namespace blink
