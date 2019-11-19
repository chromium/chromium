// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/worker_thread_registry.h"

#include "base/logging.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/public/renderer/worker_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WorkerThreadRegistryTest : public testing::Test {
 public:
  void FakeStart() { task_runner_.DidStartCurrentWorkerThread(); }
  void FakeStop() { task_runner_.WillStopCurrentWorkerThread(); }
  WorkerThreadRegistry task_runner_;

 private:
  base::test::TaskEnvironment task_environment_;
};

class MockObserver : public WorkerThread::Observer {
 public:
  MOCK_METHOD0(WillStopCurrentWorkerThread, void());
  void RemoveSelfOnNotify() {
    ON_CALL(*this, WillStopCurrentWorkerThread())
        .WillByDefault(testing::Invoke(this, &MockObserver::RemoveSelf));
  }
  void RemoveSelf() { WorkerThread::RemoveObserver(this); }
  WorkerThreadRegistry* runner_;
};

TEST_F(WorkerThreadRegistryTest, BasicObservingAndWorkerId) {
  ASSERT_EQ(0, WorkerThread::GetCurrentId());
  MockObserver o;
  EXPECT_CALL(o, WillStopCurrentWorkerThread()).Times(1);
  FakeStart();
  WorkerThread::AddObserver(&o);
  ASSERT_LT(0, WorkerThread::GetCurrentId());
  FakeStop();
}

TEST_F(WorkerThreadRegistryTest, CanRemoveSelfDuringNotification) {
  MockObserver o;
  o.RemoveSelfOnNotify();
  o.runner_ = &task_runner_;
  EXPECT_CALL(o, WillStopCurrentWorkerThread()).Times(1);
  FakeStart();
  WorkerThread::AddObserver(&o);
  FakeStop();
}

TEST_F(WorkerThreadRegistryTest, TaskRunnerRemovedCorrectly) {
  ASSERT_EQ(0, WorkerThread::GetCurrentId());
  MockObserver o;
  FakeStart();
  WorkerThread::AddObserver(&o);
  EXPECT_CALL(o, WillStopCurrentWorkerThread()).Times(1);

  // Get the thread id and the task runner for the live thread.
  int thread_id = WorkerThread::GetCurrentId();
  base::TaskRunner* task_runner = task_runner_.GetTaskRunnerFor(thread_id);
  ASSERT_LT(0, thread_id);
  ASSERT_NE(nullptr, task_runner);

  // Check that the task runner is no longer used after the thread is
  // terminated.
  FakeStop();
  ASSERT_NE(task_runner, task_runner_.GetTaskRunnerFor(thread_id));
}

}  // namespace content
