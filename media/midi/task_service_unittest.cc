// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/task_service.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace midi {

namespace {

enum {
  kDefaultRunner = TaskService::kDefaultRunnerId,
  kFirstRunner,
  kSecondRunner
};

base::WaitableEvent* GetEvent() {
  static base::WaitableEvent* event =
      new base::WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
  return event;
}

void SignalEvent() {
  GetEvent()->Signal();
}

void WaitEvent() {
  GetEvent()->Wait();
}

void ResetEvent() {
  GetEvent()->Reset();
}

class TaskServiceClient {
 public:
  TaskServiceClient(TaskService* task_service)
      : task_service_(task_service),
        wait_task_event_(std::make_unique<base::WaitableEvent>(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED)),
        count_(0u) {
    DCHECK(task_service);
  }

  bool Bind() { return task_service()->BindInstance(); }

  bool Unbind() { return task_service()->UnbindInstance(); }

  void PostBoundTask(TaskService::RunnerId runner_id) {
    task_service()->PostBoundTask(
        runner_id, base::BindOnce(&TaskServiceClient::IncrementCount,
                                  base::Unretained(this)));
  }

  void PostBoundSignalTask(TaskService::RunnerId runner_id) {
    task_service()->PostBoundTask(
        runner_id, base::BindOnce(&TaskServiceClient::SignalEvent,
                                  base::Unretained(this)));
  }

  void PostBoundWaitTask(TaskService::RunnerId runner_id) {
    wait_task_event_->Reset();
    task_service()->PostBoundTask(
        runner_id,
        base::BindOnce(&TaskServiceClient::WaitEvent, base::Unretained(this)));
  }

  void PostBoundDelayedSignalTask(TaskService::RunnerId runner_id) {
    task_service()->PostBoundDelayedTask(
        runner_id,
        base::BindOnce(&TaskServiceClient::SignalEvent, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(100));
  }

  void WaitTask() { wait_task_event_->Wait(); }

  size_t count() {
    base::AutoLock lock(lock_);
    return count_;
  }

 private:
  TaskService* task_service() { return task_service_; }

  void IncrementCount() {
    base::AutoLock lock(lock_);
    count_++;
  }

  void SignalEvent() {
    IncrementCount();
    midi::SignalEvent();
  }

  void WaitEvent() {
    IncrementCount();
    wait_task_event_->Signal();
    midi::WaitEvent();
  }

  base::Lock lock_;
  TaskService* task_service_;
  std::unique_ptr<base::WaitableEvent> wait_task_event_;
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(TaskServiceClient);
};

class MidiTaskServiceTest : public ::testing::Test {
 public:
  MidiTaskServiceTest() = default;

 protected:
  TaskService* task_service() { return &task_service_; }
  void RunUntilIdle() { task_runner_->RunUntilIdle(); }

 private:
  void SetUp() override {
    ResetEvent();
    task_runner_ = new base::TestSimpleTaskRunner();
    thread_task_runner_handle_ =
        std::make_unique<base::ThreadTaskRunnerHandle>(task_runner_);
  }

  void TearDown() override {
    thread_task_runner_handle_.reset();
    task_runner_.reset();
  }

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<base::ThreadTaskRunnerHandle> thread_task_runner_handle_;
  TaskService task_service_;

  DISALLOW_COPY_AND_ASSIGN(MidiTaskServiceTest);
};

// Tests if posted tasks without calling BindInstance() are ignored.
TEST_F(MidiTaskServiceTest, RunUnauthorizedBoundTask) {
  std::unique_ptr<TaskServiceClient> client =
      std::make_unique<TaskServiceClient>(task_service());

  client->PostBoundTask(kFirstRunner);

  // Destruct |client| immediately, then see if the posted task is just ignored.
  // If it isn't, another thread will touch the destructed instance and will
  // cause a crash due to a use-after-free.
  client = nullptr;
}

// Tests if invalid BindInstance() calls are correctly rejected, and it does not
// make the service insanity.
TEST_F(MidiTaskServiceTest, BindTwice) {
  std::unique_ptr<TaskServiceClient> client =
      std::make_unique<TaskServiceClient>(task_service());

  EXPECT_TRUE(client->Bind());

  // Should not be able to call BindInstance() twice before unbinding current
  // bound instance.
  EXPECT_FALSE(client->Bind());

  // Should be able to unbind only the first instance.
  EXPECT_TRUE(client->Unbind());
  EXPECT_FALSE(client->Unbind());
}

// Tests if posted static tasks can be processed correctly.
TEST_F(MidiTaskServiceTest, RunStaticTask) {
  std::unique_ptr<TaskServiceClient> client =
      std::make_unique<TaskServiceClient>(task_service());

  EXPECT_TRUE(client->Bind());
  // Should be able to post a static task while an instance is bound.
  task_service()->PostStaticTask(kFirstRunner, base::BindOnce(&SignalEvent));
  WaitEvent();
  EXPECT_TRUE(client->Unbind());

  ResetEvent();

  EXPECT_TRUE(client->Bind());
  task_service()->PostStaticTask(kFirstRunner, base::BindOnce(&SignalEvent));
  // Should be able to unbind the instance to process a static task.
  EXPECT_TRUE(client->Unbind());
  WaitEvent();

  ResetEvent();

  // Should be able to post a static task without a bound instance.
  task_service()->PostStaticTask(kFirstRunner, base::BindOnce(&SignalEvent));
  WaitEvent();
}

// Tests functionalities to run bound tasks.
TEST_F(MidiTaskServiceTest, RunBoundTasks) {
  std::unique_ptr<TaskServiceClient> client =
      std::make_unique<TaskServiceClient>(task_service());

  EXPECT_TRUE(client->Bind());

  // Tests if a post task run.
  EXPECT_EQ(0u, client->count());
  client->PostBoundSignalTask(kFirstRunner);
  WaitEvent();
  EXPECT_EQ(1u, client->count());

  // Tests if another posted task is handled correctly even if the instance is
  // unbound immediately. The posted task should run safely if it starts before
  // UnboundInstance() is call. Otherwise, it should be ignored. It completely
  // depends on timing.
  client->PostBoundTask(kFirstRunner);
  EXPECT_TRUE(client->Unbind());
  client = std::make_unique<TaskServiceClient>(task_service());

  // Tests if an immediate call of another BindInstance() works correctly.
  EXPECT_TRUE(client->Bind());

  // Runs two tasks in two runners.
  ResetEvent();
  client->PostBoundSignalTask(kFirstRunner);
  client->PostBoundTask(kSecondRunner);

  // Waits only the first runner completion to see if the second runner handles
  // the task correctly even if the bound instance is destructed.
  WaitEvent();
  EXPECT_TRUE(client->Unbind());
  client = nullptr;
}

// Tests if a blocking task does not block other task runners.
TEST_F(MidiTaskServiceTest, RunBlockingTask) {
  std::unique_ptr<TaskServiceClient> client =
      std::make_unique<TaskServiceClient>(task_service());

  EXPECT_TRUE(client->Bind());

  // Posts a task that waits until the event is signaled.
  client->PostBoundWaitTask(kFirstRunner);
  // Confirms if the posted task starts. Now, the task should block in the task
  // until the second task is invoked.
  client->WaitTask();

  // Posts another task to the second runner. The task should be able to run
  // even though another posted task is blocking inside a critical section that
  // protects running tasks from an instance unbinding.
  client->PostBoundSignalTask(kSecondRunner);

  // Wait until the second task runs.
  WaitEvent();

  // UnbindInstance() should wait until any running task finishes so that the
  // instance can be destructed safely.
  EXPECT_TRUE(client->Unbind());
  EXPECT_EQ(2u, client->count());
  client = nullptr;
}

// Tests if a bound delayed task runs correctly.
TEST_F(MidiTaskServiceTest, RunBoundDelayedTask) {
  std::unique_ptr<TaskServiceClient> client =
      std::make_unique<TaskServiceClient>(task_service());

  EXPECT_TRUE(client->Bind());

  // Posts a delayed task that signals after 100msec.
  client->PostBoundDelayedSignalTask(kFirstRunner);

  // Wait until the delayed task runs.
  WaitEvent();

  EXPECT_TRUE(client->Unbind());
  EXPECT_EQ(1u, client->count());
  client = nullptr;
}

// Tests if a bound task runs on the thread that bound the instance.
TEST_F(MidiTaskServiceTest, RunBoundTaskOnDefaultRunner) {
  std::unique_ptr<TaskServiceClient> client =
      std::make_unique<TaskServiceClient>(task_service());

  EXPECT_TRUE(client->Bind());

  // Posts a task that increments the count on the caller thread.
  client->PostBoundTask(kDefaultRunner);

  // The posted task should not run until the current message loop is processed.
  EXPECT_EQ(0u, client->count());
  RunUntilIdle();
  EXPECT_EQ(1u, client->count());

  EXPECT_TRUE(client->Unbind());
}

}  // namespace

}  // namespace midi
