// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_impl.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

using testing::_;
using testing::AnyOf;
using testing::ElementsAre;
using testing::Invoke;

namespace blink {
namespace scheduler {
namespace worker_thread_unittest {

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

class MockIdleTask {
 public:
  MOCK_METHOD1(Run, void(double deadline));
};

class TestObserver : public Thread::TaskObserver {
 public:
  explicit TestObserver(StringBuilder* calls) : calls_(calls) {}

  ~TestObserver() override = default;

  void WillProcessTask(const base::PendingTask&, bool) override {
    calls_->Append(" willProcessTask");
  }

  void DidProcessTask(const base::PendingTask&) override {
    calls_->Append(" didProcessTask");
  }

 private:
  raw_ptr<StringBuilder> calls_;  // NOT OWNED
};

void RunTestTask(StringBuilder* calls) {
  calls->Append(" run");
}

void AddTaskObserver(Thread* thread, TestObserver* observer) {
  thread->AddTaskObserver(observer);
}

void RemoveTaskObserver(Thread* thread, TestObserver* observer) {
  thread->RemoveTaskObserver(observer);
}

void ShutdownOnThread(Thread* thread) {
  thread->Scheduler()->Shutdown();
}

class NonMainThreadImplTest : public testing::Test {
 public:
  NonMainThreadImplTest() = default;
  NonMainThreadImplTest(const NonMainThreadImplTest&) = delete;
  NonMainThreadImplTest& operator=(const NonMainThreadImplTest&) = delete;

  ~NonMainThreadImplTest() override = default;

  void SetUp() override {
    thread_ = NonMainThread::CreateThread(
        ThreadCreationParams(ThreadType::kTestThread));
  }

  void RunOnWorkerThread(const base::Location& from_here,
                         base::OnceClosure task) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread_->GetTaskRunner()->PostTask(
        from_here,
        base::BindOnce(&NonMainThreadImplTest::RunOnWorkerThreadTask,
                       base::Unretained(this), std::move(task), &completion));
    completion.Wait();
  }

 protected:
  void RunOnWorkerThreadTask(base::OnceClosure task,
                             base::WaitableEvent* completion) {
    std::move(task).Run();
    completion->Signal();
  }

  std::unique_ptr<NonMainThread> thread_;
};

TEST_F(NonMainThreadImplTest, TestDefaultTask) {
  MockTask task;
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  EXPECT_CALL(task, Run());
  ON_CALL(task, Run()).WillByDefault(Invoke([&completion]() {
    completion.Signal();
  }));

  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&MockTask::Run, WTF::CrossThreadUnretained(&task)));
  completion.Wait();
}

TEST_F(NonMainThreadImplTest, TestTaskObserver) {
  StringBuilder calls;
  TestObserver observer(&calls);

  RunOnWorkerThread(FROM_HERE,
                    base::BindOnce(&AddTaskObserver, thread_.get(), &observer));
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&RunTestTask, WTF::CrossThreadUnretained(&calls)));
  RunOnWorkerThread(
      FROM_HERE, base::BindOnce(&RemoveTaskObserver, thread_.get(), &observer));

  // We need to be careful what we test here.  We want to make sure the
  // observers are un in the expected order before and after the task.
  // Sometimes we get an internal scheduler task running before or after
  // TestTask as well. This is not a bug, and we need to make sure the test
  // doesn't fail when that happens.
  EXPECT_THAT(calls.ToString().Utf8(),
              testing::HasSubstr("willProcessTask run didProcessTask"));
}

TEST_F(NonMainThreadImplTest, TestShutdown) {
  MockTask task;
  MockTask delayed_task;

  EXPECT_CALL(task, Run()).Times(0);
  EXPECT_CALL(delayed_task, Run()).Times(0);

  RunOnWorkerThread(FROM_HERE,
                    base::BindOnce(&ShutdownOnThread, thread_.get()));
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&MockTask::Run, WTF::CrossThreadUnretained(&task)));
  PostDelayedCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&MockTask::Run,
                          WTF::CrossThreadUnretained(&delayed_task)),
      base::Milliseconds(50));
  thread_.reset();
}

}  // namespace worker_thread_unittest

// Needs to be in scheduler namespace for FRIEND_TEST_ALL_PREFIXES to work
#if BUILDFLAG(IS_APPLE)
TEST(NonMainThreadImplRealtimePeriodTest, RealtimePeriodConfiguration) {
  ThreadCreationParams params(ThreadType::kTestThread);
  params.realtime_period = base::Milliseconds(10);

  auto non_main_thread = std::make_unique<NonMainThreadImpl>(params);
  non_main_thread->Init();
  // No period configuration for a non-real-time thread.
  EXPECT_EQ(static_cast<base::PlatformThread::Delegate*>(
                non_main_thread->thread_.get())
                ->GetRealtimePeriod(),
            base::TimeDelta());

  params.base_thread_type = base::ThreadType::kRealtimeAudio;

  non_main_thread = std::make_unique<NonMainThreadImpl>(params);
  non_main_thread->Init();
  // Delegate correctly reports period for a real-time thread.
  EXPECT_EQ(static_cast<base::PlatformThread::Delegate*>(
                non_main_thread->thread_.get())
                ->GetRealtimePeriod(),
            params.realtime_period);
}
#endif

}  // namespace scheduler
}  // namespace blink
