// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <map>
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
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/resource_loading_task_runner_handle_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
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
using PrioritisationType = MainThreadTaskQueue::QueueTraits::PrioritisationType;
using testing::Return;

namespace {

constexpr base::TimeDelta kDefaultThrottledWakeUpInterval =
    PageSchedulerImpl::kDefaultThrottledWakeUpInterval;
constexpr auto kShortDelay = base::TimeDelta::FromMilliseconds(10);

// This is a wrapper around MainThreadSchedulerImpl::CreatePageScheduler, that
// returns the PageScheduler as a PageSchedulerImpl.
std::unique_ptr<PageSchedulerImpl> CreatePageScheduler(
    PageScheduler::Delegate* page_scheduler_delegate,
    MainThreadSchedulerImpl* scheduler,
    WebAgentGroupScheduler& agent_group_scheduler) {
  std::unique_ptr<PageScheduler> page_scheduler =
      agent_group_scheduler.AsAgentGroupScheduler().CreatePageScheduler(
          page_scheduler_delegate);
  std::unique_ptr<PageSchedulerImpl> page_scheduler_impl(
      static_cast<PageSchedulerImpl*>(page_scheduler.release()));
  return page_scheduler_impl;
}

// This is a wrapper around PageSchedulerImpl::CreateFrameScheduler, that
// returns the FrameScheduler as a FrameSchedulerImpl.
std::unique_ptr<FrameSchedulerImpl> CreateFrameScheduler(
    PageSchedulerImpl* page_scheduler,
    FrameScheduler::Delegate* delegate,
    blink::BlameContext* blame_context,
    FrameScheduler::FrameType frame_type) {
  auto frame_scheduler =
      page_scheduler->CreateFrameScheduler(delegate, blame_context, frame_type);
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_impl(
      static_cast<FrameSchedulerImpl*>(frame_scheduler.release()));
  return frame_scheduler_impl;
}

void RecordRunTime(std::vector<base::TimeTicks>* run_times) {
  run_times->push_back(base::TimeTicks::Now());
}

}  // namespace

// All TaskTypes that can be passed to
// FrameSchedulerImpl::CreateQueueTraitsForTaskType().
constexpr TaskType kAllFrameTaskTypes[] = {
    TaskType::kInternalContentCapture,
    TaskType::kJavascriptTimerImmediate,
    TaskType::kJavascriptTimerDelayedLowNesting,
    TaskType::kJavascriptTimerDelayedHighNesting,
    TaskType::kInternalLoading,
    TaskType::kNetworking,
    TaskType::kNetworkingWithURLLoaderAnnotation,
    TaskType::kNetworkingUnfreezable,
    TaskType::kNetworkingControl,
    TaskType::kDOMManipulation,
    TaskType::kHistoryTraversal,
    TaskType::kEmbed,
    TaskType::kCanvasBlobSerialization,
    TaskType::kRemoteEvent,
    TaskType::kWebSocket,
    TaskType::kMicrotask,
    TaskType::kUnshippedPortMessage,
    TaskType::kFileReading,
    TaskType::kPresentation,
    TaskType::kSensor,
    TaskType::kPerformanceTimeline,
    TaskType::kWebGL,
    TaskType::kIdleTask,
    TaskType::kInternalDefault,
    TaskType::kMiscPlatformAPI,
    TaskType::kFontLoading,
    TaskType::kApplicationLifeCycle,
    TaskType::kBackgroundFetch,
    TaskType::kPermission,
    TaskType::kPostedMessage,
    TaskType::kServiceWorkerClientMessage,
    TaskType::kWorkerAnimation,
    TaskType::kUserInteraction,
    TaskType::kMediaElementEvent,
    TaskType::kInternalWebCrypto,
    TaskType::kInternalMedia,
    TaskType::kInternalMediaRealTime,
    TaskType::kInternalUserInteraction,
    TaskType::kInternalIntersectionObserver,
    TaskType::kInternalFindInPage,
    TaskType::kInternalContinueScriptLoading,
    TaskType::kDatabaseAccess,
    TaskType::kInternalNavigationAssociated,
    TaskType::kInternalTest,
    TaskType::kWebLocks,
    TaskType::kInternalFrameLifecycleControl,
    TaskType::kInternalTranslation,
    TaskType::kInternalInspector,
    TaskType::kInternalNavigationAssociatedUnfreezable,
    TaskType::kInternalHighPriorityLocalFrame};

static_assert(
    static_cast<int>(TaskType::kCount) == 76,
    "When adding a TaskType, make sure that kAllFrameTaskTypes is updated.");

void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(std::move(value));
}

class FrameSchedulerDelegateForTesting : public FrameScheduler::Delegate {
 public:
  FrameSchedulerDelegateForTesting() = default;

  ~FrameSchedulerDelegateForTesting() override = default;

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

  // Constructs a FrameSchedulerImplTest with a list of features to enable and a
  // list of features to disable.
  FrameSchedulerImplTest(std::vector<base::Feature> features_to_enable,
                         std::vector<base::Feature> features_to_disable)
      : FrameSchedulerImplTest() {
    feature_list_.InitWithFeatures(features_to_enable, features_to_disable);
  }

  // Constructs a FrameSchedulerImplTest with a feature to enable, associated
  // params, and a list of features to disable.
  FrameSchedulerImplTest(const base::Feature& feature_to_enable,
                         const base::FieldTrialParams& feature_to_enable_params,
                         const std::vector<base::Feature>& features_to_disable)
      : FrameSchedulerImplTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{feature_to_enable, feature_to_enable_params}}, features_to_disable);
  }

  ~FrameSchedulerImplTest() override = default;

  void SetUp() override {
    scheduler_ = std::make_unique<MainThreadSchedulerImpl>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock()),
        base::nullopt);
    agent_group_scheduler_ = scheduler_->CreateAgentGroupScheduler();
    page_scheduler_ =
        CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
    frame_scheduler_delegate_ = std::make_unique<
        testing::StrictMock<FrameSchedulerDelegateForTesting>>();
    frame_scheduler_ = CreateFrameScheduler(
        page_scheduler_.get(), frame_scheduler_delegate_.get(), nullptr,
        FrameScheduler::FrameType::kSubframe);
  }

  void ResetFrameScheduler(FrameScheduler::FrameType frame_type) {
    auto new_delegate_ = std::make_unique<
        testing::StrictMock<FrameSchedulerDelegateForTesting>>();
    frame_scheduler_ = CreateFrameScheduler(
        page_scheduler_.get(), new_delegate_.get(), nullptr, frame_type);
    frame_scheduler_delegate_ = std::move(new_delegate_);
  }

  void StorePageInBackForwardCache() {
    page_scheduler_->SetPageBackForwardCached(true);
  }

  void RestorePageFromBackForwardCache() {
    page_scheduler_->SetPageBackForwardCached(false);
  }

  void TearDown() override {
    throttleable_task_queue_.reset();
    frame_scheduler_.reset();
    page_scheduler_.reset();
    agent_group_scheduler_.reset();
    scheduler_->Shutdown();
    scheduler_.reset();
    frame_scheduler_delegate_.reset();
  }

  // Helper for posting several tasks of specific prioritisation types for
  // testing the relative order of tasks. |task_descriptor| is a string with
  // space delimited task identifiers. The first letter of each task identifier
  // specifies the prioritisation type:
  // - 'R': Regular (normal priority)
  // - 'V': Internal Script Continuation (very high priority)
  // - 'B': Best-effort
  // - 'D': Database
  void PostTestTasksForPrioritisationType(Vector<String>* run_order,
                                          const String& task_descriptor) {
    std::istringstream stream(task_descriptor.Utf8());
    PrioritisationType prioritisation_type;
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'R':
          prioritisation_type = PrioritisationType::kRegular;
          break;
        case 'V':
          prioritisation_type = PrioritisationType::kInternalScriptContinuation;
          break;
        case 'B':
          prioritisation_type = PrioritisationType::kBestEffort;
          break;
        case 'D':
          prioritisation_type = PrioritisationType::kExperimentalDatabase;
          break;
        default:
          EXPECT_FALSE(true);
          return;
      }
      auto queue_traits =
          FrameSchedulerImpl::PausableTaskQueueTraits().SetPrioritisationType(
              prioritisation_type);
      GetTaskQueue(queue_traits)
          ->GetTaskRunnerWithDefaultTaskType()
          ->PostTask(FROM_HERE,
                     base::BindOnce(&AppendToVectorTestTask, run_order,
                                    String::FromUTF8(task)));
    }
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

  // Fast-forwards to the next time aligned on |interval|.
  void FastForwardToAlignedTime(base::TimeDelta interval) {
    const base::TimeTicks now = base::TimeTicks::Now();
    const base::TimeTicks aligned =
        now.SnappedToNextTick(base::TimeTicks(), interval);
    if (aligned != now)
      task_environment_.FastForwardBy(aligned - now);
  }

  static uint64_t GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
      FrameSchedulerImpl* frame_scheduler) {
    return frame_scheduler
        ->GetActiveFeaturesTrackedForBackForwardCacheMetricsMask();
  }

 protected:
  scoped_refptr<MainThreadTaskQueue> throttleable_task_queue() {
    return throttleable_task_queue_;
  }

  void LazyInitThrottleableTaskQueue() {
    EXPECT_FALSE(throttleable_task_queue());
    throttleable_task_queue_ = ThrottleableTaskQueue();
    EXPECT_TRUE(throttleable_task_queue());
  }

  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(
      MainThreadTaskQueue::QueueTraits queue_traits) {
    return frame_scheduler_->FrameTaskQueueControllerForTest()->GetTaskQueue(
        queue_traits);
  }

  scoped_refptr<MainThreadTaskQueue> ThrottleableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::ThrottleableTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> JavaScriptTimerTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::ThrottleableTaskQueueTraits().SetPrioritisationType(
            PrioritisationType::kJavaScriptTimer));
  }

  scoped_refptr<MainThreadTaskQueue> JavaScriptTimerNonThrottleableTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::DeferrableTaskQueueTraits().SetPrioritisationType(
            PrioritisationType::kJavaScriptTimer));
  }

  scoped_refptr<MainThreadTaskQueue> LoadingTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::LoadingTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> LoadingControlTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::LoadingControlTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> UnfreezableLoadingTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::UnfreezableLoadingTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> DeferrableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::DeferrableTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> PausableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::PausableTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> UnpausableTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::UnpausableTaskQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> ForegroundOnlyTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::ForegroundOnlyTaskQueueTraits());
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
        throttleable_task_queue()->GetTaskQueue());
  }

  bool IsTaskTypeThrottled(TaskType task_type) {
    scoped_refptr<MainThreadTaskQueue> task_queue = GetTaskQueue(task_type);
    return scheduler_->task_queue_throttler()->IsThrottled(
        task_queue->GetTaskQueue());
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
  std::unique_ptr<WebAgentGroupScheduler> agent_group_scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
  std::unique_ptr<testing::StrictMock<FrameSchedulerDelegateForTesting>>
      frame_scheduler_delegate_;
  scoped_refptr<MainThreadTaskQueue> throttleable_task_queue_;
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

class FrameSchedulerImplStopInBackgroundDisabledTest
    : public FrameSchedulerImplTest,
      public ::testing::WithParamInterface<TaskType> {
 public:
  FrameSchedulerImplStopInBackgroundDisabledTest()
      : FrameSchedulerImplTest({}, {blink::features::kStopInBackground}) {}
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

void RecordQueueName(String name, Vector<String>* tasks) {
  tasks->push_back(std::move(name));
}

// Simulate running a task of a particular length by fast forwarding the task
// environment clock, which is used to determine the wall time of a task.
void RunTaskOfLength(base::test::TaskEnvironment* task_environment,
                     base::TimeDelta length) {
  task_environment->FastForwardBy(length);
}

class FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase
    : public FrameSchedulerImplTest {
 public:
  using Super = FrameSchedulerImplTest;

  explicit FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase(
      bool can_intensively_throttle_low_nesting_level)
      : FrameSchedulerImplTest(
            features::kIntensiveWakeUpThrottling,
            // If |can_intensively_throttle_low_nesting_level| is true, set the
            // feature param to "true". Otherwise, test the default behavior (no
            // feature params).
            can_intensively_throttle_low_nesting_level
                ? base::FieldTrialParams(
                      {{kIntensiveWakeUpThrottling_CanIntensivelyThrottleLowNestingLevel_Name,
                        "true"}})
                : base::FieldTrialParams(),
            {features::kStopInBackground}) {}

  void SetUp() override {
    Super::SetUp();
    ClearIntensiveWakeUpThrottlingPolicyOverrideCacheForTesting();
  }

  void TearDown() override {
    ClearIntensiveWakeUpThrottlingPolicyOverrideCacheForTesting();
    Super::TearDown();
  }

  const int kNumTasks = 5;
  const base::TimeDelta kGracePeriod =
      GetIntensiveWakeUpThrottlingGracePeriod();
  const base::TimeDelta kIntensiveThrottlingDurationBetweenWakeUps =
      GetIntensiveWakeUpThrottlingDurationBetweenWakeUps();
};

// Test param for FrameSchedulerImplTestWithIntensiveWakeUpThrottling
struct IntensiveWakeUpThrottlingTestParam {
  // TaskType used to obtain TaskRunners from the FrameScheduler.
  TaskType task_type;
  // Whether the feature param to allow throttling of timers with a low nesting
  // level should be set to "true" at the beginning of the test.
  bool can_intensively_throttle_low_nesting_level;
  // Whether it is expected that tasks will be intensively throttled.
  bool is_intensive_throttling_expected;
};

class FrameSchedulerImplTestWithIntensiveWakeUpThrottling
    : public FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase,
      public ::testing::WithParamInterface<IntensiveWakeUpThrottlingTestParam> {
 public:
  FrameSchedulerImplTestWithIntensiveWakeUpThrottling()
      : FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase(
            GetParam().can_intensively_throttle_low_nesting_level) {}

  TaskType GetTaskType() const { return GetParam().task_type; }
  bool IsIntensiveThrottlingExpected() const {
    return GetParam().is_intensive_throttling_expected;
  }

  base::TimeDelta GetExpectedWakeUpInterval() const {
    if (IsIntensiveThrottlingExpected())
      return kIntensiveThrottlingDurationBetweenWakeUps;
    return kDefaultThrottledWakeUpInterval;
  }
};

class FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride
    : public FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase {
 public:
  FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride()
      : FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase(
            /* can_intensively_throttle_low_nesting_level=*/false) {}

  // This should only be called once per test, and prior to the
  // PageSchedulerImpl logic actually parsing the policy switch.
  void SetPolicyOverride(bool enabled) {
    DCHECK(!scoped_command_line_.GetProcessCommandLine()->HasSwitch(
        switches::kIntensiveWakeUpThrottlingPolicy));
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kIntensiveWakeUpThrottlingPolicy,
        enabled ? switches::kIntensiveWakeUpThrottlingPolicy_ForceEnable
                : switches::kIntensiveWakeUpThrottlingPolicy_ForceDisable);
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

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
  frame_scheduler_->SetCrossOriginToMainFrame(true);
  frame_scheduler_->SetCrossOriginToMainFrame(false);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOriginToMainFrame(true);
  EXPECT_TRUE(IsThrottled());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOriginToMainFrame(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest,
       FrameHidden_CrossOrigin_NoThrottling_ExplicitInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOriginToMainFrame(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_NoThrottling_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(false);
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOriginToMainFrame(true);
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
  frame_scheduler_->SetCrossOriginToMainFrame(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_LazyInit) {
  ScopedTimerThrottlingForHiddenFramesForTest throttle_hidden_frames(true);
  frame_scheduler_->SetFrameVisible(true);
  frame_scheduler_->SetCrossOriginToMainFrame(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PauseAndResume) {
  int counter = 0;
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
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
  EXPECT_TRUE(LoadingTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(ThrottleableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(DeferrableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(PausableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(UnpausableTaskQueue()->GetTaskQueue()->IsQueueEnabled());

  frame_scheduler_->SetPreemptedForCooperativeScheduling(
      FrameOrWorkerScheduler::Preempted(true));
  EXPECT_FALSE(LoadingTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(ThrottleableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(DeferrableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(PausableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_FALSE(UnpausableTaskQueue()->GetTaskQueue()->IsQueueEnabled());

  frame_scheduler_->SetPreemptedForCooperativeScheduling(
      FrameOrWorkerScheduler::Preempted(false));
  EXPECT_TRUE(LoadingTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(ThrottleableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(DeferrableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(PausableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
  EXPECT_TRUE(UnpausableTaskQueue()->GetTaskQueue()->IsQueueEnabled());
}

namespace {

// A task that re-posts itself with a delay in order until it has run
// |num_remaining_tasks| times.
void RePostTask(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                base::TimeDelta delay,
                int* num_remaining_tasks) {
  --(*num_remaining_tasks);
  if (*num_remaining_tasks > 0) {
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RePostTask, task_runner, delay,
                       base::Unretained(num_remaining_tasks)),
        delay);
  }
}

}  // namespace

// Verify that tasks in a throttled task queue cause 1 wake up per second, when
// intensive wake up throttling is disabled. Disable the kStopInBackground
// feature because it hides the effect of intensive wake up throttling.
TEST_P(FrameSchedulerImplStopInBackgroundDisabledTest, ThrottledTaskExecution) {
  constexpr auto kTaskPeriod = base::TimeDelta::FromSeconds(1);

  // This test posts enough tasks to run past the default intensive wake up
  // throttling grace period. This allows verifying that intensive wake up
  // throttling is disabled by default.
  constexpr int kNumTasks =
      base::TimeDelta::FromSeconds(
          kIntensiveWakeUpThrottling_GracePeriodSeconds_Default * 2)
          .IntDiv(kTaskPeriod);
  // This TaskRunner is throttled.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(GetParam());

  // Hide the page. This enables wake up throttling.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);

  // Post an initial task.
  int num_remaining_tasks = kNumTasks;
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RePostTask, task_runner, kShortDelay,
                     base::Unretained(&num_remaining_tasks)),
      kShortDelay);

  // A task should run every second.
  while (num_remaining_tasks > 0) {
    int previous_num_remaining_tasks = num_remaining_tasks;
    task_environment_.FastForwardBy(kTaskPeriod);
    EXPECT_EQ(previous_num_remaining_tasks - 1, num_remaining_tasks);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllTimerTaskTypes,
    FrameSchedulerImplStopInBackgroundDisabledTest,
    testing::Values(TaskType::kJavascriptTimerDelayedLowNesting,
                    TaskType::kJavascriptTimerDelayedHighNesting),
    [](const testing::TestParamInfo<TaskType>& info) {
      return TaskTypeNames::TaskTypeToString(info.param);
    });

TEST_F(FrameSchedulerImplTest, FreezeForegroundOnlyTasks) {
  int counter = 0;
  ForegroundOnlyTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
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
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
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
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
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
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                base::TimeDelta::FromMilliseconds(10)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetTaskTime().is_zero());
  EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
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
    UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                  base::TimeDelta::FromMilliseconds(60)));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(GetTaskTime().is_zero());
    EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());

    DidCommitProvisionalLoad(test_case.navigation_type);

    UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                  base::TimeDelta::FromMilliseconds(60)));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(test_case.expect_task_time_zero, GetTaskTime().is_zero());
    EXPECT_EQ(test_case.expected_total_calls, GetTotalUpdateTaskTimeCalls());
  }
}

TEST_F(FrameSchedulerImplTest, PageFreezeWithKeepActive) {
  Vector<String> tasks;
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName,
                     LoadingTaskQueue()->GetTaskQueue()->GetName(), &tasks));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName,
                     ThrottleableTaskQueue()->GetTaskQueue()->GetName(),
                     &tasks));
  DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName,
                     DeferrableTaskQueue()->GetTaskQueue()->GetName(), &tasks));
  PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName,
                     PausableTaskQueue()->GetTaskQueue()->GetName(), &tasks));
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName,
                     UnpausableTaskQueue()->GetTaskQueue()->GetName(), &tasks));

  page_scheduler_->SetKeepActive(true);  // say we have a Service Worker
  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // Everything runs except throttleable tasks (timers)
  EXPECT_THAT(tasks,
              UnorderedElementsAre(
                  String(LoadingTaskQueue()->GetTaskQueue()->GetName()),
                  String(DeferrableTaskQueue()->GetTaskQueue()->GetName()),
                  String(PausableTaskQueue()->GetTaskQueue()->GetName()),
                  String(UnpausableTaskQueue()->GetTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName,
                     LoadingTaskQueue()->GetTaskQueue()->GetName(), &tasks));

  EXPECT_THAT(tasks, UnorderedElementsAre());
  base::RunLoop().RunUntilIdle();
  // loading task runs
  EXPECT_THAT(tasks, UnorderedElementsAre(String(
                         LoadingTaskQueue()->GetTaskQueue()->GetName())));

  tasks.clear();
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordQueueName,
                     LoadingTaskQueue()->GetTaskQueue()->GetName(), &tasks));
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
  EXPECT_THAT(tasks, UnorderedElementsAre(String(
                         LoadingTaskQueue()->GetTaskQueue()->GetName())));
}

TEST_F(FrameSchedulerImplStopNonTimersInBackgroundEnabledTest,
       PageFreezeAndPageVisible) {
  int counter = 0;
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
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

class FrameSchedulerImplTestWithUnfreezableLoading
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplTestWithUnfreezableLoading()
      : FrameSchedulerImplTest({blink::features::kLoadingTasksUnfreezable},
                               {}) {}
};

TEST_F(FrameSchedulerImplTestWithUnfreezableLoading,
       LoadingTasksKeepRunningWhenFrozen) {
  int counter = 0;
  UnfreezableLoadingTaskQueue()->GetTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  LoadingTaskQueue()->GetTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));

  page_scheduler_->SetPageVisible(false);
  page_scheduler_->SetPageFrozen(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  // Unfreezable tasks continue to run.
  EXPECT_EQ(1, counter);

  page_scheduler_->SetPageFrozen(false);

  EXPECT_EQ(1, counter);
  // Same as RunUntilIdle but also advances the clock if necessary.
  task_environment_.FastForwardUntilNoTasksRemain();
  // Freezable tasks resume.
  EXPECT_EQ(2, counter);
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

TEST_F(FrameSchedulerImplTest, LogIpcsPostedToFramesInBackForwardCache) {
  base::HistogramTester histogram_tester;

  // Create the task queue implicitly.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(TaskType::kInternalTest);

  StorePageInBackForwardCache();

  // Run the tasks so that they are recorded in the histogram
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  // Post IPC tasks, accounting for delay for when tracking starts.
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(1);
    task_runner->PostTask(FROM_HERE, base::DoNothing());
  }
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash_2(2);
    task_runner->PostTask(FROM_HERE, base::DoNothing());
  }
  task_environment_.RunUntilIdle();

  // Once the page is restored from the cache, IPCs should no longer be
  // recorded.
  RestorePageFromBackForwardCache();

  // Start posting tasks immediately - will not be recorded
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash_3(3);
    task_runner->PostTask(FROM_HERE, base::DoNothing());
  }
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash_4(4);
    task_runner->PostTask(FROM_HERE, base::DoNothing());
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "BackForwardCache.Experimental."
          "UnexpectedIPCMessagePostedToCachedFrame.MethodHash"),
      testing::UnorderedElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));

  // TimeUntilIPCReceived should have values in the 300000 bucket corresponding
  // with the hour delay in task_environment_.FastForwardBy.
  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "BackForwardCache.Experimental."
          "UnexpectedIPCMessagePostedToCachedFrame.TimeUntilIPCReceived"),
      testing::UnorderedElementsAre(base::Bucket(300000, 2)));
}

TEST_F(FrameSchedulerImplTest,
       LogIpcsFromMultipleThreadsPostedToFramesInBackForwardCache) {
  base::HistogramTester histogram_tester;

  // Create the task queue explicitly to ensure it exists when the page enters
  // the back-forward cache, and that the IPC handler is registerd as well.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(TaskType::kInternalTest);

  StorePageInBackForwardCache();

  // Run the tasks so that they are recorded in the histogram
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
            base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(1);
            task_runner->PostTask(FROM_HERE, base::DoNothing());
          },
          task_runner));
  task_environment_.RunUntilIdle();

  base::RepeatingClosure restore_from_cache_callback = base::BindRepeating(
      &FrameSchedulerImplTest::RestorePageFromBackForwardCache,
      base::Unretained(this));

  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             base::RepeatingClosure restore_from_cache_callback) {
            {
              base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(2);
              task_runner->PostTask(FROM_HERE, base::DoNothing());
            }
            {
              // Once the page is restored from the cache, ensure that the IPC
              // restoring the page from the cache is not recorded as well.
              base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(3);
              task_runner->PostTask(FROM_HERE, restore_from_cache_callback);
            }
            {
              base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(4);
              task_runner->PostTask(FROM_HERE, base::DoNothing());
            }
          },
          task_runner, restore_from_cache_callback));
  task_environment_.RunUntilIdle();

  // Start posting tasks immediately - will not be recorded
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
            base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(5);
            task_runner->PostTask(FROM_HERE, base::DoNothing());
          },
          task_runner));
  task_environment_.RunUntilIdle();

  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
            base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(6);
            task_runner->PostTask(FROM_HERE, base::DoNothing());
          },
          task_runner));
  task_environment_.RunUntilIdle();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "BackForwardCache.Experimental."
          "UnexpectedIPCMessagePostedToCachedFrame.MethodHash"),
      testing::UnorderedElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));
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
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Visible Frame Task Queues.
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);
  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  // Hidden Frame Task Queues.
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  main_frame_scheduler->OnFirstMeaningfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kNone);
  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  frame_scheduler_ =
      CreateFrameScheduler(page_scheduler_.get(), nullptr, nullptr,
                           FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);
  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  main_frame_scheduler->OnFirstMeaningfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kNone);
  EXPECT_FALSE(page_scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_ =
      CreateFrameScheduler(page_scheduler_.get(), nullptr, nullptr,
                           FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);
  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread scheduler is no longer in loading use case.
  main_frame_scheduler->OnFirstMeaningfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kNone);
  EXPECT_FALSE(page_scheduler_->IsLoading());

  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_ =
      CreateFrameScheduler(page_scheduler_.get(), nullptr, nullptr,
                           FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);
  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread is no longer in loading use case.
  main_frame_scheduler->OnFirstMeaningfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kNone);

  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(LowPriorityThrottleableTaskDuringLoadingExperimentTest,
       MainFrameQueuesPriorities) {
  frame_scheduler_->OnFirstContentfulPaint();
  frame_scheduler_->OnFirstMeaningfulPaint();

  frame_scheduler_ =
      CreateFrameScheduler(page_scheduler_.get(), nullptr, nullptr,
                           FrameScheduler::FrameType::kMainFrame);

  // Main thread is in the loading use case.
  frame_scheduler_->OnFirstContentfulPaint();

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread is no longer in loading use case.
  frame_scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(page_scheduler_->IsLoading());

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetIsAdFrame();

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);
  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread scheduler is no longer in loading use case.
  main_frame_scheduler->OnFirstMeaningfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kNone);

  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetIsAdFrame();

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);
  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kBestEffortPriority);

  // Main thread scheduler is no longer in loading use case.
  main_frame_scheduler->OnFirstMeaningfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kNone);

  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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

  TaskQueue::QueuePriority priority =
      task_queue->GetTaskQueue()->GetQueuePriority();
  EXPECT_EQ(priority, TaskQueue::QueuePriority::kNormalPriority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOWEST);
  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::HIGHEST);
  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);

  std::unique_ptr<ResourceLoadingTaskRunnerHandleImpl> handle =
      GetResourceLoadingTaskRunnerHandleImpl();
  scoped_refptr<MainThreadTaskQueue> task_queue = handle->task_queue();

  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Experiment is only enabled during the loading phase.
  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOWEST);
  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  // Main thread scheduler is in the loading use case.
  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  handle = GetResourceLoadingTaskRunnerHandleImpl();
  task_queue = handle->task_queue();

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOWEST);
  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::HIGHEST);
  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(),
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

  TaskQueue::QueuePriority priority =
      task_queue->GetTaskQueue()->GetQueuePriority();

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::LOW);
  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(), priority);

  DidChangeResourceLoadingPriority(task_queue, net::RequestPriority::HIGHEST);
  EXPECT_EQ(task_queue->GetTaskQueue()->GetQueuePriority(), priority);
}

class LowPriorityCrossOriginTaskExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityCrossOriginTaskExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForCrossOrigin}, {}) {}
};

TEST_F(LowPriorityCrossOriginTaskExperimentTest, FrameQueuesPriorities) {
  EXPECT_FALSE(frame_scheduler_->IsCrossOriginToMainFrame());

  // Same Origin Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetCrossOriginToMainFrame(true);
  EXPECT_TRUE(frame_scheduler_->IsCrossOriginToMainFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
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
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kMainFrame);

  main_frame_scheduler->OnFirstContentfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kLoading);

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);

  frame_scheduler_->SetCrossOriginToMainFrame(true);
  EXPECT_TRUE(frame_scheduler_->IsCrossOriginToMainFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kLowPriority);

  // Main thread is no longer in loading use case.
  main_frame_scheduler->OnFirstMeaningfulPaint();
  ASSERT_EQ(scheduler_->current_use_case(), UseCase::kNone);
  EXPECT_FALSE(page_scheduler_->IsLoading());

  EXPECT_EQ(LoadingTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest, TaskTypeToTaskQueueMapping) {
  // Make sure the queue lookup and task type to queue traits map works as
  // expected. This test will fail if these task types are moved to different
  // default queues.
  EXPECT_EQ(GetTaskQueue(TaskType::kJavascriptTimerDelayedLowNesting),
            JavaScriptTimerTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kJavascriptTimerDelayedHighNesting),
            JavaScriptTimerTaskQueue());
    EXPECT_EQ(GetTaskQueue(TaskType::kJavascriptTimerImmediate),
              JavaScriptTimerNonThrottleableTaskQueue());

  EXPECT_EQ(GetTaskQueue(TaskType::kWebSocket), DeferrableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kDatabaseAccess), PausableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kPostedMessage), PausableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kWebLocks), UnpausableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kNetworking), LoadingTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kNetworkingControl),
            LoadingControlTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kInternalTranslation),
            ForegroundOnlyTaskQueue());
}

// Verify that kJavascriptTimer* are the only non-internal TaskType that can be
// throttled. This ensures that the Javascript timer throttling experiment only
// affects wake ups from Javascript timers https://crbug.com/1075553
TEST_F(FrameSchedulerImplTest, ThrottledTaskTypes) {
  page_scheduler_->SetPageVisible(false);

  for (TaskType task_type : kAllFrameTaskTypes) {
    SCOPED_TRACE(testing::Message()
                 << "TaskType is "
                 << TaskTypeNames::TaskTypeToString(task_type));
    switch (task_type) {
      case TaskType::kInternalContentCapture:
      case TaskType::kJavascriptTimerDelayedLowNesting:
      case TaskType::kJavascriptTimerDelayedHighNesting:
      case TaskType::kInternalTranslation:
        EXPECT_TRUE(IsTaskTypeThrottled(task_type));
        break;
      default:
        EXPECT_FALSE(IsTaskTypeThrottled(task_type));
        break;
    };
  }
}

class FrameSchedulerImplDatabaseAccessWithoutHighPriority
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplDatabaseAccessWithoutHighPriority()
      : FrameSchedulerImplTest({}, {kHighPriorityDatabaseTaskType}) {}
};

TEST_F(FrameSchedulerImplDatabaseAccessWithoutHighPriority, QueueTraits) {
  auto da_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(da_queue->GetQueueTraits().prioritisation_type,
            MainThreadTaskQueue::QueueTraits::PrioritisationType::kRegular);
  EXPECT_EQ(da_queue->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kNormalPriority);
}

class FrameSchedulerImplDatabaseAccessWithHighPriority
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplDatabaseAccessWithHighPriority()
      : FrameSchedulerImplTest({kHighPriorityDatabaseTaskType}, {}) {}
};

TEST_F(FrameSchedulerImplDatabaseAccessWithHighPriority, QueueTraits) {
  auto da_queue = GetTaskQueue(TaskType::kDatabaseAccess);
  EXPECT_EQ(da_queue->GetQueueTraits().prioritisation_type,
            MainThreadTaskQueue::QueueTraits::PrioritisationType::
                kExperimentalDatabase);
  EXPECT_EQ(da_queue->GetTaskQueue()->GetQueuePriority(),
            TaskQueue::QueuePriority::kHighPriority);
}

TEST_F(FrameSchedulerImplDatabaseAccessWithHighPriority, RunOrder) {
  Vector<String> run_order;
  PostTestTasksForPrioritisationType(&run_order, "D1 R1 D2 V1 B1");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("V1", "D1", "D2", "R1", "B1"));
}

TEST_F(FrameSchedulerImplDatabaseAccessWithHighPriority,
       NormalPriorityInBackground) {
  page_scheduler_->SetPageVisible(false);

  Vector<String> run_order;
  PostTestTasksForPrioritisationType(&run_order, "D1 R1 D2 V1 B1");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("V1", "D1", "R1", "D2", "B1"));
}

TEST_F(FrameSchedulerImplTest, ContentCaptureHasIdleTaskQueue) {
  auto task_queue = GetTaskQueue(TaskType::kInternalContentCapture);

  EXPECT_EQ(TaskQueue::QueuePriority::kBestEffortPriority,
            task_queue->GetTaskQueue()->GetQueuePriority());
}

TEST_F(FrameSchedulerImplTest, ComputePriorityForDetachedFrame) {
  auto task_queue = GetTaskQueue(TaskType::kJavascriptTimerDelayedLowNesting);
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

  frame_scheduler_->GetTaskRunner(TaskType::kJavascriptTimerImmediate)
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

  frame_scheduler_->GetTaskRunner(TaskType::kJavascriptTimerImmediate)
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
  frame_scheduler_->GetTaskRunner(TaskType::kJavascriptTimerImmediate)
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
      task_queues_.push_back(std::move(task_queue));
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
  // - 'U': UserBlocking
  // - 'V': UserVisible
  // - 'B': Background
  void PostWebSchedulingTestTasks(Vector<String>* run_order,
                                  const String& task_descriptor) {
    std::istringstream stream(task_descriptor.Utf8());
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      WebSchedulingPriority priority;
      switch (task[0]) {
        case 'U':
          priority = WebSchedulingPriority::kUserBlockingPriority;
          break;
        case 'V':
          priority = WebSchedulingPriority::kUserVisiblePriority;
          break;
        case 'B':
          priority = WebSchedulingPriority::kBackgroundPriority;
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

  Vector<std::unique_ptr<WebSchedulingTaskQueue>> task_queues_;
};

TEST_F(WebSchedulingTaskQueueTest, TasksRunInPriorityOrder) {
  Vector<String> run_order;

  PostWebSchedulingTestTasks(&run_order, "B1 B2 V1 V2 U1 U2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("U1", "U2", "V1", "V2", "B1", "B2"));
}

TEST_F(WebSchedulingTaskQueueTest, DynamicTaskPriorityOrder) {
  Vector<String> run_order;

  PostWebSchedulingTestTasks(&run_order, "B1 B2 V1 V2 U1 U2");
  task_queues_[static_cast<int>(WebSchedulingPriority::kUserBlockingPriority)]
      ->SetPriority(WebSchedulingPriority::kBackgroundPriority);

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("V1", "V2", "B1", "B2", "U1", "U2"));
}

// Verify that tasks posted with TaskType::kJavascriptTimerDelayed* run at the
// expected time when throttled.
TEST_F(FrameSchedulerImplTest, ThrottledJSTimerTasksRunTime) {
  constexpr TaskType kJavaScriptTimerTaskTypes[] = {
      TaskType::kJavascriptTimerDelayedLowNesting,
      TaskType::kJavascriptTimerDelayedHighNesting};

  // Snap the time to a multiple of 1 second. Otherwise, the exact run time
  // of throttled tasks after hiding the page will vary.
  FastForwardToAlignedTime(base::TimeDelta::FromSeconds(1));
  const base::TimeTicks start = base::TimeTicks::Now();

  // Hide the page to start throttling JS Timers.
  page_scheduler_->SetPageVisible(false);

  std::map<TaskType, std::vector<base::TimeTicks>> run_times;

  // Post tasks with each Javascript Timer Task Type.
  for (TaskType task_type : kJavaScriptTimerTaskTypes) {
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        frame_scheduler_->GetTaskRunner(task_type);

    // Note: Taking the address of an element in |run_times| is safe because
    // inserting elements in a map does not invalidate references.

    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::TimeDelta::FromMilliseconds(1000));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::TimeDelta::FromMilliseconds(1002));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::TimeDelta::FromMilliseconds(1004));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::TimeDelta::FromMilliseconds(2500));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::TimeDelta::FromMilliseconds(6000));
  }

  // Make posted tasks run.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  // The effective delay of a throttled task is >= the requested delay, and is
  // within [N * 1000, N * 1000 + 3] ms, where N is an integer. This is because
  // the wake up rate is 1 per second, and the duration of each wake up is 3 ms.
  for (TaskType task_type : kJavaScriptTimerTaskTypes) {
    EXPECT_THAT(
        run_times[task_type],
        testing::ElementsAre(start + base::TimeDelta::FromMilliseconds(0),
                             start + base::TimeDelta::FromMilliseconds(1000),
                             start + base::TimeDelta::FromMilliseconds(1002),
                             start + base::TimeDelta::FromMilliseconds(2000),
                             start + base::TimeDelta::FromMilliseconds(3000),
                             start + base::TimeDelta::FromMilliseconds(6000)));
  }
}

namespace {
class MockMainThreadScheduler : public MainThreadSchedulerImpl {
 public:
  explicit MockMainThreadScheduler(
      base::test::TaskEnvironment& task_environment)
      : MainThreadSchedulerImpl(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr,
                task_environment.GetMainThreadTaskRunner(),
                task_environment.GetMockTickClock()),
            base::nullopt) {}

  MOCK_METHOD(void, OnMainFramePaint, ());
};
}  // namespace

TEST_F(FrameSchedulerImplTest, ReportFMPAndFCPForMainFrames) {
  MockMainThreadScheduler mock_main_thread_scheduler{task_environment_};
  std::unique_ptr<WebAgentGroupScheduler> agent_group_scheduler =
      mock_main_thread_scheduler.CreateAgentGroupScheduler();
  std::unique_ptr<PageSchedulerImpl> page_scheduler = CreatePageScheduler(
      nullptr, &mock_main_thread_scheduler, *agent_group_scheduler);

  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler.get(), nullptr, nullptr,
                           FrameScheduler::FrameType::kMainFrame);

  EXPECT_CALL(mock_main_thread_scheduler, OnMainFramePaint).Times(2);

  main_frame_scheduler->OnFirstMeaningfulPaint();
  main_frame_scheduler->OnFirstContentfulPaint();

  main_frame_scheduler = nullptr;
  page_scheduler = nullptr;
  agent_group_scheduler = nullptr;
  mock_main_thread_scheduler.Shutdown();
}

TEST_F(FrameSchedulerImplTest, DontReportFMPAndFCPForSubframes) {
  MockMainThreadScheduler mock_main_thread_scheduler{task_environment_};
  std::unique_ptr<WebAgentGroupScheduler> agent_group_scheduler =
      mock_main_thread_scheduler.CreateAgentGroupScheduler();
  std::unique_ptr<PageSchedulerImpl> page_scheduler = CreatePageScheduler(
      nullptr, &mock_main_thread_scheduler, *agent_group_scheduler);

  std::unique_ptr<FrameSchedulerImpl> subframe_scheduler =
      CreateFrameScheduler(page_scheduler.get(), nullptr, nullptr,
                           FrameScheduler::FrameType::kSubframe);

  EXPECT_CALL(mock_main_thread_scheduler, OnMainFramePaint).Times(0);

  subframe_scheduler->OnFirstMeaningfulPaint();
  subframe_scheduler->OnFirstContentfulPaint();

  subframe_scheduler = nullptr;
  page_scheduler = nullptr;
  agent_group_scheduler = nullptr;
  mock_main_thread_scheduler.Shutdown();
}

// Verify that tasks run at the expected time in frame that is same-origin with
// the main frame with intensive wake up throttling.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       TaskExecutionSameOriginFrame) {
  ASSERT_FALSE(frame_scheduler_->IsCrossOriginToMainFrame());

  // Throttled TaskRunner to which tasks are posted in this test.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(GetTaskType());

  // Snap the time to a multiple of
  // |kIntensiveThrottlingDurationBetweenWakeUps|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);
  const base::TimeTicks test_start = base::TimeTicks::Now();

  // Hide the page. This starts the delay to throttle background wake ups.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);

  // Initially, wake ups are not intensively throttled.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start);
    std::vector<base::TimeTicks> run_times;

    for (int i = 0; i < kNumTasks; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kShortDelay + i * kDefaultThrottledWakeUpInterval);
    }

    task_environment_.FastForwardBy(kGracePeriod);
    EXPECT_THAT(run_times, testing::ElementsAre(
                               scope_start + base::TimeDelta::FromSeconds(1),
                               scope_start + base::TimeDelta::FromSeconds(2),
                               scope_start + base::TimeDelta::FromSeconds(3),
                               scope_start + base::TimeDelta::FromSeconds(4),
                               scope_start + base::TimeDelta::FromSeconds(5)));
  }

  // After the grace period:

  // Test that wake ups are 1-second aligned if there is no recent wake up.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(5));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kDefaultThrottledWakeUpInterval);
    EXPECT_THAT(run_times, testing::ElementsAre(
                               scope_start + base::TimeDelta::FromSeconds(1)));
  }

  // Test that if there is a recent wake up:
  //   TaskType can be intensively throttled:   Wake ups are 1-minute aligned
  //   Otherwise:                               Wake ups are 1-second aligned
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(5) +
                               base::TimeDelta::FromSeconds(1));
    std::vector<base::TimeTicks> run_times;

    for (int i = 0; i < kNumTasks; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kShortDelay + i * kDefaultThrottledWakeUpInterval);
    }

    FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);

    if (IsIntensiveThrottlingExpected()) {
      const base::TimeTicks aligned_time =
          scope_start + base::TimeDelta::FromSeconds(59);
      EXPECT_THAT(run_times,
                  testing::ElementsAre(aligned_time, aligned_time, aligned_time,
                                       aligned_time, aligned_time));
    } else {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(scope_start + base::TimeDelta::FromSeconds(1),
                               scope_start + base::TimeDelta::FromSeconds(2),
                               scope_start + base::TimeDelta::FromSeconds(3),
                               scope_start + base::TimeDelta::FromSeconds(4),
                               scope_start + base::TimeDelta::FromSeconds(5)));
    }
  }

  // Post an extra task with a short delay. The wake up should be 1-minute
  // aligned if the TaskType supports intensive throttling, or 1-second aligned
  // otherwise.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(6));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kIntensiveThrottlingDurationBetweenWakeUps);

    EXPECT_THAT(run_times, testing::ElementsAre(scope_start +
                                                GetExpectedWakeUpInterval()));
  }

  // Post an extra task with a delay longer than the intensive throttling wake
  // up interval. The wake up should be 1-second aligned, even if the TaskType
  // supports intensive throttling, because there was no wake up in the last
  // minute.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(7));
    std::vector<base::TimeTicks> run_times;

    const base::TimeDelta kLongDelay =
        kIntensiveThrottlingDurationBetweenWakeUps * 5 +
        kDefaultThrottledWakeUpInterval;
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times), kLongDelay);

    task_environment_.FastForwardBy(kLongDelay);
    EXPECT_THAT(run_times, testing::ElementsAre(scope_start + kLongDelay));
  }

  // Post tasks with short delays after the page communicated with the user in
  // background. Tasks should be 1-second aligned for 3 seconds. After that, if
  // the TaskType supports intensive throttling, wake ups should be 1-minute
  // aligned.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(12) +
                               kDefaultThrottledWakeUpInterval);
    std::vector<base::TimeTicks> run_times;

    page_scheduler_->OnTitleOrFaviconUpdated();
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          RecordRunTime(&run_times);
          for (int i = 0; i < kNumTasks; ++i) {
            task_runner->PostDelayedTask(
                FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
                kDefaultThrottledWakeUpInterval * (i + 1));
          }
          page_scheduler_->OnTitleOrFaviconUpdated();
        }),
        kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardUntilNoTasksRemain();

    if (IsIntensiveThrottlingExpected()) {
      EXPECT_THAT(run_times, testing::ElementsAre(
                                 scope_start + base::TimeDelta::FromSeconds(1),
                                 scope_start + base::TimeDelta::FromSeconds(2),
                                 scope_start + base::TimeDelta::FromSeconds(3),
                                 scope_start - kDefaultThrottledWakeUpInterval +
                                     base::TimeDelta::FromMinutes(1),
                                 scope_start - kDefaultThrottledWakeUpInterval +
                                     base::TimeDelta::FromMinutes(1),
                                 scope_start - kDefaultThrottledWakeUpInterval +
                                     base::TimeDelta::FromMinutes(1)));
    } else {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(scope_start + base::TimeDelta::FromSeconds(1),
                               scope_start + base::TimeDelta::FromSeconds(2),
                               scope_start + base::TimeDelta::FromSeconds(3),
                               scope_start + base::TimeDelta::FromSeconds(4),
                               scope_start + base::TimeDelta::FromSeconds(5),
                               scope_start + base::TimeDelta::FromSeconds(6)));
    }
  }
}

// Verify that tasks run at the expected time in a frame that is cross-origin
// with the main frame with intensive wake up throttling.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       TaskExecutionCrossOriginFrame) {
  frame_scheduler_->SetCrossOriginToMainFrame(true);

  // Throttled TaskRunner to which tasks are posted in this test.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(GetTaskType());

  // Snap the time to a multiple of
  // |kIntensiveThrottlingDurationBetweenWakeUps|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);
  const base::TimeTicks test_start = base::TimeTicks::Now();

  // Hide the page. This starts the delay to throttle background wake ups.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);

  // Initially, wake ups are not intensively throttled.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start);
    std::vector<base::TimeTicks> run_times;

    for (int i = 0; i < kNumTasks; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kShortDelay + i * kDefaultThrottledWakeUpInterval);
    }

    task_environment_.FastForwardBy(kGracePeriod);
    EXPECT_THAT(run_times, testing::ElementsAre(
                               scope_start + base::TimeDelta::FromSeconds(1),
                               scope_start + base::TimeDelta::FromSeconds(2),
                               scope_start + base::TimeDelta::FromSeconds(3),
                               scope_start + base::TimeDelta::FromSeconds(4),
                               scope_start + base::TimeDelta::FromSeconds(5)));
  }

  // After the grace period:

  // Test posting a task when there is no recent wake up. The wake up should be
  // 1-minute aligned if the TaskType supports intensive throttling (in a main
  // frame, it would have been 1-second aligned since there was no wake up in
  // the last minute). Otherwise, it should be 1-second aligned.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(5));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kIntensiveThrottlingDurationBetweenWakeUps);
    EXPECT_THAT(run_times, testing::ElementsAre(scope_start +
                                                GetExpectedWakeUpInterval()));
  }

  // Test posting many tasks with short delays. Wake ups should be 1-minute
  // aligned if the TaskType supports intensive throttling, or 1-second aligned
  // otherwise.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(6));
    std::vector<base::TimeTicks> run_times;

    for (int i = 0; i < kNumTasks; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kShortDelay + i * kDefaultThrottledWakeUpInterval);
    }

    task_environment_.FastForwardBy(kIntensiveThrottlingDurationBetweenWakeUps);

    if (IsIntensiveThrottlingExpected()) {
      const base::TimeTicks aligned_time =
          scope_start + kIntensiveThrottlingDurationBetweenWakeUps;
      EXPECT_THAT(run_times,
                  testing::ElementsAre(aligned_time, aligned_time, aligned_time,
                                       aligned_time, aligned_time));
    } else {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(scope_start + base::TimeDelta::FromSeconds(1),
                               scope_start + base::TimeDelta::FromSeconds(2),
                               scope_start + base::TimeDelta::FromSeconds(3),
                               scope_start + base::TimeDelta::FromSeconds(4),
                               scope_start + base::TimeDelta::FromSeconds(5)));
    }
  }

  // Post an extra task with a short delay. Wake ups should be 1-minute aligned
  // if the TaskType supports intensive throttling, or 1-second aligned
  // otherwise.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(7));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kIntensiveThrottlingDurationBetweenWakeUps);
    EXPECT_THAT(run_times, testing::ElementsAre(scope_start +
                                                GetExpectedWakeUpInterval()));
  }

  // Post an extra task with a delay longer than the intensive throttling wake
  // up interval. The wake up should be 1-minute aligned if the TaskType
  // supports intensive throttling (in a main frame, it would have been 1-second
  // aligned because there was no wake up in the last minute). Otherwise, it
  // should be 1-second aligned.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(8));
    std::vector<base::TimeTicks> run_times;

    const base::TimeDelta kLongDelay =
        kIntensiveThrottlingDurationBetweenWakeUps * 6;
    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kLongDelay - kShortDelay);

    task_environment_.FastForwardBy(kLongDelay);
    EXPECT_THAT(run_times, testing::ElementsAre(scope_start + kLongDelay));
  }

  // Post tasks with short delays after the page communicated with the user in
  // background. Wake ups should be 1-minute aligned if the TaskType supports
  // intensive throttling, since cross-origin frames are not affected by title
  // or favicon update. Otherwise, they should be 1-second aligned.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::TimeDelta::FromMinutes(14));
    std::vector<base::TimeTicks> run_times;

    page_scheduler_->OnTitleOrFaviconUpdated();
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          RecordRunTime(&run_times);
          for (int i = 0; i < kNumTasks; ++i) {
            task_runner->PostDelayedTask(
                FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
                kDefaultThrottledWakeUpInterval * (i + 1));
          }
          page_scheduler_->OnTitleOrFaviconUpdated();
        }),
        kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardUntilNoTasksRemain();

    if (IsIntensiveThrottlingExpected()) {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(scope_start + base::TimeDelta::FromMinutes(1),
                               scope_start + base::TimeDelta::FromMinutes(2),
                               scope_start + base::TimeDelta::FromMinutes(2),
                               scope_start + base::TimeDelta::FromMinutes(2),
                               scope_start + base::TimeDelta::FromMinutes(2),
                               scope_start + base::TimeDelta::FromMinutes(2)));
    } else {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(scope_start + base::TimeDelta::FromSeconds(1),
                               scope_start + base::TimeDelta::FromSeconds(2),
                               scope_start + base::TimeDelta::FromSeconds(3),
                               scope_start + base::TimeDelta::FromSeconds(4),
                               scope_start + base::TimeDelta::FromSeconds(5),
                               scope_start + base::TimeDelta::FromSeconds(6)));
    }
  }
}

// Verify that tasks from different frames that are same-origin with the main
// frame run at the expected time.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       ManySameOriginFrames) {
  ASSERT_FALSE(frame_scheduler_->IsCrossOriginToMainFrame());
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(GetTaskType());

  // Create a FrameScheduler that is same-origin with the main frame, and an
  // associated throttled TaskRunner.
  std::unique_ptr<FrameSchedulerImpl> other_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kSubframe);
  ASSERT_FALSE(other_frame_scheduler->IsCrossOriginToMainFrame());
  const scoped_refptr<base::SingleThreadTaskRunner> other_task_runner =
      other_frame_scheduler->GetTaskRunner(GetTaskType());

  // Snap the time to a multiple of
  // |kIntensiveThrottlingDurationBetweenWakeUps|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);

  // Hide the page and wait until the intensive throttling grace period has
  // elapsed.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);
  task_environment_.FastForwardBy(kGracePeriod);

  // Post tasks in both frames, with delays shorter than the intensive wake up
  // interval.
  const base::TimeTicks post_time = base::TimeTicks::Now();
  std::vector<base::TimeTicks> run_times;
  task_runner->PostDelayedTask(FROM_HERE,
                               base::BindOnce(&RecordRunTime, &run_times),
                               kDefaultThrottledWakeUpInterval + kShortDelay);
  other_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
      2 * kDefaultThrottledWakeUpInterval + kShortDelay);
  task_environment_.FastForwardUntilNoTasksRemain();

  // The first task is 1-second aligned, because there was no wake up in the
  // last minute. The second task is 1-minute aligned if the TaskType supports
  // intensive throttling, or 1-second aligned otherwise.
  if (IsIntensiveThrottlingExpected()) {
    EXPECT_THAT(run_times,
                testing::ElementsAre(
                    post_time + 2 * kDefaultThrottledWakeUpInterval,
                    post_time + kIntensiveThrottlingDurationBetweenWakeUps));
  } else {
    EXPECT_THAT(
        run_times,
        testing::ElementsAre(post_time + 2 * kDefaultThrottledWakeUpInterval,
                             post_time + 3 * kDefaultThrottledWakeUpInterval));
  }
}

// Verify that intensive throttling is disabled when there is an opt-out.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       AggressiveThrottlingOptOut) {
  constexpr int kNumTasks = 3;
  // |task_runner| is throttled.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(GetTaskType());
  // |other_task_runner| is throttled. It belongs to a different frame on the
  // same page.
  const auto other_frame_scheduler = CreateFrameScheduler(
      page_scheduler_.get(), frame_scheduler_delegate_.get(), nullptr,
      FrameScheduler::FrameType::kSubframe);
  const scoped_refptr<base::SingleThreadTaskRunner> other_task_runner =
      frame_scheduler_->GetTaskRunner(GetTaskType());

  // Fast-forward the time to a multiple of
  // |kIntensiveThrottlingDurationBetweenWakeUps|. Otherwise,
  // the time at which tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);

  // Hide the page and wait until the intensive throttling grace period has
  // elapsed.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);
  task_environment_.FastForwardBy(kGracePeriod);

  {
    // Wake ups are intensively throttled, since there is no throttling opt-out.
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    std::vector<base::TimeTicks> run_times;
    for (int i = 1; i < kNumTasks + 1; ++i) {
      task_runner->PostDelayedTask(FROM_HERE,
                                   base::BindOnce(&RecordRunTime, &run_times),
                                   i * kShortDelay);
    }
    for (int i = 1; i < kNumTasks + 1; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kDefaultThrottledWakeUpInterval + i * kShortDelay);
    }
    task_environment_.FastForwardUntilNoTasksRemain();
    if (IsIntensiveThrottlingExpected()) {
      // Note: Wake ups can be unaligned when there is no recent wake up.
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(
              scope_start + kDefaultThrottledWakeUpInterval,
              scope_start + kDefaultThrottledWakeUpInterval,
              scope_start + kDefaultThrottledWakeUpInterval,
              scope_start + kIntensiveThrottlingDurationBetweenWakeUps,
              scope_start + kIntensiveThrottlingDurationBetweenWakeUps,
              scope_start + kIntensiveThrottlingDurationBetweenWakeUps));
    } else {
      EXPECT_THAT(run_times,
                  testing::ElementsAre(
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + 2 * kDefaultThrottledWakeUpInterval,
                      scope_start + 2 * kDefaultThrottledWakeUpInterval,
                      scope_start + 2 * kDefaultThrottledWakeUpInterval));
    }
  }

  {
    // Create an opt-out.
    auto handle = frame_scheduler_->RegisterFeature(
        SchedulingPolicy::Feature::kWebRTC,
        {SchedulingPolicy::DisableAggressiveThrottling()});

    {
      // Tasks should run after |kDefaultThrottledWakeUpInterval|, since
      // aggressive throttling is disabled, but default wake up throttling
      // remains enabled.
      const base::TimeTicks scope_start = base::TimeTicks::Now();
      std::vector<base::TimeTicks> run_times;
      for (int i = 1; i < kNumTasks + 1; ++i) {
        task_runner->PostDelayedTask(FROM_HERE,
                                     base::BindOnce(&RecordRunTime, &run_times),
                                     i * kShortDelay);
      }
      task_environment_.FastForwardUntilNoTasksRemain();
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(scope_start + kDefaultThrottledWakeUpInterval,
                               scope_start + kDefaultThrottledWakeUpInterval,
                               scope_start + kDefaultThrottledWakeUpInterval));
    }

    {
      // Same thing for another frame on the same page.
      const base::TimeTicks scope_start = base::TimeTicks::Now();
      std::vector<base::TimeTicks> run_times;
      for (int i = 1; i < kNumTasks + 1; ++i) {
        other_task_runner->PostDelayedTask(
            FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
            i * kShortDelay);
      }
      task_environment_.FastForwardUntilNoTasksRemain();
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(scope_start + kDefaultThrottledWakeUpInterval,
                               scope_start + kDefaultThrottledWakeUpInterval,
                               scope_start + kDefaultThrottledWakeUpInterval));
    }
  }

  // Fast-forward so that there is no recent wake up. Then, align the time on
  // |kIntensiveThrottlingDurationBetweenWakeUps| to simplify expectations.
  task_environment_.FastForwardBy(kIntensiveThrottlingDurationBetweenWakeUps);
  FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);

  {
    // Wake ups are intensively throttled, since there is no throttling opt-out.
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    std::vector<base::TimeTicks> run_times;
    for (int i = 1; i < kNumTasks + 1; ++i) {
      task_runner->PostDelayedTask(FROM_HERE,
                                   base::BindOnce(&RecordRunTime, &run_times),
                                   i * kShortDelay);
    }
    for (int i = 1; i < kNumTasks + 1; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kDefaultThrottledWakeUpInterval + i * kShortDelay);
    }
    task_environment_.FastForwardUntilNoTasksRemain();
    if (IsIntensiveThrottlingExpected()) {
      // Note: Wake ups can be unaligned when there is no recent wake up.
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(
              scope_start + kDefaultThrottledWakeUpInterval,
              scope_start + kDefaultThrottledWakeUpInterval,
              scope_start + kDefaultThrottledWakeUpInterval,
              scope_start + kIntensiveThrottlingDurationBetweenWakeUps,
              scope_start + kIntensiveThrottlingDurationBetweenWakeUps,
              scope_start + kIntensiveThrottlingDurationBetweenWakeUps));
    } else {
      EXPECT_THAT(run_times,
                  testing::ElementsAre(
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + 2 * kDefaultThrottledWakeUpInterval,
                      scope_start + 2 * kDefaultThrottledWakeUpInterval,
                      scope_start + 2 * kDefaultThrottledWakeUpInterval));
    }
  }
}

// Verify that tasks run at the same time when a frame switches between being
// same-origin and cross-origin with the main frame.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       FrameChangesOriginType) {
  EXPECT_FALSE(frame_scheduler_->IsCrossOriginToMainFrame());
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(GetTaskType());

  // Create a new FrameScheduler that remains cross-origin with the main frame
  // throughout the test.
  std::unique_ptr<FrameSchedulerImpl> cross_origin_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(), nullptr,
                           FrameScheduler::FrameType::kSubframe);
  cross_origin_frame_scheduler->SetCrossOriginToMainFrame(true);
  const scoped_refptr<base::SingleThreadTaskRunner> cross_origin_task_runner =
      cross_origin_frame_scheduler->GetTaskRunner(GetTaskType());

  // Snap the time to a multiple of
  // |kIntensiveThrottlingDurationBetweenWakeUps|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);

  // Hide the page and wait until the intensive throttling grace period has
  // elapsed.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);
  task_environment_.FastForwardBy(kGracePeriod);

  {
    // Post delayed tasks with short delays to both frames. The
    // main-frame-origin task can run at the desired time, because there is no
    // recent wake up. The cross-origin task must run at an aligned time.
    int counter = 0;
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)),
        kDefaultThrottledWakeUpInterval);
    int cross_origin_counter = 0;
    cross_origin_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter,
                       base::Unretained(&cross_origin_counter)),
        kDefaultThrottledWakeUpInterval);

    // Make the |frame_scheduler_| cross-origin. Its task must now run at an
    // aligned time.
    frame_scheduler_->SetCrossOriginToMainFrame(true);

    task_environment_.FastForwardBy(kDefaultThrottledWakeUpInterval);
    if (IsIntensiveThrottlingExpected()) {
      EXPECT_EQ(0, counter);
      EXPECT_EQ(0, cross_origin_counter);
    } else {
      EXPECT_EQ(1, counter);
      EXPECT_EQ(1, cross_origin_counter);
    }

    FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);
    EXPECT_EQ(1, counter);
    EXPECT_EQ(1, cross_origin_counter);
  }

  {
    // Post delayed tasks with long delays that aren't aligned with the wake up
    // interval. They should run at aligned times, since they are cross-origin.
    const base::TimeDelta kLongUnalignedDelay =
        5 * kIntensiveThrottlingDurationBetweenWakeUps +
        kDefaultThrottledWakeUpInterval;
    int counter = 0;
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter, base::Unretained(&counter)),
        kLongUnalignedDelay);
    int cross_origin_counter = 0;
    cross_origin_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&IncrementCounter,
                       base::Unretained(&cross_origin_counter)),
        kLongUnalignedDelay);

    // Make the |frame_scheduler_| same-origin. Its task can now run at a
    // 1-second aligned time, since there was no wake up in the last minute.
    frame_scheduler_->SetCrossOriginToMainFrame(false);

    task_environment_.FastForwardBy(kLongUnalignedDelay);
    if (IsIntensiveThrottlingExpected()) {
      EXPECT_EQ(1, counter);
      EXPECT_EQ(0, cross_origin_counter);
    } else {
      EXPECT_EQ(1, counter);
      EXPECT_EQ(1, cross_origin_counter);
    }

    FastForwardToAlignedTime(kIntensiveThrottlingDurationBetweenWakeUps);
    EXPECT_EQ(1, counter);
    EXPECT_EQ(1, cross_origin_counter);
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllTimerTaskTypes,
    FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
    testing::Values(
        IntensiveWakeUpThrottlingTestParam{
            /* task_type=*/TaskType::kJavascriptTimerDelayedLowNesting,
            /* can_intensively_throttle_low_nesting_level=*/false,
            /* is_intensive_throttling_expected=*/false},
        IntensiveWakeUpThrottlingTestParam{
            /* task_type=*/TaskType::kJavascriptTimerDelayedLowNesting,
            /* can_intensively_throttle_low_nesting_level=*/true,
            /* is_intensive_throttling_expected=*/true},
        IntensiveWakeUpThrottlingTestParam{
            /* task_type=*/TaskType::kJavascriptTimerDelayedHighNesting,
            /* can_intensively_throttle_low_nesting_level=*/false,
            /* is_intensive_throttling_expected=*/true}),
    [](const testing::TestParamInfo<IntensiveWakeUpThrottlingTestParam>& info) {
      const std::string task_type =
          TaskTypeNames::TaskTypeToString(info.param.task_type);
      if (info.param.can_intensively_throttle_low_nesting_level)
        return task_type + "_can_intensively_throttle_low_nesting_level";
      return task_type;
    });

TEST_F(FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride,
       PolicyForceEnable) {
  SetPolicyOverride(/* enabled = */ true);
  EXPECT_TRUE(IsIntensiveWakeUpThrottlingEnabled());

  // The parameters should be the defaults.
  EXPECT_EQ(base::TimeDelta::FromSeconds(
                kIntensiveWakeUpThrottling_GracePeriodSeconds_Default),
            GetIntensiveWakeUpThrottlingGracePeriod());
  EXPECT_EQ(
      base::TimeDelta::FromSeconds(
          kIntensiveWakeUpThrottling_DurationBetweenWakeUpsSeconds_Default),
      GetIntensiveWakeUpThrottlingDurationBetweenWakeUps());
  EXPECT_EQ(
      kIntensiveWakeUpThrottling_CanIntensivelyThrottleLowNestingLevel_Default,
      CanIntensivelyThrottleLowNestingLevel());
}

TEST_F(FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride,
       PolicyForceDisable) {
  SetPolicyOverride(/* enabled = */ false);
  EXPECT_FALSE(IsIntensiveWakeUpThrottlingEnabled());
}

}  // namespace frame_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
