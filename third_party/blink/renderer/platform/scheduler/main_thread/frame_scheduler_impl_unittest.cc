// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/task/common/task_annotator.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
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
constexpr base::TimeDelta kIntensiveThrottledWakeUpInterval =
    PageSchedulerImpl::kIntensiveThrottledWakeUpInterval;
constexpr auto kShortDelay = base::Milliseconds(10);

// This is a wrapper around MainThreadSchedulerImpl::CreatePageScheduler, that
// returns the PageScheduler as a PageSchedulerImpl.
std::unique_ptr<PageSchedulerImpl> CreatePageScheduler(
    PageScheduler::Delegate* page_scheduler_delegate,
    MainThreadSchedulerImpl* scheduler,
    AgentGroupScheduler& agent_group_scheduler) {
  std::unique_ptr<PageScheduler> page_scheduler =
      agent_group_scheduler.CreatePageScheduler(page_scheduler_delegate);
  std::unique_ptr<PageSchedulerImpl> page_scheduler_impl(
      static_cast<PageSchedulerImpl*>(page_scheduler.release()));
  return page_scheduler_impl;
}

// This is a wrapper around PageSchedulerImpl::CreateFrameScheduler, that
// returns the FrameScheduler as a FrameSchedulerImpl.
std::unique_ptr<FrameSchedulerImpl> CreateFrameScheduler(
    PageSchedulerImpl* page_scheduler,
    FrameScheduler::Delegate* delegate,
    bool is_in_embedded_frame_tree,
    FrameScheduler::FrameType frame_type) {
  auto frame_scheduler = page_scheduler->CreateFrameScheduler(
      delegate, is_in_embedded_frame_tree, frame_type);
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_impl(
      static_cast<FrameSchedulerImpl*>(frame_scheduler.release()));
  return frame_scheduler_impl;
}

void RecordRunTime(std::vector<base::TimeTicks>* run_times) {
  run_times->push_back(base::TimeTicks::Now());
}

class TestObject {
 public:
  explicit TestObject(int* counter) : counter_(counter) {}

  ~TestObject() { ++(*counter_); }

 private:
  int* counter_;
};

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
    TaskType::kNetworkingUnfreezable,
    TaskType::kNetworkingControl,
    TaskType::kLowPriorityScriptExecution,
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
    TaskType::kInternalHighPriorityLocalFrame,
    TaskType::kInternalInputBlocking,
    TaskType::kWakeLock,
    TaskType::kStorage,
    TaskType::kWebGPU,
    TaskType::kInternalPostMessageForwarding,
    TaskType::kInternalNavigationCancellation};

static_assert(
    static_cast<int>(TaskType::kMaxValue) == 82,
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
  MOCK_METHOD(void,
              UpdateBackForwardCacheDisablingFeatures,
              (uint64_t,
               const BFCacheBlockingFeatureAndLocations&,
               const BFCacheBlockingFeatureAndLocations&));

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
  FrameSchedulerImplTest(
      std::vector<base::test::FeatureRef> features_to_enable,
      std::vector<base::test::FeatureRef> features_to_disable)
      : FrameSchedulerImplTest() {
    feature_list_.InitWithFeatures(features_to_enable, features_to_disable);
  }

  // Constructs a FrameSchedulerImplTest with a feature to enable, associated
  // params, and a list of features to disable.
  FrameSchedulerImplTest(
      const base::Feature& feature_to_enable,
      const base::FieldTrialParams& feature_to_enable_params,
      const std::vector<base::test::FeatureRef>& features_to_disable)
      : FrameSchedulerImplTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{feature_to_enable, feature_to_enable_params}}, features_to_disable);
  }

  ~FrameSchedulerImplTest() override = default;

  void SetUp() override {
    scheduler_ = std::make_unique<MainThreadSchedulerImpl>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, task_environment_.GetMainThreadTaskRunner(),
            task_environment_.GetMockTickClock(),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetPrioritySettings(CreatePrioritySettings())
                .Build()));
    agent_group_scheduler_ = scheduler_->CreateAgentGroupScheduler();
    page_scheduler_ =
        CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
    frame_scheduler_delegate_ = std::make_unique<
        testing::StrictMock<FrameSchedulerDelegateForTesting>>();
    frame_scheduler_ = CreateFrameScheduler(
        page_scheduler_.get(), frame_scheduler_delegate_.get(),
        /*is_in_embedded_frame_tree=*/false,
        FrameScheduler::FrameType::kSubframe);
  }

  void ResetFrameScheduler(bool is_in_embedded_frame_tree,
                           FrameScheduler::FrameType frame_type) {
    auto new_delegate_ = std::make_unique<
        testing::StrictMock<FrameSchedulerDelegateForTesting>>();
    frame_scheduler_ =
        CreateFrameScheduler(page_scheduler_.get(), new_delegate_.get(),
                             is_in_embedded_frame_tree, frame_type);
    frame_scheduler_delegate_ = std::move(new_delegate_);
  }

  void StorePageInBackForwardCache() {
    page_scheduler_->SetPageVisible(false);
    page_scheduler_->SetPageFrozen(true);
    page_scheduler_->SetPageBackForwardCached(true);
  }

  void RestorePageFromBackForwardCache() {
    page_scheduler_->SetPageVisible(true);
    page_scheduler_->SetPageFrozen(false);
    page_scheduler_->SetPageBackForwardCached(false);
  }

  void TearDown() override {
    throttleable_task_queue_.reset();
    frame_scheduler_.reset();
    page_scheduler_.reset();
    agent_group_scheduler_ = nullptr;
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

  // Helper for posting several tasks to specific queues. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each task
  // identifier specifies the task queue:
  // - 'L': Loading task queue
  // - 'T': Throttleable task queue
  // - 'P': Pausable task queue
  // - 'U': Unpausable task queue
  // - 'D': Deferrable task queue
  void PostTestTasksToQueuesWithTrait(Vector<String>* run_order,
                                      const String& task_descriptor) {
    std::istringstream stream(task_descriptor.Utf8());
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'L':
          LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'T':
          ThrottleableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'P':
          PausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'U':
          UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'D':
          DeferrableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        default:
          NOTREACHED();
      }
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

  scoped_refptr<MainThreadTaskQueue>
  JavaScriptTimerNormalThrottleableTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::ThrottleableTaskQueueTraits().SetPrioritisationType(
            PrioritisationType::kJavaScriptTimer));
  }

  scoped_refptr<MainThreadTaskQueue>
  JavaScriptTimerIntensivelyThrottleableTaskQueue() {
    return GetTaskQueue(
        FrameSchedulerImpl::ThrottleableTaskQueueTraits()
            .SetPrioritisationType(PrioritisationType::kJavaScriptTimer)
            .SetCanBeIntensivelyThrottled(true));
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

  scoped_refptr<MainThreadTaskQueue> InputBlockingTaskQueue() {
    return GetTaskQueue(FrameSchedulerImpl::InputBlockingQueueTraits());
  }

  scoped_refptr<MainThreadTaskQueue> GetTaskQueue(TaskType type) {
    return frame_scheduler_->GetTaskQueue(type);
  }

  bool IsThrottled() {
    EXPECT_TRUE(throttleable_task_queue());
    return throttleable_task_queue()->IsThrottled();
  }

  bool IsTaskTypeThrottled(TaskType task_type) {
    scoped_refptr<MainThreadTaskQueue> task_queue = GetTaskQueue(task_type);
    return task_queue->IsThrottled();
  }

  SchedulingLifecycleState CalculateLifecycleState(
      FrameScheduler::ObserverType type) {
    return frame_scheduler_->CalculateLifecycleState(type);
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
  Persistent<AgentGroupScheduler> agent_group_scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
  std::unique_ptr<testing::StrictMock<FrameSchedulerDelegateForTesting>>
      frame_scheduler_delegate_;
  scoped_refptr<MainThreadTaskQueue> throttleable_task_queue_;
};

class FrameSchedulerImplStopInBackgroundDisabledTest
    : public FrameSchedulerImplTest,
      public ::testing::WithParamInterface<TaskType> {
 public:
  FrameSchedulerImplStopInBackgroundDisabledTest()
      : FrameSchedulerImplTest({}, {blink::features::kStopInBackground}) {}
};

namespace {

class MockLifecycleObserver {
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

  void OnLifecycleStateChanged(SchedulingLifecycleState state) {
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

  FrameOrWorkerScheduler::OnLifecycleStateChangedCallback GetCallback() {
    return base::BindRepeating(&MockLifecycleObserver::OnLifecycleStateChanged,
                               base::Unretained(this));
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

  FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase()
      : FrameSchedulerImplTest({features::kIntensiveWakeUpThrottling},
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
      GetIntensiveWakeUpThrottlingGracePeriod(false);
};

// Test param for FrameSchedulerImplTestWithIntensiveWakeUpThrottling
struct IntensiveWakeUpThrottlingTestParam {
  // TaskType used to obtain TaskRunners from the FrameScheduler.
  TaskType task_type;
  // Whether it is expected that tasks will be intensively throttled.
  bool is_intensive_throttling_expected;
};

class FrameSchedulerImplTestWithIntensiveWakeUpThrottling
    : public FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase,
      public ::testing::WithParamInterface<IntensiveWakeUpThrottlingTestParam> {
 public:
  FrameSchedulerImplTestWithIntensiveWakeUpThrottling() = default;

  TaskType GetTaskType() const { return GetParam().task_type; }
  bool IsIntensiveThrottlingExpected() const {
    return GetParam().is_intensive_throttling_expected;
  }

  // Get the TaskRunner from |frame_scheduler_| using the test's task type
  // parameter.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() {
    return GetTaskRunner(frame_scheduler_.get());
  }

  // Get the TaskRunner from the provided |frame_scheduler| using the test's
  // task type parameter.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      FrameSchedulerImpl* frame_scheduler) {
    const TaskType task_type = GetTaskType();
    if (task_type == TaskType::kWebSchedulingPostedTask) {
      test_web_scheduling_task_queues_.push_back(
          frame_scheduler->CreateWebSchedulingTaskQueue(
              WebSchedulingPriority::kUserVisiblePriority));
      return test_web_scheduling_task_queues_.back()->GetTaskRunner();
    }
    return frame_scheduler->GetTaskRunner(task_type);
  }

  base::TimeDelta GetExpectedWakeUpInterval() const {
    if (IsIntensiveThrottlingExpected())
      return kIntensiveThrottledWakeUpInterval;
    return kDefaultThrottledWakeUpInterval;
  }

 private:
  // Store web scheduling task queues that are created for tests so
  // they do not get destroyed. Destroying them before their tasks finish
  // running will break throttling.
  Vector<std::unique_ptr<WebSchedulingTaskQueue>>
      test_web_scheduling_task_queues_;
};

class FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride
    : public FrameSchedulerImplTestWithIntensiveWakeUpThrottlingBase {
 public:
  FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride() = default;

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
  EXPECT_FALSE(throttleable_task_queue());
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_ExplicitInit) {
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  page_scheduler_->SetPageVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHidden_LazyInit) {
  page_scheduler_->SetPageVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, PageHiddenThenVisible_ExplicitInit) {
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
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOriginToNearestMainFrame(true);
  frame_scheduler_->SetCrossOriginToNearestMainFrame(false);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOriginToNearestMainFrame(true);
  EXPECT_TRUE(IsThrottled());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_CrossOrigin_LazyInit) {
  frame_scheduler_->SetFrameVisible(false);
  frame_scheduler_->SetCrossOriginToNearestMainFrame(true);
  LazyInitThrottleableTaskQueue();
  EXPECT_TRUE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_ExplicitInit) {
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameHidden_SameOrigin_LazyInit) {
  frame_scheduler_->SetFrameVisible(false);
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_ExplicitInit) {
  LazyInitThrottleableTaskQueue();
  EXPECT_FALSE(IsThrottled());
  EXPECT_TRUE(throttleable_task_queue());
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_FALSE(IsThrottled());
  frame_scheduler_->SetCrossOriginToNearestMainFrame(true);
  EXPECT_FALSE(IsThrottled());
}

TEST_F(FrameSchedulerImplTest, FrameVisible_CrossOrigin_LazyInit) {
  frame_scheduler_->SetFrameVisible(true);
  frame_scheduler_->SetCrossOriginToNearestMainFrame(true);
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

// Verify that tasks in a throttled task queue cause:
// - Before intensive wake up throttling kicks in: 1 wake up per second
// - After intensive wake up throttling kick in:
//    - Low nesting level: 1 wake up per second
//    - High nesting level: 1 wake up per minute
// Disable the kStopInBackground feature because it hides the effect of
// intensive wake up throttling.
// Flake test: crbug.com/1328967
TEST_P(FrameSchedulerImplStopInBackgroundDisabledTest,
       DISABLED_ThrottledTaskExecution) {
  // This TaskRunner is throttled.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(GetParam());

  // Hide the page. This enables wake up throttling.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);

  // Schedule tasks with a short delay, during the intensive wake up throttling
  // grace period.
  int num_remaining_tasks =
      base::Seconds(kIntensiveWakeUpThrottling_GracePeriodSeconds_Default)
          .IntDiv(kDefaultThrottledWakeUpInterval);
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RePostTask, task_runner, kShortDelay,
                     base::Unretained(&num_remaining_tasks)),
      kShortDelay);

  // A task should run every second.
  while (num_remaining_tasks > 0) {
    int previous_num_remaining_tasks = num_remaining_tasks;
    task_environment_.FastForwardBy(kDefaultThrottledWakeUpInterval);
    EXPECT_EQ(previous_num_remaining_tasks - 1, num_remaining_tasks);
  }

  // Schedule tasks with a short delay, after the intensive wake up throttling
  // grace period.
  num_remaining_tasks = 5;
  task_runner->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RePostTask, task_runner, kShortDelay,
                     base::Unretained(&num_remaining_tasks)),
      kShortDelay);

  // Task run every minute if the nesting level is high, or every second
  // otherwise.
  const base::TimeDelta expected_period_after_grace_period =
      (GetParam() == TaskType::kJavascriptTimerDelayedLowNesting)
          ? kDefaultThrottledWakeUpInterval
          : kIntensiveThrottledWakeUpInterval;

  while (num_remaining_tasks > 0) {
    int previous_num_remaining_tasks = num_remaining_tasks;
    task_environment_.FastForwardBy(expected_period_after_grace_period);
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

  frame_scheduler_->SetFrameVisible(false);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, counter);

  frame_scheduler_->SetFrameVisible(true);

  EXPECT_EQ(0, counter);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);
}

TEST_F(FrameSchedulerImplTest, PageFreezeAndUnfreeze) {
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
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(5, counter);
}

// Similar to PageFreezeAndUnfreeze, but unfreezes task queues by making the
// page visible instead of by invoking SetPageFrozen(false).
TEST_F(FrameSchedulerImplTest, PageFreezeAndPageVisible) {
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

  // Making the page visible should cause frozen queues to resume.
  page_scheduler_->SetPageVisible(true);

  EXPECT_EQ(1, counter);
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(5, counter);
}

TEST_F(FrameSchedulerImplTest, PagePostsCpuTasks) {
  EXPECT_TRUE(GetTaskTime().is_zero());
  EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                base::Milliseconds(10)));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetTaskTime().is_zero());
  EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());
  UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                base::Milliseconds(100)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GetTaskTime().is_zero());
  EXPECT_EQ(1, GetTotalUpdateTaskTimeCalls());
}

TEST_F(FrameSchedulerImplTest, FramePostsCpuTasksThroughReloadRenavigate) {
  const struct {
    bool embedded_frame_tree;
    FrameScheduler::FrameType frame_type;
    FrameScheduler::NavigationType navigation_type;
    bool expect_task_time_zero;
    int expected_total_calls;
  } kTestCases[] = {{false, FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kOther, false, 0},
                    {false, FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kReload, false, 0},
                    {false, FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kSameDocument, true, 1},
                    {false, FrameScheduler::FrameType::kSubframe,
                     FrameScheduler::NavigationType::kOther, true, 1},
                    {false, FrameScheduler::FrameType::kSubframe,
                     FrameScheduler::NavigationType::kSameDocument, true, 1},
                    {true, FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kOther, true, 1},
                    {true, FrameScheduler::FrameType::kMainFrame,
                     FrameScheduler::NavigationType::kSameDocument, true, 1},
                    {true, FrameScheduler::FrameType::kSubframe,
                     FrameScheduler::NavigationType::kOther, true, 1},
                    {true, FrameScheduler::FrameType::kSubframe,
                     FrameScheduler::NavigationType::kSameDocument, true, 1}};
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(String::Format(
        "FrameType: %d, NavigationType: %d : TaskTime.is_zero %d, CallCount %d",
        test_case.frame_type, test_case.navigation_type,
        test_case.expect_task_time_zero, test_case.expected_total_calls));
    ResetFrameScheduler(test_case.embedded_frame_tree, test_case.frame_type);
    EXPECT_TRUE(GetTaskTime().is_zero());
    EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());

    // Check the rest of the values after different types of commit.
    UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                  base::Milliseconds(60)));
    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(GetTaskTime().is_zero());
    EXPECT_EQ(0, GetTotalUpdateTaskTimeCalls());

    DidCommitProvisionalLoad(test_case.navigation_type);

    UnpausableTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE, base::BindOnce(&RunTaskOfLength, &task_environment_,
                                  base::Milliseconds(60)));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(test_case.expect_task_time_zero, GetTaskTime().is_zero());
    EXPECT_EQ(test_case.expected_total_calls, GetTotalUpdateTaskTimeCalls());
  }
}

class FrameSchedulerImplTestWithUnfreezableLoading
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplTestWithUnfreezableLoading()
      : FrameSchedulerImplTest({blink::features::kLoadingTasksUnfreezable},
                               {}) {
    WebRuntimeFeatures::EnableBackForwardCache(true);
  }
};

TEST_F(FrameSchedulerImplTestWithUnfreezableLoading,
       LoadingTasksKeepRunningWhenFrozen) {
  int counter = 0;
  UnfreezableLoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  LoadingTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
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

// Tests if throttling observer callbacks work.
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
      FrameScheduler::ObserverType::kLoader, observer->GetCallback());

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

  task_environment_.FastForwardBy(base::Seconds(30));

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
  task_environment_.FastForwardBy(base::Seconds(100));
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
      FrameScheduler::ObserverType::kLoader, loader_observer->GetCallback());

  auto worker_observer_handle = frame_scheduler_->AddLifecycleObserver(
      FrameScheduler::ObserverType::kWorkerScheduler,
      worker_observer->GetCallback());

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
            loader_observer_added_after_stopped->GetCallback());
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
  task_environment_.FastForwardBy(base::Hours(1));

  // Post IPC tasks, accounting for delay for when tracking starts.
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(1);
    task_runner->PostTask(FROM_HERE, base::DoNothing());
  }
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash_2(2);
    task_runner->PostTask(FROM_HERE, base::DoNothing());
  }
  // Logging is delayed by one second, so guarantee that our IPCS are logged.
  task_environment_.FastForwardBy(base::Seconds(2));
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
  task_environment_.FastForwardBy(base::Hours(1));

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
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
            {
              base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(2);
              task_runner->PostTask(FROM_HERE, base::DoNothing());
            }
          },
          task_runner));
  // Logging is delayed by one second, so guarantee that our IPCS are logged.
  task_environment_.FastForwardBy(base::Seconds(2));
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             base::RepeatingClosure restore_from_cache_callback) {
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

class InputHighPriorityFrameSchedulerImplTest : public FrameSchedulerImplTest {
 public:
  InputHighPriorityFrameSchedulerImplTest()
      : FrameSchedulerImplTest(
            {},
            {::blink::features::kInputTargetClientHighPriority}) {}
};

// TODO(farahcharab) Move priority testing to MainThreadTaskQueueTest after
// landing the change that moves priority computation to MainThreadTaskQueue.

TEST_F(InputHighPriorityFrameSchedulerImplTest,
       NormalPriorityInputBlockingTaskQueue) {
  page_scheduler_->SetPageVisible(false);
  EXPECT_EQ(InputBlockingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(InputBlockingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

TEST_F(FrameSchedulerImplTest, HighestPriorityInputBlockingTaskQueue) {
  page_scheduler_->SetPageVisible(false);
  EXPECT_EQ(InputBlockingTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighestPriority);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(InputBlockingTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighestPriority);
}

class LowPriorityBackgroundPageExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityBackgroundPageExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForBackgroundPages}, {}) {}
};

TEST_F(LowPriorityBackgroundPageExperimentTest, FrameQueuesPriorities) {
  page_scheduler_->SetPageVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(), TaskPriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
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
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);

  page_scheduler_->AudioStateChanged(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);

  page_scheduler_->AudioStateChanged(false);
  page_scheduler_->SetPageVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

class LowPriorityHiddenFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityHiddenFrameExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForHiddenFrame}, {}) {}
};

TEST_F(LowPriorityHiddenFrameExperimentTest, FrameQueuesPriorities) {
  // Hidden Frame Task Queues.
  frame_scheduler_->SetFrameVisible(false);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(), TaskPriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);

  // Visible Frame Task Queues.
  frame_scheduler_->SetFrameVisible(true);
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

class LowPrioritySubFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPrioritySubFrameExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForSubFrame}, {}) {}
};

TEST_F(LowPrioritySubFrameExperimentTest, FrameQueuesPriorities) {
  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(), TaskPriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);

  frame_scheduler_ =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

class LowPrioritySubFrameThrottleableTaskExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPrioritySubFrameThrottleableTaskExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForSubFrameThrottleableTask}, {}) {}
};

TEST_F(LowPrioritySubFrameThrottleableTaskExperimentTest,
       FrameQueuesPriorities) {
  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);

  frame_scheduler_ =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

class LowPriorityThrottleableTaskExperimentTest
    : public FrameSchedulerImplTest {
 public:
  LowPriorityThrottleableTaskExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForThrottleableTask}, {}) {}
};

TEST_F(LowPriorityThrottleableTaskExperimentTest, FrameQueuesPriorities) {
  // Sub-Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);

  frame_scheduler_ =
      CreateFrameScheduler(page_scheduler_.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kMainFrame);

  // Main Frame Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

class LowPriorityAdFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityAdFrameExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForAdFrame}, {}) {}
};

TEST_F(LowPriorityAdFrameExperimentTest, FrameQueuesPriorities) {
  EXPECT_FALSE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);

  frame_scheduler_->SetIsAdFrame(true);

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(), TaskPriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);

  frame_scheduler_->SetIsAdFrame(false);

  EXPECT_FALSE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

class BestEffortPriorityAdFrameExperimentTest : public FrameSchedulerImplTest {
 public:
  BestEffortPriorityAdFrameExperimentTest()
      : FrameSchedulerImplTest({kBestEffortPriorityForAdFrame}, {}) {}
};

TEST_F(BestEffortPriorityAdFrameExperimentTest, FrameQueuesPriorities) {
  EXPECT_FALSE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);

  frame_scheduler_->SetIsAdFrame(true);

  EXPECT_TRUE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kBestEffortPriority);

  frame_scheduler_->SetIsAdFrame(false);
  EXPECT_FALSE(frame_scheduler_->IsAdFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
}

class LowPriorityCrossOriginTaskExperimentTest : public FrameSchedulerImplTest {
 public:
  LowPriorityCrossOriginTaskExperimentTest()
      : FrameSchedulerImplTest({kLowPriorityForCrossOrigin}, {}) {}
};

TEST_F(LowPriorityCrossOriginTaskExperimentTest, FrameQueuesPriorities) {
  EXPECT_FALSE(frame_scheduler_->IsCrossOriginToNearestMainFrame());

  // Same Origin Task Queues.
  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kHighPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kNormalPriority);

  frame_scheduler_->SetCrossOriginToNearestMainFrame(true);
  EXPECT_TRUE(frame_scheduler_->IsCrossOriginToNearestMainFrame());

  EXPECT_EQ(LoadingTaskQueue()->GetQueuePriority(), TaskPriority::kLowPriority);
  EXPECT_EQ(LoadingControlTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(DeferrableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(ThrottleableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(PausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
  EXPECT_EQ(UnpausableTaskQueue()->GetQueuePriority(),
            TaskPriority::kLowPriority);
}

TEST_F(FrameSchedulerImplTest, TaskTypeToTaskQueueMapping) {
  // Make sure the queue lookup and task type to queue traits map works as
  // expected. This test will fail if these task types are moved to different
  // default queues.
  EXPECT_EQ(GetTaskQueue(TaskType::kJavascriptTimerDelayedLowNesting),
            JavaScriptTimerNormalThrottleableTaskQueue());
  EXPECT_EQ(GetTaskQueue(TaskType::kJavascriptTimerDelayedHighNesting),
            JavaScriptTimerIntensivelyThrottleableTaskQueue());
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
  EXPECT_EQ(da_queue->GetQueuePriority(), TaskPriority::kNormalPriority);
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
  EXPECT_EQ(da_queue->GetQueuePriority(), TaskPriority::kHighPriority);
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

  EXPECT_EQ(TaskPriority::kBestEffortPriority, task_queue->GetQueuePriority());
}

TEST_F(FrameSchedulerImplTest, ComputePriorityForDetachedFrame) {
  auto task_queue = GetTaskQueue(TaskType::kJavascriptTimerDelayedLowNesting);
  // Just check that it does not crash.
  page_scheduler_.reset();
  frame_scheduler_->ComputePriority(task_queue.get());
}

TEST_F(FrameSchedulerImplTest,
       LowPriorityScriptExecutionHasBestEffortPriority) {
  auto task_queue = GetTaskQueue(TaskType::kLowPriorityScriptExecution);

  EXPECT_EQ(TaskPriority::kBestEffortPriority, task_queue->GetQueuePriority());
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
      {SchedulingPolicy::DisableBackForwardCache()});

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  auto feature_handle2 = frame_scheduler_->RegisterFeature(
      SchedulingPolicy::Feature::kWebRTC,
      {SchedulingPolicy::DisableBackForwardCache()});

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
      {SchedulingPolicy::DisableBackForwardCache()});

  EXPECT_THAT(
      frame_scheduler_->GetActiveFeaturesTrackedForBackForwardCacheMetrics(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_EQ(ComputeMaskFromFeatures(frame_scheduler_.get()),
            GetActiveFeaturesTrackedForBackForwardCacheMetricsMask(
                frame_scheduler_.get()));

  frame_scheduler_->RegisterStickyFeature(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
      {SchedulingPolicy::DisableBackForwardCache()});

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
  ResetFrameScheduler(/*is_in_embedded_frame_tree=*/false,
                      FrameScheduler::FrameType::kMainFrame);

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
                    {SchedulingPolicy::DisableBackForwardCache()});
                frame_scheduler->RegisterStickyFeature(
                    SchedulingPolicy::Feature::
                        kMainResourceHasCacheControlNoCache,
                    {SchedulingPolicy::DisableBackForwardCache()});
                // Ensure that the feature upload is delayed.
                testing::Mock::VerifyAndClearExpectations(delegate);
                EXPECT_CALL(
                    *delegate,
                    UpdateBackForwardCacheDisablingFeatures(
                        (1 << static_cast<size_t>(
                             SchedulingPolicy::Feature::
                                 kMainResourceHasCacheControlNoStore)) |
                            (1 << static_cast<size_t>(
                                 SchedulingPolicy::Feature::
                                     kMainResourceHasCacheControlNoCache)),
                        BFCacheBlockingFeatureAndLocations(),
                        BFCacheBlockingFeatureAndLocations(
                            {FeatureAndJSLocationBlockingBFCache(
                                 SchedulingPolicy::Feature::
                                     kMainResourceHasCacheControlNoStore,
                                 nullptr),
                             FeatureAndJSLocationBlockingBFCache(
                                 SchedulingPolicy::Feature::
                                     kMainResourceHasCacheControlNoCache,
                                 nullptr)})));
              },
              frame_scheduler_.get(), frame_scheduler_delegate_.get()));

  base::RunLoop().RunUntilIdle();

  testing::Mock::VerifyAndClearExpectations(frame_scheduler_delegate_.get());
}

TEST_F(FrameSchedulerImplTest, FeatureUpload_FrameDestruction) {
  ResetFrameScheduler(/*is_in_embedded_frame_tree=*/false,
                      FrameScheduler::FrameType::kMainFrame);

  FeatureHandle feature_handle(frame_scheduler_->RegisterFeature(
      SchedulingPolicy::Feature::kWebSocket,
      {SchedulingPolicy::DisableBackForwardCache()}));

  frame_scheduler_->GetTaskRunner(TaskType::kJavascriptTimerImmediate)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](FrameSchedulerImpl* frame_scheduler,
                 testing::StrictMock<FrameSchedulerDelegateForTesting>*
                     delegate,
                 FeatureHandle* feature_handle) {
                // Ensure that the feature upload is delayed.
                testing::Mock::VerifyAndClearExpectations(delegate);
                EXPECT_CALL(
                    *delegate,
                    UpdateBackForwardCacheDisablingFeatures(
                        (1 << static_cast<size_t>(
                             SchedulingPolicy::Feature::kWebSocket)),
                        BFCacheBlockingFeatureAndLocations(
                            {feature_handle
                                 ->GetFeatureAndJSLocationBlockingBFCache()}),
                        BFCacheBlockingFeatureAndLocations()));
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
                           UpdateBackForwardCacheDisablingFeatures(
                               testing::_, BFCacheBlockingFeatureAndLocations(),
                               BFCacheBlockingFeatureAndLocations()))
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
                                  const String& task_descriptor,
                                  base::TimeDelta delay = base::TimeDelta()) {
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
      web_scheduling_task_runners_[static_cast<int>(priority)]->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&AppendToVectorTestTask, run_order,
                         String::FromUTF8(task)),
          delay);
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

TEST_F(WebSchedulingTaskQueueTest, DynamicTaskPriorityOrderDelayedTasks) {
  Vector<String> run_order;

  PostWebSchedulingTestTasks(&run_order, "U1 U2 V1 V2", base::Milliseconds(5));
  task_queues_[static_cast<int>(WebSchedulingPriority::kUserBlockingPriority)]
      ->SetPriority(WebSchedulingPriority::kBackgroundPriority);

  task_environment_.FastForwardBy(base::Milliseconds(5));

  EXPECT_THAT(run_order, testing::ElementsAre("V1", "V2", "U1", "U2"));
}

// Verify that tasks posted with TaskType::kJavascriptTimerDelayed* and
// delayed web scheduling tasks run at the expected time when throttled.
TEST_F(FrameSchedulerImplTest, ThrottledJSTimerTasksRunTime) {
  constexpr TaskType kJavaScriptTimerTaskTypes[] = {
      TaskType::kJavascriptTimerDelayedLowNesting,
      TaskType::kJavascriptTimerDelayedHighNesting,
      TaskType::kWebSchedulingPostedTask};

  // Snap the time to a multiple of 1 second. Otherwise, the exact run time
  // of throttled tasks after hiding the page will vary.
  FastForwardToAlignedTime(base::Seconds(1));
  const base::TimeTicks start = base::TimeTicks::Now();

  // Hide the page to start throttling JS Timers.
  page_scheduler_->SetPageVisible(false);

  std::map<TaskType, std::vector<base::TimeTicks>> run_times;

  // Create the web scheduler task queue outside of the scope of the for loop.
  // This is necessary because otherwise the queue is deleted before tasks run,
  // and this breaks throttling.
  std::unique_ptr<WebSchedulingTaskQueue> web_scheduling_task_queue =
      frame_scheduler_->CreateWebSchedulingTaskQueue(
          WebSchedulingPriority::kUserVisiblePriority);

  // Post tasks with each Javascript Timer Task Type and with a
  // WebSchedulingTaskQueue.
  for (TaskType task_type : kJavaScriptTimerTaskTypes) {
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
        task_type == TaskType::kWebSchedulingPostedTask
            ? web_scheduling_task_queue->GetTaskRunner()
            : frame_scheduler_->GetTaskRunner(task_type);

    // Note: Taking the address of an element in |run_times| is safe because
    // inserting elements in a map does not invalidate references.

    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::Milliseconds(1000));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::Milliseconds(1002));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::Milliseconds(1004));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::Milliseconds(2500));
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times[task_type]),
        base::Milliseconds(6000));
  }

  // Make posted tasks run.
  task_environment_.FastForwardBy(base::Hours(1));

  // The effective delay of a throttled task is >= the requested delay, and is
  // within [N * 1000, N * 1000 + 3] ms, where N is an integer. This is because
  // the wake up rate is 1 per second, and the duration of each wake up is 3 ms.
  for (TaskType task_type : kJavaScriptTimerTaskTypes) {
    EXPECT_THAT(run_times[task_type],
                testing::ElementsAre(start + base::Milliseconds(0),
                                     start + base::Milliseconds(1000),
                                     start + base::Milliseconds(1002),
                                     start + base::Milliseconds(2000),
                                     start + base::Milliseconds(3000),
                                     start + base::Milliseconds(6000)));
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
                task_environment.GetMockTickClock(),
                base::sequence_manager::SequenceManager::Settings::Builder()
                    .SetPrioritySettings(CreatePrioritySettings())
                    .Build())) {}

  MOCK_METHOD(void, OnMainFramePaint, ());
};
}  // namespace

TEST_F(FrameSchedulerImplTest, ReportFMPAndFCPForMainFrames) {
  MockMainThreadScheduler mock_main_thread_scheduler{task_environment_};
  AgentGroupScheduler* agent_group_scheduler =
      mock_main_thread_scheduler.CreateAgentGroupScheduler();
  std::unique_ptr<PageSchedulerImpl> page_scheduler = CreatePageScheduler(
      nullptr, &mock_main_thread_scheduler, *agent_group_scheduler);

  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler =
      CreateFrameScheduler(page_scheduler.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kMainFrame);

  EXPECT_CALL(mock_main_thread_scheduler, OnMainFramePaint).Times(2);

  main_frame_scheduler->OnFirstMeaningfulPaint();
  main_frame_scheduler->OnFirstContentfulPaintInMainFrame();

  main_frame_scheduler = nullptr;
  page_scheduler = nullptr;
  agent_group_scheduler = nullptr;
  mock_main_thread_scheduler.Shutdown();
}

TEST_F(FrameSchedulerImplTest, DontReportFMPAndFCPForSubframes) {
  MockMainThreadScheduler mock_main_thread_scheduler{task_environment_};
  AgentGroupScheduler* agent_group_scheduler =
      mock_main_thread_scheduler.CreateAgentGroupScheduler();
  std::unique_ptr<PageSchedulerImpl> page_scheduler = CreatePageScheduler(
      nullptr, &mock_main_thread_scheduler, *agent_group_scheduler);

  // Test for direct subframes.
  {
    std::unique_ptr<FrameSchedulerImpl> subframe_scheduler =
        CreateFrameScheduler(page_scheduler.get(), nullptr,
                             /*is_in_embedded_frame_tree=*/false,
                             FrameScheduler::FrameType::kSubframe);

    EXPECT_CALL(mock_main_thread_scheduler, OnMainFramePaint).Times(0);

    subframe_scheduler->OnFirstMeaningfulPaint();
  }

  // Now test for embedded main frames.
  {
    std::unique_ptr<FrameSchedulerImpl> subframe_scheduler =
        CreateFrameScheduler(page_scheduler.get(), nullptr,
                             /*is_in_embedded_frame_tree=*/true,
                             FrameScheduler::FrameType::kMainFrame);

    EXPECT_CALL(mock_main_thread_scheduler, OnMainFramePaint).Times(0);

    subframe_scheduler->OnFirstMeaningfulPaint();
  }

  page_scheduler = nullptr;
  agent_group_scheduler = nullptr;
  mock_main_thread_scheduler.Shutdown();
}

// Verify that tasks run at the expected time in frame that is same-origin with
// the main frame with intensive wake up throttling.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       TaskExecutionSameOriginFrame) {
  ASSERT_FALSE(frame_scheduler_->IsCrossOriginToNearestMainFrame());

  // Throttled TaskRunner to which tasks are posted in this test.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner();

  // Snap the time to a multiple of
  // |kIntensiveThrottledWakeUpInterval|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);
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
    EXPECT_THAT(run_times,
                testing::ElementsAre(
                    scope_start + kDefaultThrottledWakeUpInterval,
                    scope_start + 2 * kDefaultThrottledWakeUpInterval,
                    scope_start + 3 * kDefaultThrottledWakeUpInterval,
                    scope_start + 4 * kDefaultThrottledWakeUpInterval,
                    scope_start + 5 * kDefaultThrottledWakeUpInterval));
  }

  // After the grace period:

  // Test that wake ups are 1-second aligned if there is no recent wake up.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::Minutes(5));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kDefaultThrottledWakeUpInterval);
    EXPECT_THAT(run_times,
                testing::ElementsAre(scope_start + base::Seconds(1)));
  }

  // Test that if there is a recent wake up:
  //   TaskType can be intensively throttled:   Wake ups are 1-minute aligned
  //   Otherwise:                               Wake ups are 1-second aligned
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::Minutes(5) + base::Seconds(1));
    std::vector<base::TimeTicks> run_times;

    for (int i = 0; i < kNumTasks; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kShortDelay + i * kDefaultThrottledWakeUpInterval);
    }

    FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);

    if (IsIntensiveThrottlingExpected()) {
      const base::TimeTicks aligned_time = scope_start + base::Seconds(59);
      EXPECT_THAT(run_times,
                  testing::ElementsAre(aligned_time, aligned_time, aligned_time,
                                       aligned_time, aligned_time));
    } else {
      EXPECT_THAT(run_times,
                  testing::ElementsAre(scope_start + base::Seconds(1),
                                       scope_start + base::Seconds(2),
                                       scope_start + base::Seconds(3),
                                       scope_start + base::Seconds(4),
                                       scope_start + base::Seconds(5)));
    }
  }

  // Post an extra task with a short delay. The wake up should be 1-minute
  // aligned if the TaskType supports intensive throttling, or 1-second aligned
  // otherwise.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::Minutes(6));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kIntensiveThrottledWakeUpInterval);

    EXPECT_THAT(run_times, testing::ElementsAre(scope_start +
                                                GetExpectedWakeUpInterval()));
  }

  // Post an extra task with a delay longer than the intensive throttling wake
  // up interval. The wake up should be 1-second aligned, even if the TaskType
  // supports intensive throttling, because there was no wake up in the last
  // minute.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::Minutes(7));
    std::vector<base::TimeTicks> run_times;

    const base::TimeDelta kLongDelay =
        kIntensiveThrottledWakeUpInterval * 5 + kDefaultThrottledWakeUpInterval;
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
    EXPECT_EQ(scope_start,
              test_start + base::Minutes(12) + kDefaultThrottledWakeUpInterval);
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
        }),
        kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardUntilNoTasksRemain();

    if (IsIntensiveThrottlingExpected()) {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(
              scope_start + base::Seconds(1), scope_start + base::Seconds(2),
              scope_start + base::Seconds(3),
              scope_start - kDefaultThrottledWakeUpInterval + base::Minutes(1),
              scope_start - kDefaultThrottledWakeUpInterval + base::Minutes(1),
              scope_start - kDefaultThrottledWakeUpInterval +
                  base::Minutes(1)));
    } else {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(
              scope_start + base::Seconds(1), scope_start + base::Seconds(2),
              scope_start + base::Seconds(3), scope_start + base::Seconds(4),
              scope_start + base::Seconds(5), scope_start + base::Seconds(6)));
    }
  }
}

// Verify that tasks run at the expected time in a frame that is cross-origin
// with the main frame with intensive wake up throttling.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       TaskExecutionCrossOriginFrame) {
  frame_scheduler_->SetCrossOriginToNearestMainFrame(true);

  // Throttled TaskRunner to which tasks are posted in this test.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner();

  // Snap the time to a multiple of
  // |kIntensiveThrottledWakeUpInterval|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);
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
    EXPECT_THAT(run_times,
                testing::ElementsAre(scope_start + base::Seconds(1),
                                     scope_start + base::Seconds(2),
                                     scope_start + base::Seconds(3),
                                     scope_start + base::Seconds(4),
                                     scope_start + base::Seconds(5)));
  }

  // After the grace period:

  // Test posting a task when there is no recent wake up. The wake up should be
  // 1-minute aligned if the TaskType supports intensive throttling (in a main
  // frame, it would have been 1-second aligned since there was no wake up in
  // the last minute). Otherwise, it should be 1-second aligned.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::Minutes(5));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kIntensiveThrottledWakeUpInterval);
    EXPECT_THAT(run_times, testing::ElementsAre(scope_start +
                                                GetExpectedWakeUpInterval()));
  }

  // Test posting many tasks with short delays. Wake ups should be 1-minute
  // aligned if the TaskType supports intensive throttling, or 1-second aligned
  // otherwise.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::Minutes(6));
    std::vector<base::TimeTicks> run_times;

    for (int i = 0; i < kNumTasks; ++i) {
      task_runner->PostDelayedTask(
          FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
          kShortDelay + i * kDefaultThrottledWakeUpInterval);
    }

    task_environment_.FastForwardBy(kIntensiveThrottledWakeUpInterval);

    if (IsIntensiveThrottlingExpected()) {
      const base::TimeTicks aligned_time =
          scope_start + kIntensiveThrottledWakeUpInterval;
      EXPECT_THAT(run_times,
                  testing::ElementsAre(aligned_time, aligned_time, aligned_time,
                                       aligned_time, aligned_time));
    } else {
      EXPECT_THAT(run_times,
                  testing::ElementsAre(scope_start + base::Seconds(1),
                                       scope_start + base::Seconds(2),
                                       scope_start + base::Seconds(3),
                                       scope_start + base::Seconds(4),
                                       scope_start + base::Seconds(5)));
    }
  }

  // Post an extra task with a short delay. Wake ups should be 1-minute aligned
  // if the TaskType supports intensive throttling, or 1-second aligned
  // otherwise.
  {
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    EXPECT_EQ(scope_start, test_start + base::Minutes(7));
    std::vector<base::TimeTicks> run_times;

    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval);

    task_environment_.FastForwardBy(kIntensiveThrottledWakeUpInterval);
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
    EXPECT_EQ(scope_start, test_start + base::Minutes(8));
    std::vector<base::TimeTicks> run_times;

    const base::TimeDelta kLongDelay = kIntensiveThrottledWakeUpInterval * 6;
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
    EXPECT_EQ(scope_start, test_start + base::Minutes(14));
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
          testing::ElementsAre(
              scope_start + base::Minutes(1), scope_start + base::Minutes(2),
              scope_start + base::Minutes(2), scope_start + base::Minutes(2),
              scope_start + base::Minutes(2), scope_start + base::Minutes(2)));
    } else {
      EXPECT_THAT(
          run_times,
          testing::ElementsAre(
              scope_start + base::Seconds(1), scope_start + base::Seconds(2),
              scope_start + base::Seconds(3), scope_start + base::Seconds(4),
              scope_start + base::Seconds(5), scope_start + base::Seconds(6)));
    }
  }
}

// Verify that tasks from different frames that are same-origin with the main
// frame run at the expected time.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       ManySameOriginFrames) {
  ASSERT_FALSE(frame_scheduler_->IsCrossOriginToNearestMainFrame());
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner();

  // Create a FrameScheduler that is same-origin with the main frame, and an
  // associated throttled TaskRunner.
  std::unique_ptr<FrameSchedulerImpl> other_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(),
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);
  ASSERT_FALSE(other_frame_scheduler->IsCrossOriginToNearestMainFrame());
  const scoped_refptr<base::SingleThreadTaskRunner> other_task_runner =
      GetTaskRunner(other_frame_scheduler.get());

  // Snap the time to a multiple of
  // |kIntensiveThrottledWakeUpInterval|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);

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
    EXPECT_THAT(run_times, testing::ElementsAre(
                               post_time + 2 * kDefaultThrottledWakeUpInterval,
                               post_time + kIntensiveThrottledWakeUpInterval));
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
      GetTaskRunner();
  // |other_task_runner| is throttled. It belongs to a different frame on the
  // same page.
  const auto other_frame_scheduler = CreateFrameScheduler(
      page_scheduler_.get(), frame_scheduler_delegate_.get(),
      /*is_in_embedded_frame_tree=*/false,
      FrameScheduler::FrameType::kSubframe);
  const scoped_refptr<base::SingleThreadTaskRunner> other_task_runner =
      GetTaskRunner(other_frame_scheduler.get());

  // Fast-forward the time to a multiple of
  // |kIntensiveThrottledWakeUpInterval|. Otherwise,
  // the time at which tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);

  // Hide the page and wait until the intensive throttling grace period has
  // elapsed.
  EXPECT_TRUE(page_scheduler_->IsPageVisible());
  page_scheduler_->SetPageVisible(false);
  task_environment_.FastForwardBy(kGracePeriod);

  {
    // Wake ups are intensively throttled, since there is no throttling opt-out.
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    std::vector<base::TimeTicks> run_times;
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times), kShortDelay);
    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval + kShortDelay);
    task_environment_.FastForwardUntilNoTasksRemain();
    if (IsIntensiveThrottlingExpected()) {
      // Note: Intensive throttling is not applied on the 1st task since there
      // is no recent wake up.
      EXPECT_THAT(run_times,
                  testing::ElementsAre(
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + kIntensiveThrottledWakeUpInterval));
    } else {
      EXPECT_THAT(run_times,
                  testing::ElementsAre(
                      scope_start + kDefaultThrottledWakeUpInterval,
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
  // |kIntensiveThrottledWakeUpInterval| to simplify expectations.
  task_environment_.FastForwardBy(kIntensiveThrottledWakeUpInterval);
  FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);

  {
    // Wake ups are intensively throttled, since there is no throttling opt-out.
    const base::TimeTicks scope_start = base::TimeTicks::Now();
    std::vector<base::TimeTicks> run_times;
    task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times), kShortDelay);
    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 kDefaultThrottledWakeUpInterval + kShortDelay);
    task_environment_.FastForwardUntilNoTasksRemain();
    if (IsIntensiveThrottlingExpected()) {
      // Note: Intensive throttling is not applied on the 1st task since there
      // is no recent wake up.
      EXPECT_THAT(run_times,
                  testing::ElementsAre(
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + kIntensiveThrottledWakeUpInterval));
    } else {
      EXPECT_THAT(run_times,
                  testing::ElementsAre(
                      scope_start + kDefaultThrottledWakeUpInterval,
                      scope_start + 2 * kDefaultThrottledWakeUpInterval));
    }
  }
}

// Verify that tasks run at the same time when a frame switches between being
// same-origin and cross-origin with the main frame.
TEST_P(FrameSchedulerImplTestWithIntensiveWakeUpThrottling,
       FrameChangesOriginType) {
  EXPECT_FALSE(frame_scheduler_->IsCrossOriginToNearestMainFrame());
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      GetTaskRunner();

  // Create a new FrameScheduler that remains cross-origin with the main frame
  // throughout the test.
  std::unique_ptr<FrameSchedulerImpl> cross_origin_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(),
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);
  cross_origin_frame_scheduler->SetCrossOriginToNearestMainFrame(true);
  const scoped_refptr<base::SingleThreadTaskRunner> cross_origin_task_runner =
      GetTaskRunner(cross_origin_frame_scheduler.get());

  // Snap the time to a multiple of
  // |kIntensiveThrottledWakeUpInterval|. Otherwise, the time at which
  // tasks can run after throttling is enabled will vary.
  FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);

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
    frame_scheduler_->SetCrossOriginToNearestMainFrame(true);

    task_environment_.FastForwardBy(kDefaultThrottledWakeUpInterval);
    if (IsIntensiveThrottlingExpected()) {
      EXPECT_EQ(0, counter);
      EXPECT_EQ(0, cross_origin_counter);
    } else {
      EXPECT_EQ(1, counter);
      EXPECT_EQ(1, cross_origin_counter);
    }

    FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);
    EXPECT_EQ(1, counter);
    EXPECT_EQ(1, cross_origin_counter);
  }

  {
    // Post delayed tasks with long delays that aren't aligned with the wake up
    // interval. They should run at aligned times, since they are cross-origin.
    const base::TimeDelta kLongUnalignedDelay =
        5 * kIntensiveThrottledWakeUpInterval + kDefaultThrottledWakeUpInterval;
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
    frame_scheduler_->SetCrossOriginToNearestMainFrame(false);

    task_environment_.FastForwardBy(kLongUnalignedDelay);
    if (IsIntensiveThrottlingExpected()) {
      EXPECT_EQ(1, counter);
      EXPECT_EQ(0, cross_origin_counter);
    } else {
      EXPECT_EQ(1, counter);
      EXPECT_EQ(1, cross_origin_counter);
    }

    FastForwardToAlignedTime(kIntensiveThrottledWakeUpInterval);
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
            /* is_intensive_throttling_expected=*/false},
        IntensiveWakeUpThrottlingTestParam{
            /* task_type=*/TaskType::kJavascriptTimerDelayedHighNesting,
            /* is_intensive_throttling_expected=*/true},
        IntensiveWakeUpThrottlingTestParam{
            /* task_type=*/TaskType::kWebSchedulingPostedTask,
            /* is_intensive_throttling_expected=*/true}),
    [](const testing::TestParamInfo<IntensiveWakeUpThrottlingTestParam>& info) {
      return TaskTypeNames::TaskTypeToString(info.param.task_type);
    });

TEST_F(FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride,
       PolicyForceEnable) {
  SetPolicyOverride(/* enabled = */ true);
  EXPECT_TRUE(IsIntensiveWakeUpThrottlingEnabled());

  // The parameters should be the defaults.
  EXPECT_EQ(
      base::Seconds(kIntensiveWakeUpThrottling_GracePeriodSeconds_Default),
      GetIntensiveWakeUpThrottlingGracePeriod(false));
}

TEST_F(FrameSchedulerImplTestWithIntensiveWakeUpThrottlingPolicyOverride,
       PolicyForceDisable) {
  SetPolicyOverride(/* enabled = */ false);
  EXPECT_FALSE(IsIntensiveWakeUpThrottlingEnabled());
}

class FrameSchedulerImplTestQuickIntensiveWakeUpThrottlingEnabled
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplTestQuickIntensiveWakeUpThrottlingEnabled()
      : FrameSchedulerImplTest(
            {features::kQuickIntensiveWakeUpThrottlingAfterLoading},
            {}) {}
};

TEST_F(FrameSchedulerImplTestQuickIntensiveWakeUpThrottlingEnabled,
       LoadingPageGracePeriod) {
  EXPECT_EQ(
      base::Seconds(kIntensiveWakeUpThrottling_GracePeriodSeconds_Default),
      GetIntensiveWakeUpThrottlingGracePeriod(true));
}

TEST_F(FrameSchedulerImplTestQuickIntensiveWakeUpThrottlingEnabled,
       LoadedPageGracePeriod) {
  EXPECT_EQ(base::Seconds(
                kIntensiveWakeUpThrottling_GracePeriodSecondsLoaded_Default),
            GetIntensiveWakeUpThrottlingGracePeriod(false));
}

// Verify that non-delayed kWebSchedulingPostedTask tasks are not throttled.
TEST_F(FrameSchedulerImplTest, ImmediateWebSchedulingTasksAreNotThrottled) {
  std::vector<base::TimeTicks> run_times;

  // Make sure we are *not* aligned to a 1 second boundary by aligning to a 1
  // second boundary and moving past it a bit. If we were throttled, even
  // non-delayed tasks will need to wait until the next aligned interval to run.
  FastForwardToAlignedTime(base::Seconds(1));
  task_environment_.FastForwardBy(base::Milliseconds(1));

  const base::TimeTicks start = base::TimeTicks::Now();

  // Hide the page to start throttling timers.
  page_scheduler_->SetPageVisible(false);

  // Post a non-delayed task to a web scheduling task queue.
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_
          ->CreateWebSchedulingTaskQueue(
              WebSchedulingPriority::kUserVisiblePriority)
          ->GetTaskRunner();
  task_runner->PostTask(FROM_HERE, base::BindOnce(&RecordRunTime, &run_times));

  // Run any ready tasks, which includes our non-delayed non-throttled web
  // scheduling task. If we are throttled, our task will not run.
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_times, testing::ElementsAre(start));
}

class FrameSchedulerImplThrottleForegroundTimersEnabledTest
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplThrottleForegroundTimersEnabledTest()
      : FrameSchedulerImplTest({features::kThrottleForegroundTimers}, {}) {}
};

TEST_F(FrameSchedulerImplThrottleForegroundTimersEnabledTest,
       ForegroundPageTimerThrottling) {
  page_scheduler_->SetPageVisible(true);

  // Snap the time to a multiple of 32ms.
  FastForwardToAlignedTime(base::Milliseconds(32));
  const base::TimeTicks start = base::TimeTicks::Now();

  std::vector<base::TimeTicks> run_times;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(
          TaskType::kJavascriptTimerDelayedLowNesting);

  for (int i = 0; i < 5; i++) {
    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 base::Milliseconds(50) * i);
  }

  // Make posted tasks run.
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_THAT(run_times,
              testing::ElementsAre(start, start + base::Milliseconds(64),
                                   start + base::Milliseconds(128),
                                   start + base::Milliseconds(160),
                                   start + base::Milliseconds(224)));
}

// Make sure the normal throttling (1 wake up per second) is applied when the
// page becomes non-visible
TEST_F(FrameSchedulerImplThrottleForegroundTimersEnabledTest,
       BackgroundPageTimerThrottling) {
  page_scheduler_->SetPageVisible(false);

  // Snap the time to a multiple of 1 second.
  FastForwardToAlignedTime(base::Seconds(1));
  const base::TimeTicks start = base::TimeTicks::Now();

  std::vector<base::TimeTicks> run_times;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(
          TaskType::kJavascriptTimerDelayedLowNesting);

  for (int i = 0; i < 5; i++) {
    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 base::Milliseconds(50) * i);
  }

  // Make posted tasks run.
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_THAT(run_times,
              testing::ElementsAre(start, start + base::Milliseconds(1000),
                                   start + base::Milliseconds(1000),
                                   start + base::Milliseconds(1000),
                                   start + base::Milliseconds(1000)));
}

TEST_F(FrameSchedulerImplThrottleForegroundTimersEnabledTest,
       HiddenAudiblePageTimerThrottling) {
  page_scheduler_->SetPageVisible(false);
  page_scheduler_->AudioStateChanged(/*is_audio_playing=*/true);

  // Snap the time to a multiple of 32ms.
  FastForwardToAlignedTime(base::Milliseconds(32));
  const base::TimeTicks start = base::TimeTicks::Now();

  std::vector<base::TimeTicks> run_times;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(
          TaskType::kJavascriptTimerDelayedLowNesting);

  for (int i = 0; i < 5; i++) {
    task_runner->PostDelayedTask(FROM_HERE,
                                 base::BindOnce(&RecordRunTime, &run_times),
                                 base::Milliseconds(50) * i);
  }

  // Make posted tasks run.
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_THAT(run_times,
              testing::ElementsAre(start, start + base::Milliseconds(64),
                                   start + base::Milliseconds(128),
                                   start + base::Milliseconds(160),
                                   start + base::Milliseconds(224)));
}

TEST_F(FrameSchedulerImplThrottleForegroundTimersEnabledTest,
       VisibleCrossOriginFrameThrottling) {
  std::unique_ptr<FrameSchedulerImpl> cross_origin_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(),
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);
  page_scheduler_->SetPageVisible(true);
  cross_origin_frame_scheduler->SetCrossOriginToNearestMainFrame(true);
  const scoped_refptr<base::SingleThreadTaskRunner> cross_origin_task_runner =
      cross_origin_frame_scheduler->GetTaskRunner(
          TaskType::kJavascriptTimerDelayedLowNesting);

  cross_origin_frame_scheduler->SetFrameVisible(true);
  // Snap the time to a multiple of 32ms.
  FastForwardToAlignedTime(base::Milliseconds(32));
  const base::TimeTicks start = base::TimeTicks::Now();

  std::vector<base::TimeTicks> run_times;
  for (int i = 0; i < 5; i++) {
    cross_origin_task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
        base::Milliseconds(50) * i);
  }

  // Make posted tasks run.
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_THAT(run_times,
              testing::ElementsAre(start, start + base::Milliseconds(64),
                                   start + base::Milliseconds(128),
                                   start + base::Milliseconds(160),
                                   start + base::Milliseconds(224)));
}

TEST_F(FrameSchedulerImplThrottleForegroundTimersEnabledTest,
       HiddenCrossOriginFrameThrottling) {
  std::unique_ptr<FrameSchedulerImpl> cross_origin_frame_scheduler =
      CreateFrameScheduler(page_scheduler_.get(),
                           frame_scheduler_delegate_.get(),
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);
  page_scheduler_->SetPageVisible(true);
  cross_origin_frame_scheduler->SetCrossOriginToNearestMainFrame(true);
  const scoped_refptr<base::SingleThreadTaskRunner> cross_origin_task_runner =
      cross_origin_frame_scheduler->GetTaskRunner(
          TaskType::kJavascriptTimerDelayedLowNesting);

  cross_origin_frame_scheduler->SetFrameVisible(false);
  // Snap the time to a multiple of 1 second.
  FastForwardToAlignedTime(base::Seconds(1));
  const base::TimeTicks start = base::TimeTicks::Now();

  std::vector<base::TimeTicks> run_times;
  for (int i = 0; i < 5; i++) {
    cross_origin_task_runner->PostDelayedTask(
        FROM_HERE, base::BindOnce(&RecordRunTime, &run_times),
        base::Milliseconds(50) * i);
  }

  // Make posted tasks run.
  task_environment_.FastForwardBy(base::Seconds(5));

  EXPECT_THAT(run_times,
              testing::ElementsAre(start, start + base::Milliseconds(1000),
                                   start + base::Milliseconds(1000),
                                   start + base::Milliseconds(1000),
                                   start + base::Milliseconds(1000)));
}

TEST_F(FrameSchedulerImplTest, PostMessageForwardingHasVeryHighPriority) {
  auto task_queue = GetTaskQueue(TaskType::kInternalPostMessageForwarding);

  EXPECT_EQ(TaskPriority::kVeryHighPriority, task_queue->GetQueuePriority());
}

class FrameSchedulerImplDeleterTaskRunnerEnabledTest
    : public FrameSchedulerImplTest {
 public:
  FrameSchedulerImplDeleterTaskRunnerEnabledTest()
      : FrameSchedulerImplTest(
            {blink::features::kUseBlinkSchedulerTaskRunnerWithCustomDeleter},
            {}) {}
};

TEST_F(FrameSchedulerImplDeleterTaskRunnerEnabledTest,
       DeleteSoonUsesBackupTaskRunner) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(TaskType::kInternalTest);
  int counter = 0;
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  frame_scheduler_.reset();

  EXPECT_EQ(0, counter);
  // Because of graceful shutdown, the increment task should run since it was
  // queued. Since it's empty after the task finishes, the queue should then be
  // shut down.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, counter);

  std::unique_ptr<TestObject> test_object =
      std::make_unique<TestObject>(&counter);
  task_runner->DeleteSoon(FROM_HERE, std::move(test_object));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, counter);
}

enum class DeleterTaskRunnerEnabled { kEnabled, kDisabled };

class FrameSchedulerImplTaskRunnerWithCustomDeleterTest
    : public FrameSchedulerImplTest,
      public ::testing::WithParamInterface<DeleterTaskRunnerEnabled> {
 public:
  FrameSchedulerImplTaskRunnerWithCustomDeleterTest() {
    feature_list_.Reset();
    if (GetParam() == DeleterTaskRunnerEnabled::kEnabled) {
      feature_list_.InitWithFeatures(
          {blink::features::kUseBlinkSchedulerTaskRunnerWithCustomDeleter}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {blink::features::kUseBlinkSchedulerTaskRunnerWithCustomDeleter});
    }
  }
};

TEST_P(FrameSchedulerImplTaskRunnerWithCustomDeleterTest,
       DeleteSoonAfterShutdown) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      frame_scheduler_->GetTaskRunner(TaskType::kInternalTest);
  int counter = 0;

  // Deleting before shutdown should always work.
  std::unique_ptr<TestObject> test_object1 =
      std::make_unique<TestObject>(&counter);
  task_runner->DeleteSoon(FROM_HERE, std::move(test_object1));
  EXPECT_EQ(counter, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(counter, 1);

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&IncrementCounter, base::Unretained(&counter)));
  frame_scheduler_.reset();

  EXPECT_EQ(counter, 1);
  // Because of graceful shutdown, the increment task should run since it was
  // queued. Since it's empty after the task finishes, the queue should then be
  // shut down.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(counter, 2);

  std::unique_ptr<TestObject> test_object2 =
      std::make_unique<TestObject>(&counter);
  TestObject* unowned_test_object2 = test_object2.get();
  task_runner->DeleteSoon(FROM_HERE, std::move(test_object2));
  EXPECT_EQ(counter, 2);
  base::RunLoop().RunUntilIdle();

  // Without the custom task runner, this leaks.
  if (GetParam() == DeleterTaskRunnerEnabled::kDisabled) {
    EXPECT_EQ(counter, 2);
    delete (unowned_test_object2);
  } else {
    EXPECT_EQ(counter, 3);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    FrameSchedulerImplTaskRunnerWithCustomDeleterTest,
    testing::Values(DeleterTaskRunnerEnabled::kEnabled,
                    DeleterTaskRunnerEnabled::kDisabled),
    [](const testing::TestParamInfo<DeleterTaskRunnerEnabled>& info) {
      switch (info.param) {
        case DeleterTaskRunnerEnabled::kEnabled:
          return "Enabled";
        case DeleterTaskRunnerEnabled::kDisabled:
          return "Disabled";
      }
    });

}  // namespace frame_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
