// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/queued_task.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"

namespace blink {

namespace {

constexpr base::TimeDelta kMetronomeTick = base::Hertz(64);

class MetronomeSourceTest : public ::testing::Test {
 public:
  MetronomeSourceTest()
      : task_environment_(
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        metronome_source_(
            base::MakeRefCounted<MetronomeSource>(kMetronomeTick)) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MetronomeSource> metronome_source_;
};

// Fake task queue for testing WebRtcMetronomeAdapter.
class FakeTaskQueue : public webrtc::TaskQueueBase {
 public:
  explicit FakeTaskQueue(bool retain_tasks)
      : retain_tasks_(retain_tasks),
        runner_(base::ThreadPool::CreateSequencedTaskRunner({})) {}

  void Delete() override { NOTREACHED(); }

  void PostDelayedTask(std::unique_ptr<webrtc::QueuedTask> task,
                       uint32_t milliseconds) override {
    NOTREACHED();
  }

  void PostTask(std::unique_ptr<webrtc::QueuedTask> task) override {
    last_task_ = std::move(task);
    if (!retain_tasks_)
      PostLastTask();
  }

  void PostLastTask() {
    if (last_task_) {
      runner_->PostTask(FROM_HERE,
                        base::BindOnce(
                            [](FakeTaskQueue* thiz,
                               std::unique_ptr<webrtc::QueuedTask> task) {
                              if (!task->Run())
                                task.release();
                            },
                            base::Unretained(this), std::move(last_task_)));
    }
  }

 private:
  const bool retain_tasks_;
  scoped_refptr<base::SequencedTaskRunner> runner_;
  std::unique_ptr<webrtc::QueuedTask> last_task_;
};

class FakeTickListener : public webrtc::Metronome::TickListener {
 public:
  explicit FakeTickListener(base::RepeatingCallback<void()> cb,
                            FakeTaskQueue* queue)
      : cb_(std::move(cb)), queue_(queue) {}

  void OnTick() override { cb_.Run(); }

  webrtc::TaskQueueBase* OnTickTaskQueue() override { return queue_; }

 private:
  base::RepeatingCallback<void()> cb_;
  FakeTaskQueue* queue_;
};

// Helper class to signal to a bool that it was destroyed. Used to test lifetime
// of base::RepatingCallback<> functions.
class DestructorSetter {
 public:
  explicit DestructorSetter(bool* flag) : flag_(flag) {}
  ~DestructorSetter() { *flag_ = true; }

  bool* flag() const { return flag_; }

 private:
  bool* flag_;
};

}  // namespace

TEST_F(MetronomeSourceTest, IsActiveWhenThereAreListeners) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  EXPECT_FALSE(metronome_source_->IsActive());
  // Activate by adding the first listener.
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle0 =
      metronome_source_->AddListener(task_runner, base::BindRepeating([]() {}));
  EXPECT_TRUE(metronome_source_->IsActive());
  // Add a second listener.
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle1 =
      metronome_source_->AddListener(task_runner, base::BindRepeating([]() {}));
  EXPECT_TRUE(metronome_source_->IsActive());
  // Removing the first listener is insufficient to make the source inactive.
  metronome_source_->RemoveListener(listener_handle0);
  EXPECT_TRUE(metronome_source_->IsActive());
  // Removing the the second listener makes the source inactive.
  metronome_source_->RemoveListener(listener_handle1);
  EXPECT_FALSE(metronome_source_->IsActive());

  // An inactive source can become active again. Sanity check that the timer
  // ends up in a working state by ensuring that the callback gets called.
  bool was_called = false;
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle2 =
      metronome_source_->AddListener(
          task_runner,
          base::BindRepeating([](bool* was_called) { *was_called = true; },
                              base::Unretained(&was_called)));
  EXPECT_TRUE(metronome_source_->IsActive());
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_TRUE(was_called);
  metronome_source_->RemoveListener(listener_handle2);
  EXPECT_FALSE(metronome_source_->IsActive());
}

TEST_F(MetronomeSourceTest, ListenerCalledOnItsTaskQueue) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  bool was_called = false;
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle =
      metronome_source_->AddListener(
          task_runner,
          base::BindRepeating(
              [](scoped_refptr<base::SequencedTaskRunner> task_runner,
                 bool* was_called) {
                EXPECT_TRUE(task_runner->RunsTasksInCurrentSequence());
                *was_called = true;
              },
              task_runner, base::Unretained(&was_called)));

  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_TRUE(was_called);

  // Cleanup.
  metronome_source_->RemoveListener(listener_handle);
}

TEST_F(MetronomeSourceTest, ListenerCalledOnEachTick) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  int callback_count = 0;
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle =
      metronome_source_->AddListener(
          task_runner,
          base::BindRepeating([](int* callback_count) { ++(*callback_count); },
                              base::Unretained(&callback_count)));

  EXPECT_EQ(callback_count, 0);
  // Fast-forward slightly less than a tick should not increment the counter.
  task_environment_.FastForwardBy(kMetronomeTick - base::Milliseconds(1));
  EXPECT_EQ(callback_count, 0);
  // Fast-forward to the first tick.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(callback_count, 1);
  // Fast-forward some more ticks.
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 2);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 3);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 4);

  // Cleanup.
  metronome_source_->RemoveListener(listener_handle);
}

TEST_F(MetronomeSourceTest, RemovedListenerStopsFiring) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  int callback_count = 0;
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle =
      metronome_source_->AddListener(
          task_runner,
          base::BindRepeating([](int* callback_count) { ++(*callback_count); },
                              base::Unretained(&callback_count)));

  metronome_source_->RemoveListener(listener_handle);
  // The listener should be removed and not increment the counter.
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 0);
}

TEST_F(MetronomeSourceTest, RemovingListenerDestroysCallback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  // This flag will be set when DestructorSetter is destroyed.
  bool callback_destroyed = false;
  DestructorSetter* destructor_setter =
      new DestructorSetter(&callback_destroyed);

  // Add a listener where the binding takes ownership of DestructorSetter.
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle =
      metronome_source_->AddListener(
          task_runner,
          base::BindRepeating(
              [](DestructorSetter*) {},
              // Pass ownership of |destructor_setter| to the binding. This
              // ensures it is destroyed when the callback is destroyed.
              base::Owned(destructor_setter)));

  EXPECT_FALSE(callback_destroyed);
  // Removing the listener and not referencing it should destroy the callback.
  metronome_source_->RemoveListener(listener_handle);
  listener_handle = nullptr;
  EXPECT_TRUE(callback_destroyed);
}

TEST_F(MetronomeSourceTest, ListenerWithInitialDelay) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  // Set wakeup time to two ticks from now.
  base::TimeTicks wakeup_time = base::TimeTicks::Now() + kMetronomeTick * 2;

  bool was_called = false;
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle =
      metronome_source_->AddListener(
          task_runner,
          base::BindRepeating([](bool* was_called) { *was_called = true; },
                              base::Unretained(&was_called)),
          wakeup_time);

  EXPECT_FALSE(was_called);
  // One trick is not sufficient for callback to be invoked.
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_FALSE(was_called);
  // On the second tick, the callback should be invoked.
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_TRUE(was_called);

  // Cleanup.
  metronome_source_->RemoveListener(listener_handle);
}

TEST_F(MetronomeSourceTest, SetWakeupTime) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  int callback_count = 0;
  scoped_refptr<MetronomeSource::ListenerHandle> listener_handle =
      metronome_source_->AddListener(
          task_runner,
          base::BindRepeating([](int* callback_count) { ++(*callback_count); },
                              base::Unretained(&callback_count)));

  EXPECT_EQ(callback_count, 0);

  // Wakeup next tick.
  listener_handle->SetWakeupTime(base::TimeTicks::Now() + kMetronomeTick);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 1);

  // Not setting another wakeup results in being idle.
  task_environment_.FastForwardBy(kMetronomeTick * 10);
  EXPECT_EQ(callback_count, 1);

  // Wakeup 3 ticks from now.
  listener_handle->SetWakeupTime(base::TimeTicks::Now() + kMetronomeTick * 3);
  task_environment_.FastForwardBy(kMetronomeTick * 2);
  EXPECT_EQ(callback_count, 1);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 2);

  // Wakeup slightly less than a tick from now.
  listener_handle->SetWakeupTime(base::TimeTicks::Now() + kMetronomeTick -
                                 base::Milliseconds(1));
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 3);

  // Wakeup slightly more than a tick from now.
  listener_handle->SetWakeupTime(base::TimeTicks::Now() + kMetronomeTick +
                                 base::Milliseconds(1));
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 3);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 4);

  // Wakeup every tick (stop being temporarily idle).
  listener_handle->SetWakeupTime(base::TimeTicks::Min());
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 5);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 6);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(callback_count, 7);

  // Cleanup.
  metronome_source_->RemoveListener(listener_handle);
}

TEST_F(MetronomeSourceTest, WebRtcMetronomeAdapterTickPeriod) {
  EXPECT_EQ(kMetronomeTick.InMicroseconds(),
            metronome_source_->CreateWebRtcMetronome()->TickPeriod().us());
}

TEST_F(MetronomeSourceTest, WebRtcMetronomeAdapterAddsHandler) {
  int callback_count = 0;
  FakeTaskQueue fake_queue(false);
  auto tick_listener = FakeTickListener(
      base::BindRepeating([](int* callback_count) { ++(*callback_count); },
                          base::Unretained(&callback_count)),
      &fake_queue);
  auto metronome_adapter = metronome_source_->CreateWebRtcMetronome();

  EXPECT_FALSE(metronome_source_->IsActive());
  metronome_adapter->AddListener(&tick_listener);
  EXPECT_TRUE(metronome_source_->IsActive());

  // Next tick should trigger callback.
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(1, callback_count);

  // Each tick should trigger callback.
  task_environment_.FastForwardBy(kMetronomeTick * 10);
  EXPECT_EQ(11, callback_count);

  // Removing should not fire callback.
  metronome_adapter->RemoveListener(&tick_listener);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(11, callback_count);

  // Removing should inactivate metronome source.
  EXPECT_FALSE(metronome_source_->IsActive());
}

TEST_F(MetronomeSourceTest,
       WebRtcMetronomeAdapterDoesNotExecuteListenerAfterRemoval) {
  int callback_count = 0;

  FakeTaskQueue fake_queue(true);
  auto tick_listener = FakeTickListener(
      base::BindRepeating([](int* callback_count) { ++(*callback_count); },
                          base::Unretained(&callback_count)),
      &fake_queue);
  auto metronome_adapter = metronome_source_->CreateWebRtcMetronome();

  metronome_adapter->AddListener(&tick_listener);
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(0, callback_count);
  // Now task should have been posted to fake queue. Running it should increase
  // the callback count.
  fake_queue.PostLastTask();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, callback_count);

  // Simulate scenario where fake_queue will run it's task after
  // `RemoveListener` has been called. This occurs when `RemoveListener` is
  // called after the metronome has ticked but before the task has been run on
  // the queue.
  task_environment_.FastForwardBy(kMetronomeTick);
  EXPECT_EQ(1, callback_count);

  metronome_adapter->RemoveListener(&tick_listener);
  fake_queue.PostLastTask();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, callback_count);
}

}  // namespace blink
