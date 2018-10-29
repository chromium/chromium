// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/resource_loading_task_runner_handle_impl.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

using base::sequence_manager::TaskQueue;
using testing::UnorderedElementsAre;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace frame_scheduler_impl_unittest {

class FrameSchedulerImplTest : public testing::Test {
 public:
  FrameSchedulerImplTest()
      : task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
            base::test::ScopedTaskEnvironment::ExecutionMode::QUEUED) {
    // Null clock might trigger some assertions.
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(5));
  }

  FrameSchedulerImplTest(std::vector<base::Feature> features_to_enable,
                         std::vector<base::Feature> features_to_disable)
      : FrameSchedulerImplTest() {
    feature_list_.InitWithFeatures(features_to_enable, features_to_disable);
  }

  ~FrameSchedulerImplTest() override = default;

  void SetUp() override {
    scheduler_.reset(new MainThreadSchedulerImpl(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock()),
        base::nullopt));
    page_scheduler_.reset(new PageSchedulerImpl(nullptr, scheduler_.get()));
    frame_scheduler_ =
        FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                   FrameScheduler::FrameType::kSubframe);
  }

  void TearDown() override {
    frame_scheduler_.reset();
    page_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
  }

 protected:
  scoped_refptr<TaskQueue> throttleable_task_queue() {
    return throttleable_task_queue_;
  }

  void LazyInitThrottleableTaskQueue() {
    EXPECT_FALSE(throttleable_task_queue());
    throttleable_task_queue_ = ThrottleableTaskQueue();
    EXPECT_TRUE(throttleable_task_queue());
  }

  scoped_refptr<MainThreadTaskQueue> NonLoadingTaskQueue(
      MainThreadTaskQueue::QueueTraits queue_traits) {
    return frame_scheduler_->FrameTaskQueueControllerForTest()
        ->NonLoadingTaskQueue(queue_traits);
  }

  scoped_refptr<TaskQueue> ThrottleableTaskQueue() {
    return NonLoadingTaskQueue(
        FrameSchedulerImpl::ThrottleableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> LoadingTaskQueue() {
    return frame_scheduler_->FrameTaskQueueControllerForTest()
        ->LoadingTaskQueue();
  }

  scoped_refptr<TaskQueue> LoadingControlTaskQueue() {
    return frame_scheduler_->FrameTaskQueueControllerForTest()
        ->LoadingControlTaskQueue();
  }

  scoped_refptr<TaskQueue> DeferrableTaskQueue() {
    return NonLoadingTaskQueue(FrameSchedulerImpl::DeferrableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> PausableTaskQueue() {
    return NonLoadingTaskQueue(FrameSchedulerImpl::PausableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> UnpausableTaskQueue() {
    return NonLoadingTaskQueue(FrameSchedulerImpl::UnpausableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> ForegroundOnlyTaskQueue() {
    return NonLoadingTaskQueue(
        FrameSchedulerImpl::ForegroundOnlyTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(TaskType type) {
    return frame_scheduler_->GetTaskQueue(type);
  }

  std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl>
  GetResourceLoadingTaskRunnerHandleImpl() {
    return frame_scheduler_->CreateResourceLoadingTaskRunnerHandleImpl();
  }

  bool IsThrottled() {
    EXPECT_TRUE(throttleable_task_queue());
    return scheduler_->task_queue_throttler()->IsThrottled(
        throttleable_task_queue().get());
  }

  SchedulingLifecycleState CalculateLifecycleState(
      FrameScheduler::ObserverType type) {
    return frame_scheduler_->CalculateLifecycleState(type);
  }

  void DidChangeResourceLoadingPriority(
      scoped_refptr<MainThreadTaskQueue> task_queue,
      net::RequestPriority priority) {
    frame_scheduler_->DidChangeResourceLoadingPriority(task_queue, priority);
  }

  base::test::ScopedFeatureList& scoped_feature_list() { return feature_list_; }

  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  base::test::ScopedFeatureList feature_list_;
  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
  scoped_refptr<TaskQueue> throttleable_task_queue_;
};

class FrameSchedulerImplStopNonTimersInBackgroundEnabledTest
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplStopNonTimersInBackgroundEnabledTest()
      : FrameSchedulerImplTest({blink::features::kStopNonTimersInBackground},
                               {}) {}
};

class FrameSchedulerImplStopNonTimersInBackgroundDisabledTest
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplStopNonTimersInBackgroundDisabledTest()
      : FrameSchedulerImplTest({},
                               {blink::features::kStopNonTimersInBackground}) {}
};

namespace {

class MockLifecycleObserver final : public FrameScheduler::Observer {
 public:
  MockLifecycleObserver()
      : not_throttled_count_(0u),
        hidden_count_(0u),
        throttled_count_(0u),
        stopped_count_(0u) {}

  inline void CheckObserverState(base::Location from,
                                 size_t not_throttled_count_expectation,
                                 size_t hidden_count_expectation,
                                 size_t throttled_count_expectation,
                                 size_t stopped_count_expectation) {
    EXPECT_EQ(not_throttled_count_expectation, not_throttled_count_)
        << from.ToString();
    EXPECT_EQ(hidden_count_expectation, hidden_count_) << from.ToString();
    EXPECT_EQ(throttled_count_expectation, throttled_count_) << from.ToString();
    EXPECT_EQ(stopped_count_expectation, stopped_count_) << from.ToString();
  }

  void OnLifecycleStateChanged(SchedulingLifecycleState state) override {
    switch (state) {
      case SchedulingLifecycleState::kNotThrottled:
        not_throttled_count_++;
        break;
      case SchedulingLifecycleState::kHidden:
        hidden_count_++;
        break;
      case SchedulingLifecycleState::kThrottled:
        throttled_count_++;
        break;
      case SchedulingLifecycleState::kStopped:
        stopped_count_++;
        break;
        // We should not have another state, and compiler checks it.
    }
  }

 private:
  size_t not_throttled_count_;
  size_t hidden_count_;
  size_t throttled_count_;
  size_t stopped_count_;
};

void IncrementCounter(int* counter) {
  ++*counter;
}

void RecordQueueName(const scoped_refptr<TaskQueue> task_queue,
                     std::vector<std::string>* tasks) {
  tasks->push_back(task_queue->GetName());
}

}  // namespace

// Throttleable task queue is initialized lazily, so there're two scenarios:
// - Task queue created first and throttling decision made later;
// - Scheduler receives relevant signals to make a throttling decision but
//   applies one once task queue gets created.
// We test both (ExplicitInit/LazyInit) of them.

TEST_F(FrameSchedulerImplTest, PageVisible) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  EXPECT_FALSE(throttleable_task_queue());
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  page_scheduler_->SetPageVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHiddenThenVisible_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
  page_scheduler_->SetPageVisible(true);
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest,
       FrameHiddenThenVisible_CrossOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  frame_scheduler_->SetCrossOrigin(false);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_TRUE(IsThrottled());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest,
       FrameHidden_CrossOrigin_NoThrottling_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_NoThrottling_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  EXPECT_TRUE(throttleable_task_queue());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(true);
  frame_scheduler_->SetCrossOrigin(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PauseAndResume) {
  int counter = 0;
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  frame_scheduler_->SetPaused(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);

  frame_scheduler_->SetPaused(false);

  EXPECT_EQ(1, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, FreezeForegroundOnlyTasks) {
  int counter = 0;
  ForegroundOnlyTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, counter);

  page_scheduler_->SetPageVisible(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
}

TEST_F(FrameSchedulerImplStopNonTimersInBackgroundEnabledTest,
       PageFreezeAndUnfreezeFlagEnabled) {
  int counter = 0;
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  // unpausable tasks continue to run.
  EXPECT_EQ(1, counter);

  page_scheduler_->SetPageFrozen(false);

  EXPECT_EQ(1, counter);
  // Same as RunUntilIdle but also advances the clock if necessary.
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplStopNonTimersInBackgroundDisabledTest,
       PageFreezeAndUnfreezeFlagDisabled) {
  int counter = 0;
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  // throttleable tasks and loading tasks are frozen, others continue to run.
  EXPECT_EQ(3, counter);

  page_scheduler_->SetPageFrozen(false);

  EXPECT_EQ(3, counter);
  // Same as RunUntilIdle but also advances the clock if necessary.
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeWithKeepActive) {
  std::vector<std::string> tasks;
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, LoadingTaskQueue(), &tasks));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, ThrottleableTaskQueue(), &tasks));
  DeferrableTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, DeferrableTaskQueue(), &tasks));
  PausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, PausableTaskQueue(), &tasks));
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, UnpausableTaskQueue(), &tasks));

  page_scheduler_->SetKeepActive(true);  // say we have a Service Worker
  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // Everything runs except throttleable tasks (timers)
  EXPECT_THAT(tasks, UnorderedElementsAre(
                         std::string(LoadingTaskQueue()->GetName()),
                         std::string(DeferrableTaskQueue()->GetName()),
                         std::string(PausableTaskQueue()->GetName()),
                         std::string(UnpausableTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, LoadingTaskQueue(), &tasks));

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // loading task runs
  EXPECT_THAT(tasks,
              UnorderedElementsAre(std::string(LoadingTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName, LoadingTaskQueue(), &tasks));
  // KeepActive is false when Service Worker stops.
  page_scheduler_->SetKeepActive(false);
  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(tasks, UnorderedElementsAre());  // loading task does not run

  tasks.clear();
  page_scheduler_->SetKeepActive(true);
  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // loading task runs
  EXPECT_THAT(tasks,
              UnorderedElementsAre(std::string(LoadingTaskQueue()->GetName())));
}

TEST_F(FrameSchedulerImplStopNonTimersInBackgroundEnabledTest,
       PageFreezeAndPageVisible) {
  int counter = 0;
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);

  // Making the page visible should cause frozen queues to resume.
  page_scheduler_->SetPageVisible(true);

  EXPECT_EQ(1, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, counter);
}

// Tests if throttling observer interfaces work.
TEST_F(FrameSchedulerImplTest, LifecycleObserver) {
  std::unique_ptr<MockLifecycleObserver> observer =
      std::make_unique<MockLifecycleObserver>();

  size_t not_throttled_count = 0u;
  size_t hidden_count = 0u;
  size_t throttled_count = 0u;
  size_t stopped_count = 0u;

  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  auto observer_handle = frame_scheduler_->AddLifecycleObserver(
      FrameScheduler::ObserverType::kLoader, observer.get());

  // Initial state should be synchronously notified here.
  // We assume kNotThrottled is notified as an initial state, but it could
  // depend on implementation details and can be changed.
  observer->CheckObserverState(FROM_HERE, ++not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  // Once the page gets to be invisible, it should notify the observer of
  // kHidden synchronously.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, ++hidden_count,
                               throttled_count, stopped_count);

  // We do not issue new notifications without actually changing visibility
  // state.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(30));

  // The frame gets throttled after some time in background.
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               ++throttled_count, stopped_count);

  // We shouldn't issue new notifications for kThrottled state as well.
  page_scheduler_->SetPageVisible(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  // Setting background page to STOPPED, notifies observers of kStopped.
  page_scheduler_->SetPageFrozen(true);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, ++stopped_count);

  // When page is not in the STOPPED state, then page visibility is used,
  // notifying observer of kThrottled.
  page_scheduler_->SetPageFrozen(false);
  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               ++throttled_count, stopped_count);

  // Going back to visible state should notify the observer of kNotThrottled
  // synchronously.
  page_scheduler_->SetPageVisible(true);
  observer->CheckObserverState(FROM_HERE, ++not_throttled_count, hidden_count,
                               throttled_count, stopped_count);

  // Remove from the observer list, and see if any other callback should not be
  // invoked when the condition is changed.
  observer_handle.reset();
  page_scheduler_->SetPageVisible(false);

  // Wait 100 secs virtually and run pending tasks just in case.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(100));
  base::RunLoop().RunUntilIdle();

  observer->CheckObserverState(FROM_HERE, not_throttled_count, hidden_count,
                               throttled_count, stopped_count);
}

TEST_F(FrameSchedulerImplTest, DefaultSchedulingLifecycleState) {
  EXPECT_EQ(CalculateLifecycleState(FrameScheduler::ObserverType::kLoader),
            SchedulingLifecycleState::kNotThrottled);
  EXPECT_EQ(
      CalculateLifecycleState(FrameScheduler::ObserverType::kWorkerScheduler),
      SchedulingLifecycleState::kNotThrottled);
}

TEST_F(FrameSchedulerImplTest, SubesourceLoadingPaused) {
  // A loader observer and related counts.
  std::unique_ptr<MockLifecycleObserver> loader_observer =
      std::make_unique<MockLifecycleObserver>();

  size_t loader_throttled_count = 0u;
  size_t loader_not_throttled_count = 0u;
  size_t loader_hidden_count = 0u;
  size_t loader_stopped_count = 0u;

  // A worker observer and related counts.
  std::unique_ptr<MockLifecycleObserver> worker_observer =
      std::make_unique<MockLifecycleObserver>();

  size_t worker_throttled_count = 0u;
  size_t worker_not_throttled_count = 0u;
  size_t worker_hidden_count = 0u;
  size_t worker_stopped_count = 0u;

  // Both observers should start with no responses.
  loader_observer->CheckObserverState(
      FROM_HERE, loader_not_throttled_count, loader_hidden_count,
      loader_throttled_count, loader_stopped_count);

  worker_observer->CheckObserverState(
      FROM_HERE, worker_not_throttled_count, worker_hidden_count,
      worker_throttled_count, worker_stopped_count);

  // Adding the observers should recieve a non-throttled response
  auto loader_observer_handle = frame_scheduler_->AddLifecycleObserver(
      FrameScheduler::ObserverType::kLoader, loader_observer.get());

  auto worker_observer_handle = frame_scheduler_->AddLifecycleObserver(
      FrameScheduler::ObserverType::kWorkerScheduler, worker_observer.get());

  loader_observer->CheckObserverState(
      FROM_HERE, ++loader_not_throttled_count, loader_hidden_count,
      loader_throttled_count, loader_stopped_count);

  worker_observer->CheckObserverState(
      FROM_HERE, ++worker_not_throttled_count, worker_hidden_count,
      worker_throttled_count, worker_stopped_count);

  {
    auto pause_handle_a = frame_scheduler_->GetPauseSubresourceLoadingHandle();

    loader_observer->CheckObserverState(
        FROM_HERE, loader_not_throttled_count, loader_hidden_count,
        loader_throttled_count, ++loader_stopped_count);

    worker_observer->CheckObserverState(
        FROM_HERE, ++worker_not_throttled_count, worker_hidden_count,
        worker_throttled_count, worker_stopped_count);

    std::unique_ptr<MockLifecycleObserver> loader_observer_added_after_stopped =
        std::make_unique<MockLifecycleObserver>();

    auto loader_observer_handle = frame_scheduler_->AddLifecycleObserver(
        FrameScheduler::ObserverType::kLoader,
        loader_observer_added_after_stopped.get());
    // This observer should see stopped when added.
    loader_observer_added_after_stopped->CheckObserverState(FROM_HERE, 0, 0, 0,
                                                            1u);

    // Adding another handle should not create a new state.
    auto pause_handle_b = frame_scheduler_->GetPauseSubresourceLoadingHandle();

    loader_observer->CheckObserverState(
        FROM_HERE, loader_not_throttled_count, loader_hidden_count,
        loader_throttled_count, loader_stopped_count);

    worker_observer->CheckObserverState(
        FROM_HERE, worker_not_throttled_count, worker_hidden_count,
        worker_throttled_count, worker_stopped_count);
  }

  // Removing the handles should return the state to non throttled.
  loader_observer->CheckObserverState(
      FROM_HERE, ++loader_not_throttled_count, loader_hidden_count,
      loader_throttled_count, loader_stopped_count);

  worker_observer->CheckObserverState(
      FROM_HERE, ++worker_not_throttled_count, worker_hidden_count,
      worker_throttled_count, worker_stopped_count);
}

// TODO(farahcharab) Move priority testing to MainThreadTaskQueueTest after
// landing the change that moves priority computation to MainThreadTaskQueue.

class LowPriorityBackgroundPageExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityBackgroundPageExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForBackgroundPages}, {}) {}
};

TEST_F(LowPriorityBackgroundPageExperimentTest, FrameQueuesPriorities) {
  page_scheduler_->SetPageVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class BestEffortPriorityBackgroundPageExperimentTest
    : public FrameSchedulerImplTest {
 public:
  BestEffortPriorityBackgroundPageExperimentTest()
      : FrameSchedulerImplTest({kBestEffortPriorityForBackgroundPages}, {}) {}
};

TEST_F(BestEffortPriorityBackgroundPageExperimentTest, FrameQueuesPriorities) {
  page_scheduler_->SetPageVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPriorityHiddenFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityHiddenFrameExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForHiddenFrame},
                               {kFrameExperimentOnlyWhenLoading}) {}
};

TEST_F(LowPriorityHiddenFrameExperimentTest, FrameQueuesPriorities) {
  // Hidden Frame Task Queues.
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Visible Frame Task Queues.
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPriorityHiddenFrameDuringLoadingExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPriorityHiddenFrameDuringLoadingExperimentTest()
      : FrameSchedulerImplTest(
            {kLowPriorityForHiddenFrame, kFrameExperimentOnlyWhenLoading},
            {}) {}
};

TEST_F(LowPriorityHiddenFrameDuringLoadingExperimentTest,
       FrameQueuesPriorities) {
  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  // Hidden Frame Task Queues.
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPrioritySubFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPrioritySubFrameExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForSubFrame},
                               {kFrameExperimentOnlyWhenLoading}) {}
};

TEST_F(LowPrioritySubFrameExperimentTest, FrameQueuesPriorities) {
  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  frame_scheduler_ =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPrioritySubFrameDuringLoadingExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPrioritySubFrameDuringLoadingExperimentTest()
      : FrameSchedulerImplTest(
            {kLowPriorityForSubFrame, kFrameExperimentOnlyWhenLoading},
            {}) {}
};

TEST_F(LowPrioritySubFrameDuringLoadingExperimentTest, FrameQueuesPriorities) {
  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(page_scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPrioritySubFrameThrottleableTaskExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPrioritySubFrameThrottleableTaskExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForSubFrameThrottleableTask},
                               {kFrameExperimentOnlyWhenLoading}) {}
};

TEST_F(LowPrioritySubFrameThrottleableTaskExperimentTest,
       FrameQueuesPriorities) {
  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_ =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPrioritySubFrameThrottleableTaskDuringLoadingExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPrioritySubFrameThrottleableTaskDuringLoadingExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForSubFrameThrottleableTask,
                                kFrameExperimentOnlyWhenLoading},
                               {}) {}
};

TEST_F(LowPrioritySubFrameThrottleableTaskDuringLoadingExperimentTest,
       FrameQueuesPriorities) {
  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(page_scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPriorityThrottleableTaskExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPriorityThrottleableTaskExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForThrottleableTask},
                               {kFrameExperimentOnlyWhenLoading}) {}
};

TEST_F(LowPriorityThrottleableTaskExperimentTest, FrameQueuesPriorities) {
  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_ =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPriorityThrottleableTaskDuringLoadingExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPriorityThrottleableTaskDuringLoadingExperimentTest()
      : FrameSchedulerImplTest(
            {kLowPriorityForThrottleableTask, kFrameExperimentOnlyWhenLoading},
            {}) {}
};

TEST_F(LowPriorityThrottleableTaskDuringLoadingExperimentTest,
       SubFrameQueuesPriorities) {
  // Main thread is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(LowPriorityThrottleableTaskDuringLoadingExperimentTest,
       MainFrameQueuesPriorities) {
  frame_scheduler_ =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kMainFrame);

  // Main thread is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(page_scheduler_->IsLoading());

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class LowPriorityAdFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityAdFrameExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForAdFrame},
                               {kAdFrameExperimentOnlyWhenLoading}) {}
};

TEST_F(LowPriorityAdFrameExperimentTest, FrameQueuesPriorities) {
  EXPECT_FALSE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetIsAdFrame();

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
}

class LowPriorityAdFrameDuringLoadingExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPriorityAdFrameDuringLoadingExperimentTest()
      : FrameSchedulerImplTest(
            {kLowPriorityForAdFrame, kAdFrameExperimentOnlyWhenLoading},
            {}) {}
};

TEST_F(LowPriorityAdFrameDuringLoadingExperimentTest, FrameQueuesPriorities) {
  frame_scheduler_->SetIsAdFrame();

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();

  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class BestEffortPriorityAdFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  BestEffortPriorityAdFrameExperimentTest()
      : FrameSchedulerImplTest({kBestEffortPriorityForAdFrame},
                               {kAdFrameExperimentOnlyWhenLoading}) {}
};

TEST_F(BestEffortPriorityAdFrameExperimentTest, FrameQueuesPriorities) {
  EXPECT_FALSE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetIsAdFrame();

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
}

class BestEffortPriorityAdFrameDuringLoadingExperimentTest
    : public FrameSchedulerImplTest {
 public:
  BestEffortPriorityAdFrameDuringLoadingExperimentTest()
      : FrameSchedulerImplTest(
            {kBestEffortPriorityForAdFrame, kAdFrameExperimentOnlyWhenLoading},
            {}) {}
};

TEST_F(BestEffortPriorityAdFrameDuringLoadingExperimentTest,
       FrameQueuesPriorities) {
  frame_scheduler_->SetIsAdFrame();

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);

  // Main thread scheduler is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();

  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class ResourceFetchPriorityExperimentTest : public FrameSchedulerImplTest {
 public:
  ResourceFetchPriorityExperimentTest()
      : FrameSchedulerImplTest({kUseResourceFetchPriority}, {}) {
    std::map<std::string, std::string> params{
        {"HIGHEST", "HIGH"}, {"MEDIUM", "NORMAL"}, {"LOW", "NORMAL"},
        {"LOWEST", "LOW"},   {"IDLE", "LOW"},      {"THROTTLED", "LOW"}};

    const char kStudyName[] = "ResourceFetchPriorityExperiment";
    const char kGroupName[] = "GroupName1";

    field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
    base::AssociateFieldTrialParams(kStudyName, kGroupName, params);
    base::FieldTrialList::CreateFieldTrial(kStudyName, kGroupName);
  }
};

TEST_F(ResourceFetchPriorityExperimentTest, DidChangePriority) {
  std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl> handle =
      GetResourceLoadingTaskRunnerHandleImpl();
  scoped_refptr<MainThreadTaskQueue> task_queue = handle->task_queue();

  TaskQueue::QueuePriority priority = task_queue->GetQueuePriority();
  EXPECT_EQ(priority, TaskQueue::QueuePriority::kNormalPriority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOWEST);
  EXPECT_EQ(task_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::HIGHEST);
  EXPECT_EQ(task_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
}

class ResourceFetchPriorityExperimentOnlyWhenLoadingTest
    : public FrameSchedulerImplTest {
 public:
  ResourceFetchPriorityExperimentOnlyWhenLoadingTest()
      : FrameSchedulerImplTest({kUseResourceFetchPriorityOnlyWhenLoading}, {}) {
    std::map<std::string, std::string> params{
        {"HIGHEST", "HIGH"}, {"MEDIUM", "NORMAL"}, {"LOW", "NORMAL"},
        {"LOWEST", "LOW"},   {"IDLE", "LOW"},      {"THROTTLED", "LOW"}};

    const char kStudyName[] = "ResourceFetchPriorityExperiment";
    const char kGroupName[] = "GroupName2";

    field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);
    base::AssociateFieldTrialParams(kStudyName, kGroupName, params);
    base::FieldTrialList::CreateFieldTrial(kStudyName, kGroupName);
  }
};

TEST_F(ResourceFetchPriorityExperimentOnlyWhenLoadingTest, DidChangePriority) {
  std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl> handle =
      GetResourceLoadingTaskRunnerHandleImpl();
  scoped_refptr<MainThreadTaskQueue> task_queue = handle->task_queue();

  TaskQueue::QueuePriority priority = task_queue->GetQueuePriority();
  EXPECT_EQ(priority, TaskQueue::QueuePriority::kNormalPriority);

  // Experiment is only enabled during the loading phase.
  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOWEST);
  EXPECT_EQ(task_queue->GetQueuePriority(), priority);

  // Main thread scheduler is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  handle = GetResourceLoadingTaskRunnerHandleImpl();
  task_queue = handle->task_queue();

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOWEST);
  EXPECT_EQ(task_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::HIGHEST);
  EXPECT_EQ(task_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
}

TEST_F(
    FrameSchedulerImplTest,
    DidChangeResourceLoadingPriority_ResourceFecthPriorityExperimentDisabled) {
  // If the experiment is disabled, we use |loading_task_queue_| for resource
  // loading tasks and we don't want the priority of this queue to be affected
  // by individual resources.
  std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl> handle =
      GetResourceLoadingTaskRunnerHandleImpl();
  scoped_refptr<MainThreadTaskQueue> task_queue = handle->task_queue();

  TaskQueue::QueuePriority priority = task_queue->GetQueuePriority();

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOW);
  EXPECT_EQ(task_queue->GetQueuePriority(), priority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::HIGHEST);
  EXPECT_EQ(task_queue->GetQueuePriority(), priority);
}

class LowPriorityCrossOriginTaskExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityCrossOriginTaskExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForCrossOrigin}, {}) {}
};

TEST_F(LowPriorityCrossOriginTaskExperimentTest, FrameQueuesPriorities) {
  EXPECT_FALSE(frame_scheduler_->IsCrossOrigin());

  // Same Origin Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_TRUE(frame_scheduler_->IsCrossOrigin());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
}

class LowPriorityCrossOriginTaskDuringLoadingExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPriorityCrossOriginTaskDuringLoadingExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForCrossOriginOnlyWhenLoading},
                               {}) {}
};

TEST_F(LowPriorityCrossOriginTaskDuringLoadingExperimentTest,
       FrameQueuesPriorities) {
  // Main thread is in the loading use case.
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_TRUE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetCrossOrigin(true);
  EXPECT_TRUE(frame_scheduler_->IsCrossOrigin());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread is no longer in loading use case.
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest, TaskTypeToTaskQueueMapping) {
  // Make sure the queue lookup and task type to queue traits map works as
  // expected. This test will fail if these task types are moved to different
  // default queues.
  EXPECT_EQ(GetTaskQueue(TaskType::kJavascriptTimer), ThrottleableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kDatabaseAccess), DeferrableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kPostedMessage), PausableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kInternalIPC), UnpausableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kNetworking), LoadingTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kNetworkingControl),
            LoadingControlTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kInternalTranslation),
            ForegroundOnlyTaskQueue());
}

class ThrottleAndFreezeTaskTypesExperimentTest : public FrameSchedulerImplTest {
 public:
  ThrottleAndFreezeTaskTypesExperimentTest(
      std::map<std::string, std::string> params,
      const char* group_name) {
    const char kStudyName[] = "ThrottleAndFreezeTaskTypes";

    field_trial_list_ = std::make_unique<base::FieldTrialList>(nullptr);

    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial(kStudyName, group_name);

    base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
        kStudyName, group_name, params);

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        kThrottleAndFreezeTaskTypes.name,
        base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    scoped_feature_list().InitWithFeatureList(std::move(feature_list));
  }
};

class ThrottleableAndFreezableTaskTypesTest
    : public ThrottleAndFreezeTaskTypesExperimentTest {
 public:
  ThrottleableAndFreezableTaskTypesTest()
      : ThrottleAndFreezeTaskTypesExperimentTest(
            std::map<std::string, std::string>{
                // Leading spaces are allowed.
                {kThrottleableTaskTypesListParam,
                 "PostedMessage, DatabaseAccess"},
                {kFreezableTaskTypesListParam,
                 "PostedMessage, MediaElementEvent,DOMManipulation"}},
            "Group1") {}
};

TEST_F(ThrottleableAndFreezableTaskTypesTest, QueueTraitsFromFieldTrialParams) {
  if (base::FeatureList::IsEnabled(blink::features::kStopNonTimersInBackground))
    return;
  // These tests will start to fail if the default task queues or queue traits
  // change for these task types.

  // Check that the overrides work.
  auto task_queue = GetTaskQueue(TaskType::kPostedMessage);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeThrottled(true)
                                              .SetCanBeFrozen(true)
                                              .SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kMediaElementEvent);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeFrozen(true).SetCanBePaused(
          true));
  task_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeThrottled(true)
                                              .SetCanBeDeferred(true)
                                              .SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kDOMManipulation);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeFrozen(true)
                                              .SetCanBeDeferred(true)
                                              .SetCanBePaused(true));

  // Test some task types that were not configured through field trial
  // parameters.
  task_queue = GetTaskQueue(TaskType::kInternalIPC);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits());

  task_queue = GetTaskQueue(TaskType::kInternalIndexedDB);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits().SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kMiscPlatformAPI);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeDeferred(true).SetCanBePaused(
          true));
}

class FreezableOnlyTaskTypesTest
    : public ThrottleAndFreezeTaskTypesExperimentTest {
 public:
  FreezableOnlyTaskTypesTest()
      : ThrottleAndFreezeTaskTypesExperimentTest(
            std::map<std::string, std::string>{
                {kThrottleableTaskTypesListParam, ""},
                {kFreezableTaskTypesListParam,
                 "PostedMessage,MediaElementEvent,DOMManipulation"}},
            "Group2") {}
};

TEST_F(FreezableOnlyTaskTypesTest, QueueTraitsFromFieldTrialParams) {
  if (base::FeatureList::IsEnabled(blink::features::kStopNonTimersInBackground))
    return;

  // These tests will start to fail if the default task queues or queue traits
  // change for these task types.

  // Check that the overrides work.
  auto task_queue = GetTaskQueue(TaskType::kPostedMessage);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeFrozen(true).SetCanBePaused(
          true));

  task_queue = GetTaskQueue(TaskType::kMediaElementEvent);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeFrozen(true).SetCanBePaused(
          true));

  task_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeDeferred(true).SetCanBePaused(
          true));

  task_queue = GetTaskQueue(TaskType::kDOMManipulation);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeFrozen(true)
                                              .SetCanBeDeferred(true)
                                              .SetCanBePaused(true));

  // Test some task types that were not configured through field trial
  // parameters.
  task_queue = GetTaskQueue(TaskType::kInternalIPC);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits());

  task_queue = GetTaskQueue(TaskType::kInternalIndexedDB);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits().SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kMiscPlatformAPI);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeDeferred(true).SetCanBePaused(
          true));
}

class ThrottleableOnlyTaskTypesTest
    : public ThrottleAndFreezeTaskTypesExperimentTest {
 public:
  ThrottleableOnlyTaskTypesTest()
      : ThrottleAndFreezeTaskTypesExperimentTest(
            std::map<std::string, std::string>{
                {kFreezableTaskTypesListParam, ""},
                {kThrottleableTaskTypesListParam,
                 "PostedMessage,DatabaseAccess"}},
            "Group3") {}
};

TEST_F(ThrottleableOnlyTaskTypesTest, QueueTraitsFromFieldTrialParams) {
  if (base::FeatureList::IsEnabled(blink::features::kStopNonTimersInBackground))
    return;

  // These tests will start to fail if the default task queues or queue traits
  // change for these task types.

  // Check that the overrides work.
  auto task_queue = GetTaskQueue(TaskType::kPostedMessage);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeThrottled(true).SetCanBePaused(
          true));

  task_queue = GetTaskQueue(TaskType::kMediaElementEvent);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits().SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeThrottled(true)
                                              .SetCanBeDeferred(true)
                                              .SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kDOMManipulation);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeDeferred(true).SetCanBePaused(
          true));

  // Test some task types that were not configured through field trial
  // parameters.
  task_queue = GetTaskQueue(TaskType::kInternalIPC);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits());

  task_queue = GetTaskQueue(TaskType::kInternalIndexedDB);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits().SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kMiscPlatformAPI);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanBeDeferred(true).SetCanBePaused(
          true));
}

}  // namespace frame_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
