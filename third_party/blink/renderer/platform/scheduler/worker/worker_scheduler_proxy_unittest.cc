// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/worker/worker_scheduler_proxy.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"

namespace blink {
namespace scheduler {

namespace {

class WorkerThreadSchedulerForTest : public WorkerThreadScheduler {
 public:
  WorkerThreadSchedulerForTest(base::sequence_manager::SequenceManager* manager,
                               WorkerSchedulerProxy* proxy,
                               base::WaitableEvent* throtting_state_changed)
      : WorkerThreadScheduler(ThreadType::kTestThread, manager, proxy),
        throtting_state_changed_(throtting_state_changed) {}

  void OnLifecycleStateChanged(
      SchedulingLifecycleState lifecycle_state) override {
    WorkerThreadScheduler::OnLifecycleStateChanged(lifecycle_state);

    throtting_state_changed_->Signal();
  }

  using WorkerThreadScheduler::lifecycle_state;

 private:
  base::WaitableEvent* throtting_state_changed_;
};

class WorkerThreadForTest : public WorkerThread {
 public:
  WorkerThreadForTest(FrameScheduler* frame_scheduler,
                      base::WaitableEvent* throtting_state_changed)
      : WorkerThread(ThreadCreationParams(ThreadType::kTestThread)
                         .SetFrameOrWorkerScheduler(frame_scheduler)),
        throtting_state_changed_(throtting_state_changed) {}

  ~WorkerThreadForTest() override {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&WorkerThreadForTest::DisposeWorkerSchedulerOnThread,
                       base::Unretained(this), &completion));
    completion.Wait();
  }

  void DisposeWorkerSchedulerOnThread(base::WaitableEvent* completion) {
    if (worker_scheduler_) {
      worker_scheduler_->Dispose();
      worker_scheduler_ = nullptr;
    }
    completion->Signal();
  }

  std::unique_ptr<NonMainThreadSchedulerImpl> CreateNonMainThreadScheduler(
      base::sequence_manager::SequenceManager* manager) override {
    auto scheduler = std::make_unique<WorkerThreadSchedulerForTest>(
        manager, worker_scheduler_proxy(), throtting_state_changed_);
    scheduler_ = scheduler.get();
    worker_scheduler_ = std::make_unique<scheduler::WorkerScheduler>(
        scheduler_, worker_scheduler_proxy());
    return scheduler;
  }

  WorkerThreadSchedulerForTest* GetWorkerScheduler() { return scheduler_; }

 private:
  base::WaitableEvent* throtting_state_changed_;       // NOT OWNED
  WorkerThreadSchedulerForTest* scheduler_ = nullptr;  // NOT OWNED
  std::unique_ptr<WorkerScheduler> worker_scheduler_ = nullptr;
};

std::unique_ptr<WorkerThreadForTest> CreateWorkerThread(
    FrameScheduler* frame_scheduler,
    base::WaitableEvent* throtting_state_changed) {
  auto thread = std::make_unique<WorkerThreadForTest>(frame_scheduler,
                                                      throtting_state_changed);
  thread->Init();
  return thread;
}

}  // namespace

class WorkerSchedulerProxyTest : public testing::Test {
 public:
  WorkerSchedulerProxyTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        main_thread_scheduler_(std::make_unique<MainThreadSchedulerImpl>(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                task_environment_.GetMainThreadTaskRunner(),
                task_environment_.GetMockTickClock()),
            base::nullopt)),
        page_scheduler_(
            std::make_unique<PageSchedulerImpl>(nullptr,
                                                main_thread_scheduler_.get())),
        frame_scheduler_(FrameSchedulerImpl::Create(
            page_scheduler_.get(),
            nullptr,
            nullptr,
            FrameScheduler::FrameType::kMainFrame)) {}

  ~WorkerSchedulerProxyTest() override {
    frame_scheduler_.reset();
    page_scheduler_.reset();
    main_thread_scheduler_->Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImpl> main_thread_scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
};

TEST_F(WorkerSchedulerProxyTest, VisibilitySignalReceived) {
  base::WaitableEvent throtting_state_changed(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  auto worker_thread =
      CreateWorkerThread(frame_scheduler_.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kNotThrottled);

  page_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kHidden);

  // Trigger full throttling.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(30));
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kThrottled);

  page_scheduler_->SetPageVisible(true);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kNotThrottled);

  base::RunLoop().RunUntilIdle();
}

// Tests below check that no crashes occur during different shutdown sequences.

TEST_F(WorkerSchedulerProxyTest, FrameSchedulerDestroyed) {
  base::WaitableEvent throtting_state_changed(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  auto worker_thread =
      CreateWorkerThread(frame_scheduler_.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kNotThrottled);

  page_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kHidden);

  frame_scheduler_.reset();
  base::RunLoop().RunUntilIdle();

  worker_thread.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(WorkerSchedulerProxyTest, ThreadDestroyed) {
  base::WaitableEvent throtting_state_changed(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  auto worker_thread =
      CreateWorkerThread(frame_scheduler_.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kNotThrottled);

  page_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->lifecycle_state() ==
         SchedulingLifecycleState::kHidden);

  worker_thread.reset();
  base::RunLoop().RunUntilIdle();

  page_scheduler_->SetPageVisible(true);
  base::RunLoop().RunUntilIdle();

  frame_scheduler_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace scheduler
}  // namespace blink
