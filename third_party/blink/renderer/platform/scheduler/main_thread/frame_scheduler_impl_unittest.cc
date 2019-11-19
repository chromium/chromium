// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/resource_loading_task_runner_handle_impl.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using base::sequence_manager::TaskQueue;
using testing::UnorderedElementsAre;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace frame_scheduler_impl_unittest {

using FeatureHandle = FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle;

class FrameSchedulerDelegateForTesting : public FrameScheduler::Delegate {
 public:
  FrameSchedulerDelegateForTesting() {}

  ~FrameSchedulerDelegateForTesting() override {}

  ukm::UkmRecorder* GetUkmRecorder() override { return nullptr; }

  ukm::SourceId GetUkmSourceId() override { return ukm::kInvalidSourceId; }

  void UpdateTaskTime(base::TimeDelta task_time) override {
    update_task_time_calls_++;
  }

  const base::UnguessableToken& GetAgentClusterId() const override {
    return base::UnguessableToken::Null();
  }

  MOCK_METHOD1(UpdateActiveSchedulerTrackedFeatures, void(uint64_t));

  int update_task_time_calls_ = 0;
};

class FrameSchedulerImplTest : public testing::Test {
 public:
  FrameSchedulerImplTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

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
    frame_scheduler_delegate_ = std::make_unique<
        testing::StrictMock<FrameSchedulerDelegateForTesting>>();
    frame_scheduler_ = FrameSchedulerImpl::Create(
        page_scheduler_.get(), frame_scheduler_delegate_.get(), nullptr,
        FrameScheduler::FrameType::kSubframe);
    EnsureUseCaseNone();
  }

  void ResetFrameScheduler(FrameScheduler::FrameType frame_type) {
    frame_scheduler_delegate_ = std::make_unique<
        testing::StrictMock<FrameSchedulerDelegateForTesting>>();
    frame_scheduler_ = FrameSchedulerImpl::Create(
        page_scheduler_.get(), frame_scheduler_delegate_.get(), nullptr,
        frame_type);
  }

  void TearDown() override {
    throttleable_task_queue_.reset();
    frame_scheduler_.reset();
    page_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
    frame_scheduler_delegate_.reset();
  }

  void EnsureUseCaseNone() {
    // Make sure we're not in UseCase::kLoading.
    scheduler_->OnFirstContentfulPaint();
    scheduler_->OnFirstMeaningfulPaint();

    ASSERT_EQ(scheduler_->main_thread_only().current_use_case, UseCase::kNone);
  }

  void EnsureUseCaseLoading() {
    scheduler_->DidStartProvisionalLoad(true);
    scheduler_->OnFirstContentfulPaint();
    ASSERT_EQ(scheduler_->main_thread_only().current_use_case,
              UseCase::kLoading);
  }

  static void ResetForNavigation(FrameSchedulerImpl* frame_scheduler) {
    frame_scheduler->ResetForNavigation();
  }

  base::TimeDelta GetTaskTime() { return frame_scheduler_->task_time_; }

  int GetTotalUpdateTaskTimeCalls() {
    return frame_scheduler_delegate_->update_task_time_calls_;
  }

  void ResetTotalUpdateTaskTimeCalls() {
    frame_scheduler_delegate_->update_task_time_calls_ = 0;
  }

  static uint64_t GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
      FrameSchedulerImpl* frame_scheduler) {
    return frame_scheduler
        ->GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();
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

  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(
      MainThreadTaskQueue::QueueTraits queue_traits) {
    return frame_scheduler_->FrameTaskQueueControllerForTest()
        ->GetTaskQueue(queue_traits);
  }

  scoped_refptr<TaskQueue> ThrottleableTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::ThrottleableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> LoadingTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::LoadingTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> LoadingControlTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::LoadingControlTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> DeferrableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::DeferrableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> PausableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::PausableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> UnpausableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::UnpausableTaskQueueTraits());
  }

  scoped_refptr<TaskQueue> ForegroundOnlyTaskQueue() {
    return GetTaskQueue(
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

  void DidCommitProvisionalLoad(
      FrameScheduler::NavigationType navigation_type) {
    frame_scheduler_->DidCommitProvisionalLoad(
        /*is_web_history_inert_commit=*/false, navigation_type);
  }

  base::test::ScopedFeatureList& scoped_feature_list() { return feature_list_; }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MainThreadSchedulerImpl> scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
  std::unique_ptr<testing::StrictMock<FrameSchedulerDelegateForTesting>>
      frame_scheduler_delegate_;
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

void ExpectAndIncrementCounter(int expected, int* actual) {
  EXPECT_EQ(expected, *actual);
  IncrementCounter(actual);
}

void RecordQueueName(String name, Vector<String>* tasks) {
  tasks->push_back(std::move(name));
}

void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(std::move(value));
}

// Simulate running a task of a particular length by fast forwarding the task
// environment clock, which is used to determine the wall time of a task.
void RunTaskOfLength(base::test::TaskEnvironment* task_environment,
                     base::TimeDelta length) {
  task_environment->FastForwardBy(length);
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

TEST_F(FrameSchedulerImplTest, PauseAndResumeForCooperativeScheduling) {
  EXPECT_TRUE(LoadingTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(ThrottleableTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(DeferrableTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(PausableTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(UnpausableTaskQueue()->IsQueueEnabled());

  frame_scheduler_->SetPreemptedForCooperativeScheduling(
      FrameOrWorkerScheduler::Preempted(true));
  EXPECT_FALSE(LoadingTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(ThrottleableTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(DeferrableTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(PausableTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(UnpausableTaskQueue()->IsQueueEnabled());

  frame_scheduler_->SetPreemptedForCooperativeScheduling(
      FrameOrWorkerScheduler::Preempted(false));
  EXPECT_TRUE(LoadingTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(ThrottleableTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(DeferrableTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(PausableTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(UnpausableTaskQueue()->IsQueueEnabled());
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

TEST_F(FrameSchedulerImplTest, PagePostsCpuTasks) {
  EXPECT_TRUE(GetTaskTime().is_zero());
  EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                base::TimeDelta::FromMilliseconds(10)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetTaskTime().is_zero());
  EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                base::TimeDelta::FromMilliseconds(100)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetTaskTime().is_zero());
  EXPECT_EQ(1, GetTotalUpdateTaskTimeCalls());
}

TEST_F(FrameSchedulerImplTest, FramePostsCpuTasksThroughReloadRenavigate) {
  const struct {
    FrameScheduler::FrameType frame_type;
    FrameScheduler::NavigationType navigation_type;
    bool expect_task_time_zero;
    int expected_total_calls;
  } kTestCases[] = {{FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kOther, false, 0},
                    {FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kReload, false, 0},
                    {FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kSameDocument, true, 1},
                    {FrameScheduler::FrameType::kSubframe,
                     FrameScheduler::NavigationType::kOther, true, 1},
                    {FrameScheduler::FrameType::kSubframe,
                     FrameScheduler::NavigationType::kSameDocument, true, 1}};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(String::Format(
        "FrameType: %d, NavigationType: %d : TaskTime.is_zero %d, CallCount %d",
        test_case.frame_type, test_case.navigation_type,
        test_case.expect_task_time_zero, test_case.expected_total_calls));
    ResetFrameScheduler(test_case.frame_type);
    EXPECT_TRUE(GetTaskTime().is_zero());
    EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());

    // Check the rest of the values after different types of commit.
    UnpausableTaskQueue()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                  base::TimeDelta::FromMilliseconds(60)));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(GetTaskTime().is_zero());
    EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());

    DidCommitProvisionalLoad(test_case.navigation_type);

    UnpausableTaskQueue()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                  base::TimeDelta::FromMilliseconds(60)));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(test_case.expect_task_time_zero, GetTaskTime().is_zero());
    EXPECT_EQ(test_case.expected_total_calls, GetTotalUpdateTaskTimeCalls());
  }
}

TEST_F(FrameSchedulerImplTest, PageFreezeWithKeepActive) {
  Vector<String> tasks;
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, LoadingTaskQueue()->GetName(), &tasks));
  ThrottleableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName,
                                ThrottleableTaskQueue()->GetName(), &tasks));
  DeferrableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName,
                                DeferrableTaskQueue()->GetName(), &tasks));
  PausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, PausableTaskQueue()->GetName(), &tasks));
  UnpausableTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RecordQueueName,
                                UnpausableTaskQueue()->GetName(), &tasks));

  page_scheduler_->SetKeepActive(true);  // say we have a Service Worker
  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // Everything runs except throttleable tasks (timers)
  EXPECT_THAT(tasks,
              UnorderedElementsAre(String(LoadingTaskQueue()->GetName()),
                                   String(DeferrableTaskQueue()->GetName()),
                                   String(PausableTaskQueue()->GetName()),
                                   String(UnpausableTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, LoadingTaskQueue()->GetName(), &tasks));

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // loading task runs
  EXPECT_THAT(tasks,
              UnorderedElementsAre(String(LoadingTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName, LoadingTaskQueue()->GetName(), &tasks));
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
              UnorderedElementsAre(String(LoadingTaskQueue()->GetName())));
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

    auto loader_observer_added_after_stopped_handle =
        frame_scheduler_->AddLifecycleObserver(
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
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();
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
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();
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
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();
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
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();
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
  EnsureUseCaseNone();

  frame_scheduler_ =
      FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kMainFrame);

  // Main thread is in the loading use case.
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();
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
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();

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
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();

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
    base::FieldTrialParams params{{"HIGHEST", "HIGH"}, {"MEDIUM", "NORMAL"},
                                  {"LOW", "NORMAL"},   {"LOWEST", "LOW"},
                                  {"IDLE", "LOW"},     {"THROTTLED", "LOW"}};

    const char kStudyName[] = "ResourceFetchPriorityExperiment";
    const char kGroupName[] = "GroupName1";

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
    base::FieldTrialParams params{{"HIGHEST", "HIGH"}, {"MEDIUM", "NORMAL"},
                                  {"LOW", "NORMAL"},   {"LOWEST", "LOW"},
                                  {"IDLE", "LOW"},     {"THROTTLED", "LOW"}};

    const char kStudyName[] = "ResourceFetchPriorityExperiment";
    const char kGroupName[] = "GroupName2";

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
  EnsureUseCaseLoading();

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
  EnsureUseCaseLoading();

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
  EnsureUseCaseNone();
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
  EXPECT_EQ(GetTaskQueue(TaskType::kWebSocket), DeferrableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kDatabaseAccess), PausableTaskQueue());
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
  ThrottleAndFreezeTaskTypesExperimentTest(const base::FieldTrialParams& params,
                                           const char* group_name) {
    scoped_feature_list().InitAndEnableFeatureWithParameters(
        kThrottleAndFreezeTaskTypes, params);
  }
};

class ThrottleableAndFreezableTaskTypesTest
    : public ThrottleAndFreezeTaskTypesExperimentTest {
 public:
  ThrottleableAndFreezableTaskTypesTest()
      : ThrottleAndFreezeTaskTypesExperimentTest(
            base::FieldTrialParams{
                // Leading spaces are allowed.
                {kThrottleableTaskTypesListParam, "PostedMessage"},
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
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBeThrottled(true)
                .SetCanBeFrozen(true)
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kMediaElementEvent);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBeFrozen(true)
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kDOMManipulation);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBeFrozen(true)
                .SetCanBeDeferred(true)
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  // Test some task types that were not configured through field trial
  // parameters.
  task_queue = GetTaskQueue(TaskType::kInternalIPC);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kMiscPlatformAPI);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBeDeferred(true)
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));
}


class FreezableOnlyTaskTypesTest
    : public ThrottleAndFreezeTaskTypesExperimentTest {
 public:
  FreezableOnlyTaskTypesTest()
      : ThrottleAndFreezeTaskTypesExperimentTest(
            base::FieldTrialParams{
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
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeFrozen(true)
                                              .SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kMediaElementEvent);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeFrozen(true)
                                              .SetCanBePaused(true));

  task_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
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

  task_queue = GetTaskQueue(TaskType::kMiscPlatformAPI);
  EXPECT_EQ(task_queue->GetQueueTraits(), MainThreadTaskQueue::QueueTraits()
                                              .SetCanBeDeferred(true)
                                              .SetCanBePaused(true));
}

class ThrottleableOnlyTaskTypesTest
    : public ThrottleAndFreezeTaskTypesExperimentTest {
 public:
  ThrottleableOnlyTaskTypesTest()
      : ThrottleAndFreezeTaskTypesExperimentTest(
            base::FieldTrialParams{
                {kFreezableTaskTypesListParam, ""},
                {kThrottleableTaskTypesListParam, "PostedMessage"}},
            "Group3") {}
};

TEST_F(ThrottleableOnlyTaskTypesTest, QueueTraitsFromFieldTrialParams) {
  if (base::FeatureList::IsEnabled(blink::features::kStopNonTimersInBackground))
    return;

  // These tests will start to fail if the default task queues or queue traits
  // change for these task types.

  // Check that the overrides work.
  auto task_queue = GetTaskQueue(TaskType::kPostedMessage);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBeThrottled(true)
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kMediaElementEvent);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kDOMManipulation);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBeDeferred(true)
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));

  // Test some task types that were not configured through field trial
  // parameters.
  task_queue = GetTaskQueue(TaskType::kInternalIPC);
  EXPECT_EQ(
      task_queue->GetQueueTraits(),
      MainThreadTaskQueue::QueueTraits().SetCanRunWhenVirtualTimePaused(false));

  task_queue = GetTaskQueue(TaskType::kMiscPlatformAPI);
  EXPECT_EQ(task_queue->GetQueueTraits(),
            MainThreadTaskQueue::QueueTraits()
                .SetCanBeDeferred(true)
                .SetCanBePaused(true)
                .SetCanRunWhenVirtualTimePaused(false));
}

class FrameSchedulerImplDatabaseAccessWithoutHighPriority
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplDatabaseAccessWithoutHighPriority()
      : FrameSchedulerImplTest({}, {kHighPriorityDatabaseTaskType}) {}
};

TEST_F(FrameSchedulerImplDatabaseAccessWithoutHighPriority, QueueTraits) {
  // These tests will start to fail if the default task queues or queue traits
  // change for these task types.

  int counter = 0;

  auto loading_queue = GetTaskQueue(TaskType::kInternalContinueScriptLoading);
  EXPECT_EQ(loading_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kVeryHighPriority);
  loading_queue->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ExpectAndIncrementCounter, 0,
                                base::Unretained(&counter)));

  auto da_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(da_queue->GetQueueTraits().prioritisation_type,
            MainThreadTaskQueue::QueueTraits::PrioritisationType::kRegular);
  EXPECT_EQ(da_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  da_queue->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ExpectAndIncrementCounter, 1,
                                base::Unretained(&counter)));

  auto content_queue = GetTaskQueue(TaskType::kInternalContentCapture);
  EXPECT_EQ(content_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  content_queue->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ExpectAndIncrementCounter, 2,
                                base::Unretained(&counter)));

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, counter);
}

class FrameSchedulerImplDatabaseAccessWithHighPriority
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplDatabaseAccessWithHighPriority()
      : FrameSchedulerImplTest({kHighPriorityDatabaseTaskType}, {}) {}
};

TEST_F(FrameSchedulerImplDatabaseAccessWithHighPriority, QueueTraits) {
  // These tests will start to fail if the default task queues or queue traits
  // change for these task types.

  int counter = 0;

  auto loading_queue = GetTaskQueue(TaskType::kInternalContinueScriptLoading);
  EXPECT_EQ(loading_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kVeryHighPriority);
  loading_queue->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ExpectAndIncrementCounter, 0,
                                base::Unretained(&counter)));

  auto da_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(da_queue->GetQueueTraits().prioritisation_type,
            MainThreadTaskQueue::QueueTraits::PrioritisationType::kHigh);
  EXPECT_EQ(da_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  da_queue->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ExpectAndIncrementCounter, 1,
                                base::Unretained(&counter)));

  auto pausable_queue = PausableTaskQueue();
  EXPECT_EQ(pausable_queue->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  pausable_queue->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ExpectAndIncrementCounter, 2,
                                base::Unretained(&counter)));

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, counter);
}

TEST_F(FrameSchedulerImplTest, ContentCaptureHasIdleTaskQueue) {
  auto task_queue = GetTaskQueue(TaskType::kInternalContentCapture);

  EXPECT_TRUE(task_queue->FixedPriority().has_value());
  EXPECT_EQ(TaskQueue::QueuePriority::kBestEffortPriority,
            task_queue->FixedPriority().value());
}

TEST_F(FrameSchedulerImplTest, ComputePriorityForDetachedFrame) {
  auto task_queue = GetTaskQueue(TaskType::kJavascriptTimer);
  // Just check that it does not crash.
  page_scheduler_.reset();
  frame_scheduler_->ComputePriority(task_queue.get());
}

namespace {

// Mask is a preferred way of plumbing the list of features, but a list
// is more convenient to read in the tests.
// Here we ensure that these two methods are equivalent.
uint64_t ComputeMaskFromFeatures(FrameSchedulerImpl* frame_scheduler) {
  uint64_t result = 0;
  for (SchedulingPolicy::Feature feature :
       frame_scheduler->GetActiveFeaturesTrackedForBackForwardCacheMetrics()) {
    result |= (1 << static_cast<size_t>(feature));
  }
  return result;
}

}  // namespace

TEST_F(FrameSchedulerImplTest, BackForwardCacheOptOut) {
  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre());
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  auto feature_handle1 = frame_scheduler_->RegisterFeature(
      SchedulingPolicy::Feature::kWebSocket,
      {SchedulingPolicy::RecordMetricsForBackForwardCache()});

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  auto feature_handle2 = frame_scheduler_->RegisterFeature(
      SchedulingPolicy::Feature::kWebRTC,
      {SchedulingPolicy::RecordMetricsForBackForwardCache()});

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket,
                                    SchedulingPolicy::Feature::kWebRTC));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  feature_handle1.reset();

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebRTC));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  feature_handle2.reset();

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre());
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));
}

TEST_F(FrameSchedulerImplTest, BackForwardCacheOptOut_FrameNavigated) {
  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre());
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  auto feature_handle = frame_scheduler_->RegisterFeature(
      SchedulingPolicy::Feature::kWebSocket,
      {SchedulingPolicy::RecordMetricsForBackForwardCache()});

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  frame_scheduler_->RegisterStickyFeature(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
      {SchedulingPolicy::RecordMetricsForBackForwardCache()});

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(
          SchedulingPolicy::Feature::kWebSocket,
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  // Same document navigations don't affect anything.
  frame_scheduler_->DidCommitProvisionalLoad(
      false, FrameScheduler::NavigationType::kSameDocument);
  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(
          SchedulingPolicy::Feature::kWebSocket,
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  // Regular navigations reset all features.
  frame_scheduler_->DidCommitProvisionalLoad(
      false, FrameScheduler::NavigationType::kOther);
  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre());
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  // Resetting a feature handle after navigation shouldn't do anything.
  feature_handle.reset();

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre());
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));
}

TEST_F(FrameSchedulerImplTest, FeatureUpload) {
  ResetFrameScheduler(FrameScheduler::FrameType::kMainFrame);

  frame_scheduler_->GetTaskRunner(TaskType::kJavascriptTimer)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](FrameSchedulerImpl* frame_scheduler,
                 testing::StrictMock<FrameSchedulerDelegateForTesting>*
                     delegate) {
                frame_scheduler->RegisterStickyFeature(
                    SchedulingPolicy::Feature::
                        kMainResourceHasCacheControlNoStore,
                    {SchedulingPolicy::RecordMetricsForBackForwardCache()});
                frame_scheduler->RegisterStickyFeature(
                    SchedulingPolicy::Feature::
                        kMainResourceHasCacheControlNoCache,
                    {SchedulingPolicy::RecordMetricsForBackForwardCache()});
                // Ensure that the feature upload is delayed.
                testing::Mock::VerifyAndClearExpectations(delegate);
                EXPECT_CALL(
                    *delegate,
                    UpdateActiveSchedulerTrackedFeatures(
                        (1 << static_cast<size_t>(
                             SchedulingPolicy::Feature::
                                 kMainResourceHasCacheControlNoStore)) |
                        (1 << static_cast<size_t>(
                             SchedulingPolicy::Feature::
                                 kMainResourceHasCacheControlNoCache))));
              },
              frame_scheduler_.get(), frame_scheduler_delegate_.get()));

  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(frame_scheduler_delegate_.get());
}

TEST_F(FrameSchedulerImplTest, FeatureUpload_FrameDestruction) {
  ResetFrameScheduler(FrameScheduler::FrameType::kMainFrame);

  FeatureHandle feature_handle;

  frame_scheduler_->GetTaskRunner(TaskType::kJavascriptTimer)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](FrameSchedulerImpl* frame_scheduler,
                 testing::StrictMock<FrameSchedulerDelegateForTesting>*
                     delegate,
                 FeatureHandle* feature_handle) {
                *feature_handle = frame_scheduler->RegisterFeature(
                    SchedulingPolicy::Feature::kWebSocket,
                    {SchedulingPolicy::RecordMetricsForBackForwardCache()});
                // Ensure that the feature upload is delayed.
                testing::Mock::VerifyAndClearExpectations(delegate);
                EXPECT_CALL(*delegate,
                            UpdateActiveSchedulerTrackedFeatures(
                                (1 << static_cast<size_t>(
                                     SchedulingPolicy::Feature::kWebSocket))));
              },
              frame_scheduler_.get(), frame_scheduler_delegate_.get(),
              &feature_handle));
  frame_scheduler_->GetTaskRunner(TaskType::kJavascriptTimer)
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     [](FrameSchedulerImpl* frame_scheduler,
                        testing::StrictMock<FrameSchedulerDelegateForTesting>*
                            delegate,
                        FeatureHandle* feature_handle) {
                       feature_handle->reset();
                       ResetForNavigation(frame_scheduler);
                       // Ensure that we don't upload the features for frame
                       // destruction.
                       testing::Mock::VerifyAndClearExpectations(delegate);
                       EXPECT_CALL(
                           *delegate,
                           UpdateActiveSchedulerTrackedFeatures(testing::_))
                           .Times(0);
                     },
                     frame_scheduler_.get(), frame_scheduler_delegate_.get(),
                     &feature_handle));

  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(frame_scheduler_delegate_.get());
}

class WebSchedulingTaskQueueTest : public FrameSchedulerImplTest {
 public:
  void SetUp() override {
    FrameSchedulerImplTest::SetUp();

    for (int i = 0; i <= static_cast<int>(WebSchedulingPriority::kLastPriority);
         i++) {
      WebSchedulingPriority priority = static_cast<WebSchedulingPriority>(i);
      // We only need the TaskRunner, so it's ok that the WebSchedulingTaskQueue
      // gets destroyed right away.
      std::unique_ptr<WebSchedulingTaskQueue> task_queue =
          frame_scheduler_->CreateWebSchedulingTaskQueue(priority);
      web_scheduling_task_runners_.push_back(task_queue->GetTaskRunner());
    }
  }

  void TearDown() override {
    FrameSchedulerImplTest::TearDown();

    web_scheduling_task_runners_.clear();
  }

 protected:
  // Helper for posting tasks to a WebSchedulingTaskQueue. |task_descriptor| is
  // a string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task queue priority:
  // - 'I': Immediate
  // - 'H': High
  // - 'D': Default
  // - 'L': Low
  // - 'E': Idle
  void PostWebSchedulingTestTasks(Vector<String>* run_order,
                                  const String& task_descriptor) {
    std::istringstream stream(task_descriptor.Utf8());
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      WebSchedulingPriority priority;
      switch (task[0]) {
        case 'I':
          priority = WebSchedulingPriority::kImmediatePriority;
          break;
        case 'H':
          priority = WebSchedulingPriority::kHighPriority;
          break;
        case 'D':
          priority = WebSchedulingPriority::kDefaultPriority;
          break;
        case 'L':
          priority = WebSchedulingPriority::kLowPriority;
          break;
        case 'E':
          priority = WebSchedulingPriority::kIdlePriority;
          break;
        default:
          EXPECT_FALSE(true);
          return;
      }
      web_scheduling_task_runners_[static_cast<int>(priority)]->PostTask(
          FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                    String::FromUTF8(task)));
    }
  }

  Vector<scoped_refptr<base::SingleThreadTaskRunner>>
      web_scheduling_task_runners_;
};

TEST_F(WebSchedulingTaskQueueTest, TasksRunInPriorityOrder) {
  Vector<String> run_order;

  PostWebSchedulingTestTasks(&run_order, "E1 E2 L1 L2 D1 D2 H1 H2 I1 I2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("I1", "I2", "H1", "H2", "D1",
                                              "D2", "L1", "L2", "E1", "E2"));
}

}  // namespace frame_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
