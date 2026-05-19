// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/non_main_thread_impl.h"

#include <atomic>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
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

#if DCHECK_IS_ON()
  void TearDown() override {
    thread_.reset();
    SetIsBeforeThreadCreatedForTest();
  }
#endif  // DCHECK_IS_ON()

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
  ON_CALL(task, Run()).WillByDefault([&completion]() { completion.Signal(); });

  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&MockTask::Run, CrossThreadUnretained(&task)));
  completion.Wait();
}

TEST_F(NonMainThreadImplTest, TestTaskObserver) {
  StringBuilder calls;
  TestObserver observer(&calls);

  RunOnWorkerThread(FROM_HERE,
                    base::BindOnce(&AddTaskObserver, thread_.get(), &observer));
  PostCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&RunTestTask, CrossThreadUnretained(&calls)));
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
      CrossThreadBindOnce(&MockTask::Run, CrossThreadUnretained(&task)));
  PostDelayedCrossThreadTask(
      *thread_->GetTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(&MockTask::Run, CrossThreadUnretained(&delayed_task)),
      base::Milliseconds(50));
  thread_.reset();
}

// Regression test for crbug.com/513544566.
// SequenceManagerImpl::any_thread_clock() requires that the installed
// TimeDomain outlives SequenceManagerImpl (even if it is reset).
//
// Previously, ThreadSchedulerBase::Shutdown() destroyed the
// AutoAdvancingVirtualTimeDomain before the default task queue's
// GuardedTaskPoster was fully shut down. A concurrent cross-thread
// PostDelayedTask() from another thread could load a stale clock pointer
// and dereference it, causing a Use-After-Free (UAF) under ASAN inside
// LazyNow::Now().
//
// This test ensures that we safely sequence the shutdown of the virtual
// time domain. It models a DevTools client enabling virtual time on a
// worker target followed by worker termination, while another renderer
// thread is concurrently posting delayed tasks to the worker's task runner.
TEST(NonMainThreadImplVirtualTimeShutdownRace,
     CrossThreadPostDelayedTaskDuringShutdown) {
  // Oversubscribe the CPU so hammer threads are preempted between the
  // any_thread_clock() load and the LazyNow::Now() dereference.
  const int kHammerThreads = 16;

  std::unique_ptr<NonMainThread> thread = NonMainThread::CreateThread(
      ThreadCreationParams(ThreadType::kTestThread));
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      thread->GetTaskRunner();

  // Enable virtual time on the worker thread, mimicking what
  // InspectorEmulationAgent::setVirtualTimePolicy does on a worker target.
  // The policy is paused so that the worker doesn't fast-forward through
  // the incoming delayed tasks.
  {
    base::WaitableEvent done;
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(
                       [](Thread* thread, base::WaitableEvent* done) {
                         auto* sched = static_cast<WorkerThreadScheduler*>(
                             thread->Scheduler());
                         sched->EnableVirtualTime(base::Time());
                         sched->SetVirtualTimePolicy(
                             VirtualTimeController::VirtualTimePolicy::kPause);
                         done->Signal();
                       },
                       thread.get(), &done));
    done.Wait();
  }

  // Spawns threads to hammer the worker's default task queue with cross-thread
  // PostDelayedTask() calls. This exercises the code path that accesses the
  // time domain's clock concurrently.
  std::atomic<bool> stop{false};
  base::WaitableEvent go_event(base::WaitableEvent::ResetPolicy::MANUAL,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::vector<std::unique_ptr<base::Thread>> hammers;
  hammers.reserve(kHammerThreads);
  for (int i = 0; i < kHammerThreads; ++i) {
    auto hammer = std::make_unique<base::Thread>("HammerThread");
    hammer->Start();

    hammer->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WaitableEvent* go_event, std::atomic<bool>* stop,
               scoped_refptr<base::SingleThreadTaskRunner> target_runner) {
              go_event->Wait();
              while (!stop->load(std::memory_order_relaxed)) {
                target_runner->PostDelayedTask(FROM_HERE, base::DoNothing(),
                                               base::Hours(1));
              }
            },
            &go_event, &stop, task_runner));

    hammers.push_back(std::move(hammer));
  }
  go_event.Signal();
  // Let the hammers warm up so they're mid-PostDelayedTask when the worker
  // tears the time domain down.
  base::PlatformThread::Sleep(base::Microseconds(200));

  // Shut down the scheduler on the worker thread, mimicking what
  // WorkerThread::PerformShutdownOnWorkerThread does. This triggers
  // ThreadSchedulerBase::Shutdown(), which safely detaches the virtual
  // time domain observer while the helper is still alive.
  {
    base::WaitableEvent done;
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(
                              [](Thread* thread, base::WaitableEvent* done) {
                                thread->Scheduler()->Shutdown();
                                done->Signal();
                              },
                              thread.get(), &done));
    done.Wait();
  }

  stop.store(true, std::memory_order_relaxed);
  // base::Thread destructor automatically stops and Joins the thread,
  // so simply clearing the vector joins all hammer threads safely.
  hammers.clear();
  thread.reset();
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
