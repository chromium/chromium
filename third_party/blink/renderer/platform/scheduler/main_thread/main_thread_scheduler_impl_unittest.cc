// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/common/task_annotator.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/platform/web_input_event_result.h"
#include "third_party/blink/renderer/platform/scheduler/common/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/task_priority.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/find_in_page_budget_pool_controller.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_queue_type.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/scheduler/test/recording_task_time_observer.h"
#include "third_party/blink/renderer/platform/scheduler/test/web_scheduling_test_helper.h"
#include "v8/include/v8.h"

using base::sequence_manager::TaskQueue;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace main_thread_scheduler_impl_unittest {

namespace {
using ::base::Feature;
using ::base::sequence_manager::FakeTask;
using ::base::sequence_manager::FakeTaskTiming;
using blink::WebInputEvent;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using InputEventState = WidgetScheduler::InputEventState;

constexpr base::TimeDelta kDelayForHighPriorityRendering =
    base::Milliseconds(150);

// This is a wrapper around MainThreadSchedulerImpl::CreatePageScheduler, that
// returns the PageScheduler as a PageSchedulerImpl.
std::unique_ptr<PageSchedulerImpl> CreatePageScheduler(
    PageScheduler::Delegate* page_scheduler_delegate,
    ThreadSchedulerBase* scheduler,
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

class MockFrameDelegate : public FrameScheduler::Delegate {
 public:
  MockFrameDelegate() {
    ON_CALL(*this, GetAgentClusterId)
        .WillByDefault(ReturnRef(agent_cluster_id_));
  }

  MOCK_METHOD(const base::UnguessableToken&,
              GetAgentClusterId,
              (),
              (const, override));
  MOCK_METHOD(ukm::UkmRecorder*, GetUkmRecorder, ());
  MOCK_METHOD(ukm::SourceId, GetUkmSourceId, ());
  MOCK_METHOD(void, UpdateTaskTime, (base::TimeDelta));
  MOCK_METHOD(void, UpdateActiveSchedulerTrackedFeatures, (uint64_t));

 private:
  base::UnguessableToken agent_cluster_id_ = base::UnguessableToken::Create();
};

}  // namespace

class FakeInputEvent : public blink::WebInputEvent {
 public:
  explicit FakeInputEvent(blink::WebInputEvent::Type event_type,
                          int modifiers = WebInputEvent::kNoModifiers)
      : WebInputEvent(event_type,
                      modifiers,
                      WebInputEvent::GetStaticTimeStampForTests()) {}

  std::unique_ptr<WebInputEvent> Clone() const override {
    return std::make_unique<FakeInputEvent>(*this);
  }

  bool CanCoalesce(const blink::WebInputEvent& event) const override {
    return false;
  }

  void Coalesce(const WebInputEvent& event) override {
    NOTREACHED_IN_MIGRATION();
  }
};

class FakeTouchEvent : public blink::WebTouchEvent {
 public:
  explicit FakeTouchEvent(blink::WebInputEvent::Type event_type,
                          DispatchType dispatch_type =
                              blink::WebInputEvent::DispatchType::kBlocking)
      : WebTouchEvent(event_type,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests()) {
    this->dispatch_type = dispatch_type;
  }
};

class FakeMouseWheelEvent : public blink::WebMouseWheelEvent {
 public:
  explicit FakeMouseWheelEvent(
      blink::WebInputEvent::Type event_type,
      DispatchType dispatch_type =
          blink::WebInputEvent::DispatchType::kBlocking)
      : WebMouseWheelEvent(event_type,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests()) {
    this->dispatch_type = dispatch_type;
  }
};

void AppendToVectorTestTask(Vector<String>* vector, String value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(Vector<String>* vector,
                                String value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void NullTask() {}

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 Vector<int>* vector,
                                 int* reentrant_count,
                                 int max_reentrant_count) {
  vector->push_back((*reentrant_count)++);
  if (*reentrant_count < max_reentrant_count) {
    task_runner->PostTask(FROM_HERE,
                          base::BindOnce(AppendToVectorReentrantTask,
                                         base::Unretained(task_runner), vector,
                                         reentrant_count, max_reentrant_count));
  }
}

void IdleTestTask(int* run_count,
                  base::TimeTicks* deadline_out,
                  base::TimeTicks deadline) {
  (*run_count)++;
  *deadline_out = deadline;
}

int g_max_idle_task_reposts = 2;

void RepostingIdleTestTask(SingleThreadIdleTaskRunner* idle_task_runner,
                           int* run_count,
                           base::TimeTicks deadline) {
  if ((*run_count + 1) < g_max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE,
        base::BindOnce(&RepostingIdleTestTask,
                       base::Unretained(idle_task_runner), run_count));
  }
  (*run_count)++;
}

void RepostingUpdateClockIdleTestTask(
    SingleThreadIdleTaskRunner* idle_task_runner,
    int* run_count,
    scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner,
    base::TimeDelta advance_time,
    Vector<base::TimeTicks>* deadlines,
    base::TimeTicks deadline) {
  if ((*run_count + 1) < g_max_idle_task_reposts) {
    idle_task_runner->PostIdleTask(
        FROM_HERE, base::BindOnce(&RepostingUpdateClockIdleTestTask,
                                  base::Unretained(idle_task_runner), run_count,
                                  test_task_runner, advance_time, deadlines));
  }
  deadlines->push_back(deadline);
  (*run_count)++;
  test_task_runner->AdvanceMockTickClock(advance_time);
}

void WillBeginFrameIdleTask(MainThreadSchedulerImpl* scheduler,
                            uint64_t sequence_number,
                            const base::TickClock* clock,
                            base::TimeTicks deadline) {
  scheduler->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, sequence_number, clock->NowTicks(),
      base::TimeTicks(), base::Milliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
}

void UpdateClockToDeadlineIdleTestTask(
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
    int* run_count,
    base::TimeTicks deadline) {
  task_runner->AdvanceMockTickClock(
      deadline - task_runner->GetMockTickClock()->NowTicks());
  (*run_count)++;
}

void PostingYieldingTestTask(MainThreadSchedulerImpl* scheduler,
                             base::SingleThreadTaskRunner* task_runner,
                             bool simulate_input,
                             bool* should_yield_before,
                             bool* should_yield_after) {
  *should_yield_before = scheduler->ShouldYieldForHighPriorityWork();
  task_runner->PostTask(FROM_HERE, base::BindOnce(NullTask));
  if (simulate_input) {
    scheduler->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }
  *should_yield_after = scheduler->ShouldYieldForHighPriorityWork();
}

enum class SimulateInputType {
  kNone,
  kTouchStart,
  kTouchEnd,
  kGestureScrollBegin,
  kGestureScrollEnd
};

class MockPageSchedulerImpl : public PageSchedulerImpl {
 public:
  explicit MockPageSchedulerImpl(MainThreadSchedulerImpl* main_thread_scheduler,
                                 AgentGroupSchedulerImpl& agent_group_scheduler)
      : PageSchedulerImpl(nullptr, agent_group_scheduler) {
    ON_CALL(*this, IsWaitingForMainFrameContentfulPaint)
        .WillByDefault(Return(false));
    ON_CALL(*this, IsWaitingForMainFrameMeaningfulPaint)
        .WillByDefault(Return(false));
    ON_CALL(*this, IsMainFrameLoading).WillByDefault(Return(false));
    ON_CALL(*this, IsMainFrameLocal).WillByDefault(Return(true));
    ON_CALL(*this, IsOrdinary).WillByDefault(Return(true));

    // This would normally be called by
    // MainThreadSchedulerImpl::CreatePageScheduler.
    main_thread_scheduler->AddPageScheduler(this);
  }
  MockPageSchedulerImpl(const MockPageSchedulerImpl&) = delete;
  MockPageSchedulerImpl& operator=(const MockPageSchedulerImpl&) = delete;
  ~MockPageSchedulerImpl() override = default;

  MOCK_METHOD(bool, RequestBeginMainFrameNotExpected, (bool));
  MOCK_METHOD(bool, IsWaitingForMainFrameContentfulPaint, (), (const));
  MOCK_METHOD(bool, IsWaitingForMainFrameMeaningfulPaint, (), (const));
  MOCK_METHOD(bool, IsMainFrameLoading, (), (const));
  MOCK_METHOD(bool, IsMainFrameLocal, (), (const));
  MOCK_METHOD(bool, IsOrdinary, (), (const));
};

class MainThreadSchedulerImplForTest : public MainThreadSchedulerImpl {
 public:
  using MainThreadSchedulerImpl::CompositorTaskQueue;
  using MainThreadSchedulerImpl::ControlTaskQueue;
  using MainThreadSchedulerImpl::DefaultTaskQueue;
  using MainThreadSchedulerImpl::OnIdlePeriodEnded;
  using MainThreadSchedulerImpl::OnIdlePeriodStarted;
  using MainThreadSchedulerImpl::OnPendingTasksChanged;
  using MainThreadSchedulerImpl::SetHaveSeenABlockingGestureForTesting;
  using MainThreadSchedulerImpl::V8TaskQueue;

  explicit MainThreadSchedulerImplForTest(
      std::unique_ptr<base::sequence_manager::SequenceManager> manager)
      : MainThreadSchedulerImpl(std::move(manager)), update_policy_count_(0) {}

  void UpdatePolicyLocked(UpdateType update_type) override {
    update_policy_count_++;
    MainThreadSchedulerImpl::UpdatePolicyLocked(update_type);

    String use_case = UseCaseToString(main_thread_only().current_use_case);
    if (main_thread_only().blocking_input_expected_soon) {
      use_cases_.push_back(use_case + " blocking input expected");
    } else {
      use_cases_.push_back(use_case);
    }
  }

  void EnsureUrgentPolicyUpdatePostedOnMainThread() {
    base::AutoLock lock(any_thread_lock_);
    MainThreadSchedulerImpl::EnsureUrgentPolicyUpdatePostedOnMainThread(
        FROM_HERE);
  }

  void ScheduleDelayedPolicyUpdate(base::TimeTicks now, base::TimeDelta delay) {
    delayed_update_policy_runner_.SetDeadline(FROM_HERE, delay, now);
  }

  bool BeginMainFrameOnCriticalPath() {
    base::AutoLock lock(any_thread_lock_);
    return any_thread().begin_main_frame_on_critical_path;
  }

  void PerformMicrotaskCheckpoint() override {
    if (on_microtask_checkpoint_)
      std::move(on_microtask_checkpoint_).Run();
  }

  void SetCurrentUseCase(UseCase use_case) {
    SetCurrentUseCaseForTest(use_case);
  }

  int update_policy_count_;
  Vector<String> use_cases_;
  base::OnceClosure on_microtask_checkpoint_;
};

// Lets gtest print human readable Policy values.
::std::ostream& operator<<(::std::ostream& os, const UseCase& use_case) {
  return os << UseCaseToString(use_case);
}

class MainThreadSchedulerImplTest : public testing::Test {
 public:
  MainThreadSchedulerImplTest(
      const std::vector<base::test::FeatureRef>& features_to_enable,
      const std::vector<base::test::FeatureRef>& features_to_disable) {
    feature_list_.InitWithFeatures(features_to_enable, features_to_disable);
  }

  explicit MainThreadSchedulerImplTest(
      std::vector<::base::test::FeatureRefAndParams> features_to_enable) {
    feature_list_.InitWithFeaturesAndParameters(features_to_enable, {});
  }

  MainThreadSchedulerImplTest() : MainThreadSchedulerImplTest({}, {}) {}

  MainThreadSchedulerImplTest(const MainThreadSchedulerImplTest&) = delete;
  MainThreadSchedulerImplTest& operator=(const MainThreadSchedulerImplTest&) =
      delete;

  ~MainThreadSchedulerImplTest() override = default;

  void SetUp() override {
    CreateTestTaskRunner();
    Initialize(std::make_unique<MainThreadSchedulerImplForTest>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, test_task_runner_->GetMockTickClock(),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetRandomisedSamplingEnabled(true)
                .SetPrioritySettings(CreatePrioritySettings())
                .Build())));

    EXPECT_EQ(ForceUpdatePolicyAndGetCurrentUseCase(), UseCase::kNone);
    // Don't count the above policy change.
    scheduler_->update_policy_count_ = 0;
    scheduler_->use_cases_.clear();
  }

  void CreateTestTaskRunner() {
    test_task_runner_ = base::WrapRefCounted(new base::TestMockTimeTaskRunner(
        base::TestMockTimeTaskRunner::Type::kBoundToThread));
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(base::Milliseconds(5));
  }

  void Initialize(std::unique_ptr<MainThreadSchedulerImplForTest> scheduler) {
    scheduler_ = std::move(scheduler);

    if (kLaunchingProcessIsBackgrounded) {
      scheduler_->SetRendererBackgrounded(false);
      // Reset the policy count as foregrounding would force an initial update.
      scheduler_->update_policy_count_ = 0;
      scheduler_->use_cases_.clear();
    }

    default_task_runner_ =
        scheduler_->DefaultTaskQueue()->GetTaskRunnerWithDefaultTaskType();
    idle_task_runner_ = scheduler_->IdleTaskRunner();
    v8_task_runner_ =
        scheduler_->V8TaskQueue()->GetTaskRunnerWithDefaultTaskType();

    agent_group_scheduler_ = static_cast<AgentGroupSchedulerImpl*>(
        scheduler_->CreateAgentGroupScheduler());
    compositor_task_runner_ = agent_group_scheduler_->CompositorTaskQueue()
                                  ->GetTaskRunnerWithDefaultTaskType();
    page_scheduler_ = std::make_unique<NiceMock<MockPageSchedulerImpl>>(
        scheduler_.get(), *agent_group_scheduler_);
    agent_group_scheduler_->AddPageSchedulerForTesting(page_scheduler_.get());
    main_frame_scheduler_ =
        CreateFrameScheduler(page_scheduler_.get(), nullptr,
                             /*is_in_embedded_frame_tree=*/false,
                             FrameScheduler::FrameType::kMainFrame);

    widget_scheduler_ = scheduler_->CreateWidgetScheduler();
    input_task_runner_ = widget_scheduler_->InputTaskRunner();

    loading_control_task_runner_ =
        main_frame_scheduler_->FrameTaskQueueControllerForTest()
            ->GetTaskQueue(
                main_frame_scheduler_->LoadingControlTaskQueueTraits())
            ->GetTaskRunnerWithDefaultTaskType();
    throttleable_task_runner_ =
        throttleable_task_queue()->GetTaskRunnerWithDefaultTaskType();
    find_in_page_task_runner_ = main_frame_scheduler_->GetTaskRunner(
        blink::TaskType::kInternalFindInPage);
    prioritised_local_frame_task_runner_ = main_frame_scheduler_->GetTaskRunner(
        blink::TaskType::kInternalHighPriorityLocalFrame);
    render_blocking_task_runner_ = main_frame_scheduler_->GetTaskRunner(
        blink::TaskType::kNetworkingUnfreezableRenderBlockingLoading);
  }

  MainThreadTaskQueue* compositor_task_queue() {
    return agent_group_scheduler_->CompositorTaskQueue().get();
  }

  MainThreadTaskQueue* loading_task_queue() {
    auto queue_traits = FrameSchedulerImpl::LoadingTaskQueueTraits();
    return main_frame_scheduler_->FrameTaskQueueControllerForTest()
        ->GetTaskQueue(queue_traits)
        .get();
  }

  MainThreadTaskQueue* throttleable_task_queue() {
    auto* frame_task_queue_controller =
        main_frame_scheduler_->FrameTaskQueueControllerForTest();
    return frame_task_queue_controller
        ->GetTaskQueue(main_frame_scheduler_->ThrottleableTaskQueueTraits())
        .get();
  }

  MainThreadTaskQueue* find_in_page_task_queue() {
    auto* frame_task_queue_controller =
        main_frame_scheduler_->FrameTaskQueueControllerForTest();

    return frame_task_queue_controller
        ->GetTaskQueue(main_frame_scheduler_->FindInPageTaskQueueTraits())
        .get();
  }

  scoped_refptr<MainThreadTaskQueue> NewUnpausableTaskQueue() {
    return scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameUnpausable)
            .SetQueueTraits(
                main_frame_scheduler_->UnpausableTaskQueueTraits()));
  }

  void TearDown() override {
    widget_scheduler_.reset();
    main_frame_scheduler_.reset();
    page_scheduler_.reset();
    agent_group_scheduler_ = nullptr;
    scheduler_->Shutdown();
    base::RunLoop().RunUntilIdle();
    scheduler_.reset();
  }

  virtual base::TimeTicks Now() {
    CHECK(test_task_runner_);
    return test_task_runner_->GetMockTickClock()->NowTicks();
  }

  void AdvanceMockTickClockTo(base::TimeTicks time) {
    CHECK(test_task_runner_);
    CHECK_LE(Now(), time);
    test_task_runner_->AdvanceMockTickClock(time - Now());
  }

  void AdvanceMockTickClockBy(base::TimeDelta delta) {
    CHECK(test_task_runner_);
    CHECK_LE(base::TimeDelta(), delta);
    test_task_runner_->AdvanceMockTickClock(delta);
  }

  void DoMainFrame() {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidCommitFrameToCompositor();
  }

  void DoMainFrameOnCriticalPath() {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
  }

  void ForceBlockingInputToBeExpectedSoon() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollEnd),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    test_task_runner_->AdvanceMockTickClock(UserModel::kGestureEstimationLimit *
                                            2);
    scheduler_->ForceUpdatePolicy();
  }

  void SimulateExpensiveTasks(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    // Simulate a bunch of expensive tasks.
    for (int i = 0; i < 10; i++) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&base::TestMockTimeTaskRunner::AdvanceMockTickClock,
                         test_task_runner_, base::Milliseconds(500)));
    }
    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  void SimulateEnteringCompositorGestureUseCase() {
    SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
  }

  void SimulateRenderBlockingTask(base::TimeDelta duration) {
    render_blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&base::TestMockTimeTaskRunner::AdvanceMockTickClock,
                       test_task_runner_, duration));
    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  enum class TouchEventPolicy {
    kSendTouchStart,
    kDontSendTouchStart,
  };

  void SimulateCompositorGestureStart(TouchEventPolicy touch_event_policy) {
    if (touch_event_policy == TouchEventPolicy::kSendTouchStart) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    }
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }

  // Simulate a gesture where there is an active compositor scroll, but no
  // scroll updates are generated. Instead, the main thread handles
  // non-canceleable touch events, making this an effectively main thread
  // driven gesture.
  void SimulateMainThreadGestureWithoutScrollUpdates() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  }

  // Simulate a gesture where the main thread handles touch events but does not
  // preventDefault(), allowing the gesture to turn into a compositor driven
  // gesture. This function also verifies the necessary policy updates are
  // scheduled.
  void SimulateMainThreadGestureWithoutPreventDefault() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    // Touchstart policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kTouchstart, ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureTapCancel),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    // Main thread gesture policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling,
              ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchScrollStarted),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    // Compositor thread gesture policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kCompositorGesture,
              ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());
  }

  void SimulateMainThreadGestureStart(TouchEventPolicy touch_event_policy,
                                      blink::WebInputEvent::Type gesture_type) {
    if (touch_event_policy == TouchEventPolicy::kSendTouchStart) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
          WebInputEventResult::kHandledSystem,
          /*frame_requested=*/true);

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          WebInputEventResult::kHandledSystem,
          /*frame_requested=*/true);

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
          WebInputEventResult::kHandledSystem,
          /*frame_requested=*/true);
    }
    if (gesture_type != blink::WebInputEvent::Type::kUndefined) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(gesture_type),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(gesture_type), WebInputEventResult::kHandledSystem,
          /*frame_requested=*/true);
    }
  }

  void SimulateMainThreadInputHandlingCompositorTask(
      base::TimeDelta begin_main_frame_duration) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    test_task_runner_->AdvanceMockTickClock(begin_main_frame_duration);
    scheduler_->DidHandleInputEventOnMainThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        WebInputEventResult::kHandledApplication,
        /*frame_requested=*/true);
    scheduler_->DidCommitFrameToCompositor();
  }

  void SimulateMainThreadCompositorTask(
      base::TimeDelta begin_main_frame_duration) {
    test_task_runner_->AdvanceMockTickClock(begin_main_frame_duration);
    scheduler_->DidCommitFrameToCompositor();
  }

  void SimulateMainThreadCompositorAndQuitRunLoopTask(
      base::TimeDelta begin_main_frame_duration) {
    SimulateMainThreadCompositorTask(begin_main_frame_duration);
    base::RunLoop().Quit();
  }

  void SimulateThrottleableTask(base::TimeDelta duration) {
    test_task_runner_->AdvanceMockTickClock(duration);
    simulate_throttleable_task_ran_ = true;
  }

  void EnableIdleTasks() { DoMainFrame(); }

  UseCase CurrentUseCase() {
    return scheduler_->main_thread_only().current_use_case;
  }

  UseCase ForceUpdatePolicyAndGetCurrentUseCase() {
    scheduler_->ForceUpdatePolicy();
    return scheduler_->main_thread_only().current_use_case;
  }

  RAILMode GetRAILMode() {
    return scheduler_->main_thread_only().current_policy.rail_mode;
  }

  bool BlockingInputExpectedSoon() {
    return scheduler_->main_thread_only().blocking_input_expected_soon;
  }

  base::TimeTicks EstimatedNextFrameBegin() {
    return scheduler_->main_thread_only().estimated_next_frame_begin;
  }

  bool HaveSeenABlockingGesture() {
    base::AutoLock lock(scheduler_->any_thread_lock_);
    return scheduler_->any_thread().have_seen_a_blocking_gesture;
  }

  void AdvanceTimeWithTask(base::TimeDelta duration) {
    RunTask(base::BindOnce(
        [](scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner,
           base::TimeDelta duration) {
          test_task_runner->AdvanceMockTickClock(duration);
        },
        test_task_runner_, duration));
  }

  void RunTask(base::OnceClosure task) {
    scoped_refptr<MainThreadTaskQueue> fake_queue =
        scheduler_->NewLoadingTaskQueue(
            MainThreadTaskQueue::QueueType::kFrameLoading, nullptr);

    base::TimeTicks start = Now();
    FakeTask fake_task;
    fake_task.set_enqueue_order(
        base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
    scheduler_->OnTaskStarted(fake_queue.get(), fake_task,
                              FakeTaskTiming(start, base::TimeTicks()));
    std::move(task).Run();
    base::TimeTicks end = Now();
    FakeTaskTiming task_timing(start, end);
    scheduler_->OnTaskCompleted(fake_queue->weak_ptr_factory_.GetWeakPtr(),
                                fake_task, &task_timing, nullptr);
  }

  void RunSlowCompositorTask() {
    // Run a long compositor task so that compositor tasks appear to be running
    // slow and thus compositor tasks will not be prioritized.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MainThreadSchedulerImplTest::SimulateMainThreadCompositorTask,
            base::Unretained(this), base::Milliseconds(1000)));
    base::RunLoop().RunUntilIdle();
  }

  void AppendToVectorBeginMainFrameTask(Vector<String>* vector, String value) {
    DoMainFrame();
    AppendToVectorTestTask(vector, value);
  }

  void AppendToVectorBeginMainFrameTaskWithInput(Vector<String>* vector,
                                                 String value) {
    scheduler_->DidHandleInputEventOnMainThread(
        FakeInputEvent(WebInputEvent::Type::kMouseMove),
        WebInputEventResult::kHandledApplication,
        /*frame_requested=*/true);
    AppendToVectorBeginMainFrameTask(vector, value);
  }

  void AppendToVectorInputEventTask(WebInputEvent::Type event_type,
                                    Vector<String>* vector,
                                    String value) {
    scheduler_->DidHandleInputEventOnMainThread(
        FakeInputEvent(event_type), WebInputEventResult::kHandledApplication,
        /*frame_requested=*/true);
    AppendToVectorTestTask(vector, value);
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type. For 'C' and 'P' types, the second
  // letter specifies that type of task to simulate.
  // - 'D': Default task
  // - 'C': Compositor task
  //   - "CM": Compositor task that simulates running a main frame
  //   - "CI": Compositor task that simulates running a main frame with
  //            rAF-algined input
  // - 'P': Input task
  //   - "PC": Input task that simulates dispatching a continuous input event
  //   - "PD": Input task that simulates dispatching a discrete input event
  // - 'E': Input task that dispatches input events
  // - 'L': Loading task
  // - 'M': Loading Control task
  // - 'I': Idle task
  // - 'R': Render-blocking task
  // - 'T': Throttleable task
  // - 'V': kV8 task
  // - 'F': FindInPage task
  // - 'U': Prioritised local frame task
  void PostTestTasks(Vector<String>* run_order, const String& task_descriptor) {
    std::istringstream stream(task_descriptor.Utf8());
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'C':
          if (base::StartsWith(task, "CM")) {
            compositor_task_runner_->PostTask(
                FROM_HERE, base::BindOnce(&MainThreadSchedulerImplTest::
                                              AppendToVectorBeginMainFrameTask,
                                          base::Unretained(this), run_order,
                                          String::FromUTF8(task)));
          } else if (base::StartsWith(task, "CI")) {
            compositor_task_runner_->PostTask(
                FROM_HERE,
                base::BindOnce(&MainThreadSchedulerImplTest::
                                   AppendToVectorBeginMainFrameTaskWithInput,
                               base::Unretained(this), run_order,
                               String::FromUTF8(task)));
          } else {
            compositor_task_runner_->PostTask(
                FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                          String::FromUTF8(task)));
          }
          break;
        case 'P':
          if (base::StartsWith(task, "PC")) {
            input_task_runner_->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &MainThreadSchedulerImplTest::AppendToVectorInputEventTask,
                    base::Unretained(this), WebInputEvent::Type::kMouseMove,
                    run_order, String::FromUTF8(task)));

          } else if (base::StartsWith(task, "PD")) {
            input_task_runner_->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &MainThreadSchedulerImplTest::AppendToVectorInputEventTask,
                    base::Unretained(this), WebInputEvent::Type::kMouseUp,
                    run_order, String::FromUTF8(task)));
          } else {
            input_task_runner_->PostTask(
                FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                          String::FromUTF8(task)));
          }
          break;
        case 'L':
          loading_task_queue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'M':
          loading_control_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE, base::BindOnce(&AppendToVectorIdleTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'R':
          render_blocking_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'T':
          throttleable_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'V':
          v8_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'F':
          find_in_page_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        case 'U':
          prioritised_local_frame_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&AppendToVectorTestTask, run_order,
                                        String::FromUTF8(task)));
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }

 protected:
  static base::TimeDelta maximum_idle_period_duration() {
    return IdleHelper::kMaximumIdlePeriod;
  }

  static base::TimeDelta end_idle_when_hidden_delay() {
    return base::Milliseconds(
        MainThreadSchedulerImpl::kEndIdleWhenHiddenDelayMillis);
  }

  static scoped_refptr<MainThreadTaskQueue> ThrottleableTaskQueue(
      FrameSchedulerImpl* scheduler) {
    auto* frame_task_queue_controller =
        scheduler->FrameTaskQueueControllerForTest();
    auto queue_traits = FrameSchedulerImpl::ThrottleableTaskQueueTraits();
    return frame_task_queue_controller->GetTaskQueue(queue_traits);
  }

  static scoped_refptr<MainThreadTaskQueue> QueueForTaskType(
      FrameSchedulerImpl* scheduler,
      TaskType task_type) {
    return scheduler->GetTaskQueue(task_type);
  }

  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;

  std::unique_ptr<MainThreadSchedulerImplForTest> scheduler_;
  Persistent<AgentGroupSchedulerImpl> agent_group_scheduler_;
  std::unique_ptr<MockPageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler_;
  scoped_refptr<WidgetScheduler> widget_scheduler_;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_control_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> throttleable_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> v8_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> find_in_page_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner>
      prioritised_local_frame_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> render_blocking_task_runner_;
  bool simulate_throttleable_task_ran_;
  uint64_t next_begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;
};

class
    MainThreadSchedulerImplWithLoadingPhaseBufferTimeAfterFirstMeaningfulPaintTest
    : public MainThreadSchedulerImplTest,
      public ::testing::WithParamInterface<bool> {
 public:
  MainThreadSchedulerImplWithLoadingPhaseBufferTimeAfterFirstMeaningfulPaintTest() {
    if (GetParam()) {
      feature_list_.Reset();
      feature_list_.InitWithFeaturesAndParameters(
          {base::test::FeatureRefAndParams(
              features::kLoadingPhaseBufferTimeAfterFirstMeaningfulPaint,
              {{"LoadingPhaseBufferTimeAfterFirstMeaningfulPaintMillis",
                "5000"}})},
          {});
    }
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    MainThreadSchedulerImplWithLoadingPhaseBufferTimeAfterFirstMeaningfulPaintTest,
    testing::Bool());

TEST_F(MainThreadSchedulerImplTest, TestPostDefaultTask) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "D3", "D4"));
}

TEST_F(MainThreadSchedulerImplTest, TestPostDefaultAndCompositor) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::Contains("D1"));
  EXPECT_THAT(run_order, testing::Contains("C1"));
  EXPECT_THAT(run_order, testing::Contains("P1"));
}

TEST_F(MainThreadSchedulerImplTest, TestRentrantTask) {
  int count = 0;
  Vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(AppendToVectorReentrantTask,
                                base::RetainedRef(default_task_runner_),
                                &run_order, &count, 5));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(MainThreadSchedulerImplTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline = Now() + base::Milliseconds(2300);
  base::TimeTicks deadline_in_task;

  test_task_runner_->AdvanceMockTickClock(base::Milliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no WillBeginFrame.

  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::Milliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run as no DidCommitFrameToCompositor.

  test_task_runner_->AdvanceMockTickClock(base::Milliseconds(1200));
  scheduler_->DidCommitFrameToCompositor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // We missed the deadline.

  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::Milliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  test_task_runner_->AdvanceMockTickClock(base::Milliseconds(800));
  scheduler_->DidCommitFrameToCompositor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest, TestRepostingIdleTask) {
  int run_count = 0;

  g_max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count));
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);

  // Reposted tasks shouldn't run until next idle period.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestIdleTaskExceedsDeadline) {
  int run_count = 0;

  // Post two UpdateClockToDeadlineIdleTestTask tasks.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&UpdateClockToDeadlineIdleTestTask,
                                test_task_runner_, &run_count));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&UpdateClockToDeadlineIdleTestTask,
                                test_task_runner_, &run_count));

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Only the first idle task should execute since it's used up the deadline.
  EXPECT_EQ(1, run_count);

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Second task should be run on the next idle period.
  EXPECT_EQ(2, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestDelayedEndIdlePeriodCanceled) {
  int run_count = 0;

  base::TimeTicks deadline_in_task;
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  // Trigger the beginning of an idle period for 1000ms.
  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::Milliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  DoMainFrame();

  // End the idle period early (after 500ms), and send a WillBeginFrame which
  // specifies that the next idle period should end 1000ms from now.
  test_task_runner_->AdvanceMockTickClock(base::Milliseconds(500));
  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::Milliseconds(1000),
      viz::BeginFrameArgs::NORMAL));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Not currently in an idle period.

  // Trigger the start of the idle period before the task to end the previous
  // idle period has been triggered.
  test_task_runner_->AdvanceMockTickClock(base::Milliseconds(400));
  scheduler_->DidCommitFrameToCompositor();

  // Post a task which simulates running until after the previous end idle
  // period delayed task was scheduled for
  scheduler_->DefaultTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE, base::BindOnce(NullTask));
  test_task_runner_->FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(1, run_count);  // We should still be in the new idle period.
}

TEST_F(MainThreadSchedulerImplTest, TestDefaultPolicy) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 P1 C1 D2 P2 C2 U1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // High-priority input is enabled and input tasks are processed first.
  // One compositing event is prioritized after an input event but still
  // has lower priority than input event.
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "P2", "U1", "L1", "D1",
                                              "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestDefaultPolicyWithSlowCompositor) {
  DoMainFrame();
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 P1 D2 C2");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Even with slow compositor input tasks are handled first.
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "L1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("L1", "D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutScrollUpdates) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateMainThreadGestureWithoutScrollUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateMainThreadGestureWithoutPreventDefault();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("L1", "D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_LongGestureDuration) {
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  base::TimeTicks loop_end_time = Now() + UserModel::kMedianGestureDuration * 2;

  // The UseCase::kCompositorGesture usecase initially deprioritizes
  // compositor tasks (see
  // TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) but if the
  // gesture is long enough, compositor tasks get prioritized again.
  while (Now() < loop_end_time) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    test_task_runner_->AdvanceMockTickClock(base::Milliseconds(16));
    base::RunLoop().RunUntilIdle();
  }

  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "L1", "D1", "D2"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithoutTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("L1", "D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutTouchHandler) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_PreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledApplication,
      /*frame_requested=*/true);
  base::RunLoop().RunUntilIdle();
  // Because the main thread is performing custom input handling, we let all
  // tasks run. However compositing tasks are still given priority.
  EXPECT_THAT(run_order,
              testing::ElementsAre("C1", "C2", "L1", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTest,
    TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_NoPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  base::RunLoop().RunUntilIdle();
  // Because we are still waiting for the touchstart to be processed,
  // non-essential tasks like loading tasks are blocked.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kTouchstart, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, Navigation_ResetsTaskCostEstimations) {
  Vector<String> run_order;

  SimulateExpensiveTasks(throttleable_task_runner_);
  DoMainFrame();
  // A navigation occurs which creates a new Document thus resetting the task
  // cost estimations.
  scheduler_->DidStartProvisionalLoad(true);
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollUpdate);

  PostTestTasks(&run_order, "C1 T1");

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("C1", "T1"));
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicy_Compositor) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of throttleable, idle and
  // loading tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  // Animation or meta events like TapDown/FlingCancel shouldn't affect the
  // priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingCancel),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureTapDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the throttleable, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("L1", "T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicy_MainThread) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of throttleable, idle and
  // loading tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  // Meta events like TapDown/FlingCancel shouldn't affect the priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingCancel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingCancel),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureTapDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureTapDown),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the throttleable, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("L1", "T1", "T2"));
}

TEST_P(
    MainThreadSchedulerImplWithLoadingPhaseBufferTimeAfterFirstMeaningfulPaintTest,
    InitiallyInEarlyLoadingUseCase) {
  // `IsWaitingForMainFrame(Contentful|Meaningful)Paint return true for a new
  // page scheduler in production.
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsMainFrameLoading).WillByDefault(Return(true));

  scheduler_->OnMainFramePaint();

  // Should be early loading by default.
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kLoading, CurrentUseCase());

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(false));
  ON_CALL(*page_scheduler_, IsMainFrameLoading).WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_P(
    MainThreadSchedulerImplWithLoadingPhaseBufferTimeAfterFirstMeaningfulPaintTest,
    NonOrdinaryPageDoesNotTriggerLoadingUseCase) {
  // `IsWaitingForMainFrame(Contentful|Meaningful)Paint return true for a new
  // page scheduler in production.
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsMainFrameLoading).WillByDefault(Return(true));

  // Make the page non-ordinary.
  ON_CALL(*page_scheduler_, IsOrdinary).WillByDefault(Return(false));

  // The UseCase should be `kNone` event if the page is waiting for a first
  // contentful/meaningful paint.
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresMouseMove_WhenMouseUp) {
  DoMainFrame();
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_IgnoresMouseMove_WhenMouseUp) {
  DoMainFrame();
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseMove_WhenMouseDown) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  // Note that currently the compositor will never consume mouse move events,
  // but this test reflects what should happen if that was the case.
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks deprioritized.
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown_AfterMouseWheel) {
  // Simulate a main thread driven mouse wheel scroll gesture.
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollUpdate);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());

  // Now start a main thread mouse touch gesture. It should be detected as main
  // thread custom input handling.
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");
  EnableIdleTasks();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseDown,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest, EventForwardedToMainThread_MouseClick) {
  // A mouse click should be detected as main thread input handling, which means
  // we won't try to defer expensive tasks because of one. We can, however,
  // prioritize compositing/input handling.
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");
  EnableIdleTasks();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseDown,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseUp,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseWheel) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseWheel_PreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized (since they are fast).
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_NoPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2", "I1"));
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTest,
    EventForwardedToMainThreadAndBackToCompositor_MouseWheel_NoPreventDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::Type::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2", "I1"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresKeyboardEvents) {
  DoMainFrame();
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kKeyDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_IgnoresKeyboardEvents) {
  DoMainFrame();
  RunSlowCompositorTask();

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kKeyDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  // Note compositor tasks are not prioritized.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kKeyDown),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
}

TEST_F(MainThreadSchedulerImplTest,
       TestMainthreadScrollingUseCaseDoesNotStarveDefaultTasks) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  EnableIdleTasks();

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1");

  for (int i = 0; i < 20; i++) {
    compositor_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NullTask));
  }
  PostTestTasks(&run_order, "C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1"));
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicyEnds_CompositorHandlesInput) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());

  test_task_runner_->AdvanceMockTickClock(base::Seconds(1));
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicyEnds_MainThreadHandlesInput) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling,
            ForceUpdatePolicyAndGetCurrentUseCase());

  test_task_runner_->AdvanceMockTickClock(base::Seconds(1));
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicyEndsAfterTimeout) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  run_order.clear();
  test_task_runner_->AdvanceMockTickClock(base::Seconds(1));

  // Don't post any compositor tasks to simulate a very long running event
  // handler.
  PostTestTasks(&run_order, "D1 D2");

  // Touchstart policy mode should have ended now that the clock has advanced.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("L1", "D1", "D2"));
}

TEST_F(MainThreadSchedulerImplTest,
       TestTouchstartPolicyEndsAfterConsecutiveTouchmoves) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  // Observation of touchstart should defer execution of idle and loading tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));

  // Receiving the first touchmove will not affect scheduler priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Receiving the second touchmove will kick us back into compositor priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("L1"));
}

TEST_F(MainThreadSchedulerImplTest, TestShouldYield) {
  bool should_yield_before = false;
  bool should_yield_after = false;

  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PostingYieldingTestTask, scheduler_.get(),
                                base::RetainedRef(default_task_runner_), false,
                                &should_yield_before, &should_yield_after));
  base::RunLoop().RunUntilIdle();
  // Posting to default runner shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PostingYieldingTestTask, scheduler_.get(),
                     base::RetainedRef(compositor_task_runner_), false,
                     &should_yield_before, &should_yield_after));
  base::RunLoop().RunUntilIdle();
  // Posting while not mainthread scrolling shouldn't cause yielding.
  EXPECT_FALSE(should_yield_before);
  EXPECT_FALSE(should_yield_after);

  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PostingYieldingTestTask, scheduler_.get(),
                     base::RetainedRef(compositor_task_runner_), true,
                     &should_yield_before, &should_yield_after));
  base::RunLoop().RunUntilIdle();
  // We should be able to switch to compositor priority mid-task.
  EXPECT_FALSE(should_yield_before);
  EXPECT_TRUE(should_yield_after);
}

TEST_F(MainThreadSchedulerImplTest, TestShouldYield_TouchStart) {
  // Receiving a touchstart should immediately trigger yielding, even if
  // there's no immediately pending work in the compositor queue.
  EXPECT_FALSE(scheduler_->ShouldYieldForHighPriorityWork());
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_TRUE(scheduler_->ShouldYieldForHighPriorityWork());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MainThreadSchedulerImplTest, SlowMainThreadInputEvent) {
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // An input event should bump us into input priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Simulate the input event being queued for a very long time. The compositor
  // task we post here represents the enqueued input task.
  test_task_runner_->AdvanceMockTickClock(UserModel::kGestureEstimationLimit *
                                          2);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  base::RunLoop().RunUntilIdle();

  // Even though we exceeded the input priority escalation period, we should
  // still be in main thread gesture since the input remains queued.
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // After the escalation period ends we should go back into normal mode.
  test_task_runner_->FastForwardBy(UserModel::kGestureEstimationLimit * 2);
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, OnlyOnePendingUrgentPolicyUpdate) {
  for (int i = 0; i < 4; i++) {
    scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, OnePendingDelayedAndOneUrgentUpdatePolicy) {
  scheduler_->ScheduleDelayedPolicyUpdate(Now(), base::Milliseconds(1));
  scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();

  test_task_runner_->FastForwardUntilNoTasksRemain();
  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, OneUrgentAndOnePendingDelayedUpdatePolicy) {
  scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  scheduler_->ScheduleDelayedPolicyUpdate(Now(), base::Milliseconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, UpdatePolicyCountTriggeredByOneInputEvent) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  test_task_runner_->AdvanceMockTickClock(base::Seconds(1));
  base::RunLoop().RunUntilIdle();
  // We finally expect a delayed policy update 100ms later.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest,
       UpdatePolicyCountTriggeredByThreeInputEvents) {
  // We expect DidHandleInputEventOnCompositorThread to post
  // an urgent policy update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart,
                     blink::WebInputEvent::DispatchType::kEventNonBlocking),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // The second call to DidHandleInputEventOnCompositorThread should not post
  // a policy update because we are already in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // The third call to DidHandleInputEventOnCompositorThread should post a
  // policy update because the awaiting_touch_start_response_ flag changed.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::Seconds(1));
  // We finally expect a delayed policy update.
  EXPECT_EQ(3, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest,
       UpdatePolicyCountTriggeredByTwoInputEventsWithALongSeparatingDelay) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart,
                     blink::WebInputEvent::DispatchType::kEventNonBlocking),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::Seconds(1));
  // We expect a delayed policy update.
  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect the second call to DidHandleInputEventOnCompositorThread to post
  // an urgent policy update because we are no longer in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  EXPECT_EQ(3, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::Seconds(1));
  // We finally expect a delayed policy update.
  EXPECT_EQ(4, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, EnsureUpdatePolicyNotTriggeredTooOften) {
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  ForceUpdatePolicyAndGetCurrentUseCase();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  // We expect the first call to ShouldYieldForHighPriorityWork to be called
  // after receiving an input event (but before the UpdateTask was processed) to
  // call UpdatePolicy.
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  scheduler_->ShouldYieldForHighPriorityWork();
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  // Subsequent calls should not call UpdatePolicy.
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollEnd),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchEnd),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kTouchEnd),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);

  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect both the urgent and the delayed updates to run in addition to the
  // earlier updated cause by ShouldYieldForHighPriorityWork, a final update
  // transitions from 'not_scrolling touchstart expected' to 'not_scrolling'.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(scheduler_->use_cases_,
              testing::ElementsAre("none", "compositor_gesture",
                                   "compositor_gesture blocking input expected",
                                   "none blocking input expected", "none"));
}

TEST_F(MainThreadSchedulerImplTest,
       BlockingInputExpectedSoonWhenBlockInputEventSeen) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  EXPECT_TRUE(HaveSeenABlockingGesture());
  ForceBlockingInputToBeExpectedSoon();
  EXPECT_TRUE(BlockingInputExpectedSoon());
}

TEST_F(MainThreadSchedulerImplTest,
       BlockingInputNotExpectedSoonWhenNoBlockInputEventSeen) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  EXPECT_FALSE(HaveSeenABlockingGesture());
  ForceBlockingInputToBeExpectedSoon();
  EXPECT_FALSE(BlockingInputExpectedSoon());
}

TEST_F(MainThreadSchedulerImplTest, TestBeginMainFrameNotExpectedUntil) {
  base::TimeDelta ten_millis(base::Milliseconds(10));
  base::TimeTicks expected_deadline = Now() + ten_millis;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  base::TimeTicks now = Now();
  base::TimeTicks frame_time = now + ten_millis;
  // No main frame is expected until frame_time, so short idle work can be
  // scheduled in the mean time.
  scheduler_->BeginMainFrameNotExpectedUntil(frame_time);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriod) {
  base::TimeTicks expected_deadline = Now() + maximum_idle_period_duration();
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no idle period.

  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriodWithPendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::Milliseconds(30);
  base::TimeTicks expected_deadline = Now() + pending_task_delay;
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);  // Should have run in a long idle time.
  EXPECT_EQ(expected_deadline, deadline_in_task);
}

TEST_F(MainThreadSchedulerImplTest,
       TestLongIdlePeriodWithLatePendingDelayedTask) {
  base::TimeDelta pending_task_delay = base::Milliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  test_task_runner_->AdvanceMockTickClock(base::Milliseconds(20));

  // Post an idle task and BeginFrameNotExpectedSoon to initiate a long idle
  // period. Since there is a late pending delayed task this shouldn't actually
  // start an idle period.
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));
  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // After the delayed task has been run we should trigger an idle period.
  test_task_runner_->FastForwardBy(maximum_idle_period_duration());
  EXPECT_EQ(1, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriodRepeating) {
  Vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  g_max_idle_task_reposts = 3;
  base::TimeTicks clock_before = Now();
  base::TimeDelta idle_task_runtime(base::Milliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));
  scheduler_->BeginFrameNotExpectedSoon();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(3, run_count);
  EXPECT_THAT(
      actual_deadlines,
      testing::ElementsAre(clock_before + maximum_idle_period_duration(),
                           clock_before + 2 * maximum_idle_period_duration(),
                           clock_before + 3 * maximum_idle_period_duration()));

  // Check that idle tasks don't run after the idle period ends with a
  // new BeginMainFrame.
  g_max_idle_task_reposts = 5;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingUpdateClockIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count,
                     test_task_runner_, idle_task_runtime, &actual_deadlines));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&WillBeginFrameIdleTask,
                     base::Unretained(scheduler_.get()),
                     next_begin_frame_number_++,
                     base::Unretained(test_task_runner_->GetMockTickClock())));
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(4, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestLongIdlePeriodInTouchStartPolicy) {
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  // Observation of touchstart should defer the start of the long idle period.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // The long idle period should start after the touchstart policy has finished.
  test_task_runner_->FastForwardBy(UserModel::kGestureEstimationLimit);
  EXPECT_EQ(1, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TestRendererHiddenIdlePeriod) {
  int run_count = 0;

  g_max_idle_task_reposts = 2;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count));

  // Renderer should start in visible state.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(0, run_count);

  // When we hide the renderer it should start a max deadline idle period, which
  // will run an idle task and then immediately start a new idle period, which
  // runs the second idle task.
  scheduler_->SetAllRenderWidgetsHidden(true);
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(2, run_count);

  // Advance time by amount of time by the maximum amount of time we execute
  // idle tasks when hidden (plus some slack) - idle period should have ended.
  g_max_idle_task_reposts = 3;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&RepostingIdleTestTask,
                     base::RetainedRef(idle_task_runner_), &run_count));
  test_task_runner_->FastForwardBy(end_idle_when_hidden_delay() +
                                   base::Milliseconds(10));
  EXPECT_EQ(2, run_count);
}

TEST_F(MainThreadSchedulerImplTest, ThrottleableQueueEnabledByDefault) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, StopAndResumeRenderer) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle = scheduler_->PauseScheduler();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, StopAndThrottleThrottleableQueue) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle = scheduler_->PauseScheduler();
  base::RunLoop().RunUntilIdle();
  MainThreadTaskQueue::ThrottleHandle handle =
      throttleable_task_queue()->Throttle();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

TEST_F(MainThreadSchedulerImplTest, MultipleStopsNeedMultipleResumes) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle1 = scheduler_->PauseScheduler();
  auto pause_handle2 = scheduler_->PauseScheduler();
  auto pause_handle3 = scheduler_->PauseScheduler();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle1.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("T1", "T2"));
}

TEST_F(MainThreadSchedulerImplTest, PauseRenderer) {
  // Tasks in some queues don't fire when the renderer is paused.
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 L1 I1 T1");
  auto pause_handle = scheduler_->PauseScheduler();
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "I1"));

  // Tasks are executed when renderer is resumed.
  run_order.clear();
  pause_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("L1", "T1"));
}

TEST_F(MainThreadSchedulerImplTest, UseCaseToString) {
  for (unsigned i = 0; i <= static_cast<unsigned>(UseCase::kMaxValue); i++) {
    UseCaseToString(static_cast<UseCase>(i));
  }
}

TEST_F(MainThreadSchedulerImplTest, MismatchedDidHandleInputEventOnMainThread) {
  // This should not DCHECK because there was no corresponding compositor side
  // call to DidHandleInputEventOnCompositorThread with
  // blink::mojom::InputEventResultState::kNotConsumed. There are legitimate
  // reasons for the compositor to not be there and we don't want to make
  // debugging impossible.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureFlingStart),
      WebInputEventResult::kHandledSystem,
      /*frame_requested=*/true);
}

TEST_F(MainThreadSchedulerImplTest, BeginMainFrameOnCriticalPath) {
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());
  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::Milliseconds(1000), viz::BeginFrameArgs::NORMAL);
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_TRUE(scheduler_->BeginMainFrameOnCriticalPath());

  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());
}

TEST_F(MainThreadSchedulerImplTest, ShutdownPreventsPostingOfNewTasks) {
  main_frame_scheduler_.reset();
  page_scheduler_.reset();
  scheduler_->Shutdown();
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

namespace {
void SlowCountingTask(
    size_t* count,
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
    int task_duration,
    scoped_refptr<base::SingleThreadTaskRunner> throttleable_queue) {
  task_runner->AdvanceMockTickClock(base::Milliseconds(task_duration));
  if (++(*count) < 500) {
    throttleable_queue->PostTask(
        FROM_HERE, base::BindOnce(SlowCountingTask, count, task_runner,
                                  task_duration, throttleable_queue));
  }
}
}  // namespace

TEST_F(
    MainThreadSchedulerImplTest,
    SYNCHRONIZED_GESTURE_ThrottleableTaskThrottling_ThrottleableQueuesStopped) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  base::TimeTicks first_run_time = Now();

  size_t count = 0;
  // With the compositor task taking 10ms, there is not enough time to run this
  // 7ms throttleable task in the 16ms frame.
  throttleable_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &count, test_task_runner_, 7,
                                throttleable_task_runner_));

  std::unique_ptr<MainThreadScheduler::RendererPauseHandle> paused;
  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this), base::Milliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;

    // Before the policy is updated the queue will be enabled. Subsequently it
    // will be disabled until the throttled queue is pumped.
    bool expect_queue_enabled = (i == 0) || (Now() > first_run_time);
    if (paused)
      expect_queue_enabled = false;
    EXPECT_EQ(expect_queue_enabled, throttleable_task_queue()->IsQueueEnabled())
        << "i = " << i;

    // After we've run any expensive tasks suspend the queue.  The throttling
    // helper should /not/ re-enable this queue under any circumstances while
    // throttleable queues are paused.
    if (count > 0 && !paused) {
      EXPECT_EQ(2u, count) << "i = " << i;
      paused = scheduler_->PauseScheduler();
    }
  }

  // Make sure the throttleable queue stayed paused!
  EXPECT_EQ(2u, count);
}

TEST_F(MainThreadSchedulerImplTest,
       SYNCHRONIZED_GESTURE_ThrottleableTaskThrottling_task_not_expensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  size_t count = 0;
  // With the compositor task taking 10ms, there is enough time to run this 6ms
  // throttleable task in the 16ms frame.
  throttleable_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &count, test_task_runner_, 6,
                                throttleable_task_runner_));

  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this), base::Milliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
    EXPECT_TRUE(throttleable_task_queue()->IsQueueEnabled()) << "i = " << i;
  }

  // Task is not throttled.
  EXPECT_EQ(500u, count);
}

TEST_F(MainThreadSchedulerImplTest, DenyLongIdleDuringTouchStart) {
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(UseCase::kTouchstart, ForceUpdatePolicyAndGetCurrentUseCase());

  // First check that long idle is denied during the TOUCHSTART use case.
  IdleHelper::Delegate* idle_delegate = scheduler_.get();
  base::TimeTicks now;
  base::TimeDelta next_time_to_check;
  EXPECT_FALSE(idle_delegate->CanEnterLongIdlePeriod(now, &next_time_to_check));
  EXPECT_GE(next_time_to_check, base::TimeDelta());

  // Check again at a time past the TOUCHSTART expiration. We should still get a
  // non-negative delay to when to check again.
  now += base::Milliseconds(500);
  EXPECT_FALSE(idle_delegate->CanEnterLongIdlePeriod(now, &next_time_to_check));
  EXPECT_GE(next_time_to_check, base::TimeDelta());
}

TEST_F(MainThreadSchedulerImplTest, SYNCHRONIZED_GESTURE_CompositingExpensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  Vector<String> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this), base::Milliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
  }

  // Throttleable tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskPriority::kNormalPriority,
            compositor_task_queue()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(MainThreadSchedulerImplTest, MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  Vector<String> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this), base::Milliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase())
        << "i = " << i;
  }

  // Throttleable tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskPriority::kNormalPriority,
            compositor_task_queue()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(MainThreadSchedulerImplTest, MAIN_THREAD_GESTURE) {
  SimulateMainThreadGestureStart(
      TouchEventPolicy::kDontSendTouchStart,
      blink::WebInputEvent::Type::kGestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. However because this is a main thread
  // gesture instead of custom main thread input handling, we allow the
  // throttleable tasks to be starved.
  Vector<String> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this), base::Milliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase()) << "i = " << i;
  }

  EXPECT_EQ(TaskPriority::kHighestPriority,
            compositor_task_queue()->GetQueuePriority());
  EXPECT_EQ(279u, run_order.size());
}

class MockRAILModeObserver : public RAILModeObserver {
 public:
  MOCK_METHOD(void, OnRAILModeChanged, (RAILMode rail_mode));
};

TEST_F(MainThreadSchedulerImplTest, TestResponseRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kResponse));

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  ForceBlockingInputToBeExpectedSoon();
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kResponse, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestAnimateRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation)).Times(0);

  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestIdleRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kIdle));

  scheduler_->SetAllRenderWidgetsHidden(true);
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kIdle, GetRAILMode());
  scheduler_->SetAllRenderWidgetsHidden(false);
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_P(
    MainThreadSchedulerImplWithLoadingPhaseBufferTimeAfterFirstMeaningfulPaintTest,
    TestLoadRAILMode) {
  InSequence s;
  MockRAILModeObserver observer;
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kLoad));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  scheduler_->AddRAILModeObserver(&observer);

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsMainFrameLoading).WillByDefault(Return(true));
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(RAILMode::kLoad, GetRAILMode());
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(false));
  ON_CALL(*page_scheduler_, IsMainFrameLoading).WillByDefault(Return(false));
  scheduler_->OnMainFramePaint();
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_P(
    MainThreadSchedulerImplWithLoadingPhaseBufferTimeAfterFirstMeaningfulPaintTest,
    InputTerminatesLoadRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kAnimation));
  EXPECT_CALL(observer, OnRAILModeChanged(RAILMode::kLoad));

  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsMainFrameLoading).WillByDefault(Return(true));
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(RAILMode::kLoad, GetRAILMode());
  EXPECT_EQ(UseCase::kEarlyLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(RAILMode::kAnimation, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, UnthrottledTaskRunner) {
  // Ensure neither suspension nor throttleable task throttling affects an
  // unthrottled task runner.
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  scoped_refptr<MainThreadTaskQueue> unthrottled_task_queue =
      NewUnpausableTaskQueue();

  size_t throttleable_count = 0;
  size_t unthrottled_count = 0;
  throttleable_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(SlowCountingTask, &throttleable_count, test_task_runner_,
                     7, throttleable_task_runner_));
  unthrottled_task_queue->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(
          SlowCountingTask, &unthrottled_count, test_task_runner_, 7,
          unthrottled_task_queue->GetTaskRunnerWithDefaultTaskType()));
  auto handle = scheduler_->PauseScheduler();

  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::Milliseconds(16), viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::Type::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this), base::Milliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
  }

  EXPECT_EQ(0u, throttleable_count);
  EXPECT_EQ(500u, unthrottled_count);
}

TEST_F(MainThreadSchedulerImplTest, EnableVirtualTime) {
  EXPECT_FALSE(scheduler_->IsVirtualTimeEnabled());
  scheduler_->EnableVirtualTime(base::Time());
  EXPECT_TRUE(scheduler_->IsVirtualTimeEnabled());
  EXPECT_TRUE(scheduler_->GetVirtualTimeDomain());
}

TEST_F(MainThreadSchedulerImplTest, DisableVirtualTimeForTesting) {
  scheduler_->EnableVirtualTime(base::Time());
  scheduler_->DisableVirtualTimeForTesting();
  EXPECT_FALSE(scheduler_->IsVirtualTimeEnabled());
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimePauser) {
  scheduler_->EnableVirtualTime(base::Time());
  scheduler_->SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser(
      scheduler_.get(),
      WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant, "test");

  base::TimeTicks before = scheduler_->NowTicks();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  pauser.PauseVirtualTime();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  pauser.UnpauseVirtualTime();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  base::TimeTicks after = scheduler_->NowTicks();
  EXPECT_EQ(after, before);
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimePauserNonInstantTask) {
  scheduler_->EnableVirtualTime(base::Time());
  scheduler_->SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser(
      scheduler_.get(),
      WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant, "test");

  base::TimeTicks before = scheduler_->NowTicks();
  pauser.PauseVirtualTime();
  pauser.UnpauseVirtualTime();
  base::TimeTicks after = scheduler_->NowTicks();
  EXPECT_GT(after, before);
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimeWithOneQueueWithoutVirtualTime) {
  // This test ensures that we do not do anything strange like stopping
  // processing task queues after we encountered one task queue with
  // DoNotUseVirtualTime trait.
  scheduler_->EnableVirtualTime(base::Time());
  scheduler_->SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser(
      scheduler_.get(),
      WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant, "test");

  // Test will pass if the queue without virtual is the last one in the
  // iteration order. Create 100 of them and ensure that it is created in the
  // middle.
  std::vector<scoped_refptr<MainThreadTaskQueue>> task_queues;
  constexpr int kTaskQueueCount = 100;

  for (size_t i = 0; i < kTaskQueueCount; ++i) {
    task_queues.push_back(scheduler_->NewTaskQueue(
        MainThreadTaskQueue::QueueCreationParams(
            MainThreadTaskQueue::QueueType::kFrameThrottleable)
            .SetCanRunWhenVirtualTimePaused(i == 42)));
  }

  // This should install a fence on all queues with virtual time.
  pauser.PauseVirtualTime();

  int counter = 0;

  for (const auto& task_queue : task_queues) {
    task_queue->GetTaskRunnerWithDefaultTaskType()->PostTask(
        FROM_HERE, base::BindOnce([](int* counter) { ++*counter; }, &counter));
  }

  // Only the queue without virtual time should run, all others should be
  // blocked by their fences.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(counter, 1);

  pauser.UnpauseVirtualTime();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(counter, kTaskQueueCount);
}

TEST_F(MainThreadSchedulerImplTest, Tracing) {
  // This test sets renderer scheduler to some non-trivial state
  // (by posting tasks, creating child schedulers, etc) and converts it into a
  // traced value. This test checks that no internal checks fire during this.

  std::unique_ptr<PageSchedulerImpl> page_scheduler1 =
      CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
  scheduler_->AddPageScheduler(page_scheduler1.get());

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      CreateFrameScheduler(page_scheduler1.get(), nullptr,
                           /*is_in_embedded_frame_tree=*/false,
                           FrameScheduler::FrameType::kSubframe);

  std::unique_ptr<PageSchedulerImpl> page_scheduler2 =
      CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
  scheduler_->AddPageScheduler(page_scheduler2.get());

  std::unique_ptr<CPUTimeBudgetPool> time_budget_pool =
      scheduler_->CreateCPUTimeBudgetPoolForTesting("test");

  throttleable_task_queue()->AddToBudgetPool(base::TimeTicks(),
                                             time_budget_pool.get());

  throttleable_task_runner_->PostTask(FROM_HERE, base::BindOnce(NullTask));

  loading_task_queue()->GetTaskRunnerWithDefaultTaskType()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NullTask), base::Milliseconds(10));

  scheduler_->CreateTraceEventObjectSnapshot();
}

TEST_F(MainThreadSchedulerImplTest,
       LogIpcsPostedToDocumentsInBackForwardCache) {
  base::HistogramTester histogram_tester;

  // Start recording IPCs immediately.
  base::FieldTrialParams params;
  params["delay_before_tracking_ms"] = "0";
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kLogUnexpectedIPCPostedToBackForwardCachedDocuments,
      params);

  // Store documents inside the back-forward cache. IPCs are only tracked IFF
  // all pages are in the back-forward cache.
  PageSchedulerImpl* page_scheduler = page_scheduler_.get();
  page_scheduler->SetPageBackForwardCached(true);
  base::RunLoop().RunUntilIdle();
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(1);
    default_task_runner_->PostTask(FROM_HERE, base::DoNothing());
  }
  base::RunLoop().RunUntilIdle();

  // Adding a new page scheduler results in IPCs not being logged, as this
  // page scheduler is not in the cache.
  std::unique_ptr<PageSchedulerImpl> page_scheduler1 =
      CreatePageScheduler(nullptr, scheduler_.get(), *agent_group_scheduler_);
  scheduler_->AddPageScheduler(page_scheduler1.get());
  base::RunLoop().RunUntilIdle();
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(2);
    default_task_runner_->PostTask(FROM_HERE, base::DoNothing());
  }
  base::RunLoop().RunUntilIdle();

  // Removing an un-cached page scheduler results in IPCs being logged, as all
  // page schedulers are now in the cache.
  page_scheduler1.reset();
  base::RunLoop().RunUntilIdle();
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(3);
    default_task_runner_->PostTask(FROM_HERE, base::DoNothing());
  }
  base::RunLoop().RunUntilIdle();

  // When a page is restored from the back-forward cache, IPCs should not be
  // recorded anymore, as not all pages are in the cache.
  page_scheduler->SetPageBackForwardCached(false);
  base::RunLoop().RunUntilIdle();
  {
    base::TaskAnnotator::ScopedSetIpcHash scoped_set_ipc_hash(4);
    default_task_runner_->PostTask(FROM_HERE, base::DoNothing());
  }
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "BackForwardCache.Experimental."
          "UnexpectedIPCMessagePostedToCachedFrame.MethodHash"),
      testing::UnorderedElementsAre(base::Bucket(1, 1), base::Bucket(3, 1)));
}

void RecordingTimeTestTask(
    Vector<base::TimeTicks>* run_times,
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
  run_times->push_back(task_runner->GetMockTickClock()->NowTicks());
}

TEST_F(MainThreadSchedulerImplTest, LoadingControlTasks) {
  // Expect control loading tasks (M) to jump ahead of any regular loading
  // tasks (L).
  Vector<String> run_order;
  PostTestTasks(&run_order, "L1 L2 M1 L3 L4 M2 L5 L6");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("M1", "M2", "L1", "L2", "L3",
                                              "L4", "L5", "L6"));
}

TEST_F(MainThreadSchedulerImplTest, RequestBeginMainFrameNotExpected) {
  scheduler_->OnPendingTasksChanged(true);
  EXPECT_CALL(*page_scheduler_, RequestBeginMainFrameNotExpected(true))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler_.get());

  scheduler_->OnPendingTasksChanged(false);
  EXPECT_CALL(*page_scheduler_, RequestBeginMainFrameNotExpected(false))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler_.get());
}

TEST_F(MainThreadSchedulerImplTest,
       RequestBeginMainFrameNotExpected_MultipleCalls) {
  scheduler_->OnPendingTasksChanged(true);
  scheduler_->OnPendingTasksChanged(true);
  // Multiple calls should result in only one call.
  EXPECT_CALL(*page_scheduler_, RequestBeginMainFrameNotExpected(true))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler_.get());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(MainThreadSchedulerImplTest, PauseTimersForAndroidWebView) {
  // Tasks in some queues don't fire when the throttleable queues are paused.
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 L1 I1 T1");
  scheduler_->PauseTimersForAndroidWebView();
  EnableIdleTasks();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1", "L1", "I1"));
  // The rest queued tasks fire when the throttleable queues are resumed.
  run_order.clear();
  scheduler_->ResumeTimersForAndroidWebView();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_order, testing::ElementsAre("T1"));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(MainThreadSchedulerImplTest, FreezesCompositorQueueWhenAllPagesFrozen) {
  main_frame_scheduler_.reset();
  page_scheduler_.reset();

  std::unique_ptr<PageScheduler> sched_1 =
      agent_group_scheduler_->CreatePageScheduler(nullptr);
  sched_1->SetPageVisible(false);
  std::unique_ptr<PageScheduler> sched_2 =
      agent_group_scheduler_->CreatePageScheduler(nullptr);
  sched_2->SetPageVisible(false);

  Vector<String> run_order;

  sched_1->SetPageVisible(false);
  sched_1->SetPageFrozen(true);
  PostTestTasks(&run_order, "D1 C1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "C1"));

  run_order.clear();
  sched_2->SetPageFrozen(true);
  PostTestTasks(&run_order, "D2 C2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D2"));

  run_order.clear();
  std::unique_ptr<PageScheduler> sched_3 =
      agent_group_scheduler_->CreatePageScheduler(nullptr);
  sched_3->SetPageVisible(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C2"));

  run_order.clear();
  PostTestTasks(&run_order, "D3 C3");
  sched_3.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D3"));

  run_order.clear();
  sched_1.reset();
  sched_2.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C3"));
}

class MainThreadSchedulerImplWithInitalVirtualTimeTest
    : public MainThreadSchedulerImplTest {
 public:
  void SetUp() override {
    CreateTestTaskRunner();
    auto main_thread_scheduler =
        std::make_unique<MainThreadSchedulerImplForTest>(
            base::sequence_manager::SequenceManagerForTest::Create(
                nullptr, test_task_runner_,
                test_task_runner_->GetMockTickClock(),
                base::sequence_manager::SequenceManager::Settings::Builder()
                    .SetRandomisedSamplingEnabled(true)
                    .SetPrioritySettings(CreatePrioritySettings())
                    .Build()));
    main_thread_scheduler->EnableVirtualTime(
        /* initial_time= */ base::Time::FromMillisecondsSinceUnixEpoch(
            1000000.0));
    main_thread_scheduler->SetVirtualTimePolicy(
        VirtualTimeController::VirtualTimePolicy::kPause);
    Initialize(std::move(main_thread_scheduler));
  }
};

TEST_F(MainThreadSchedulerImplWithInitalVirtualTimeTest, VirtualTimeOverride) {
  EXPECT_TRUE(scheduler_->IsVirtualTimeEnabled());
  EXPECT_EQ(VirtualTimeController::VirtualTimePolicy::kPause,
            scheduler_->GetVirtualTimePolicyForTest());
  EXPECT_EQ(base::Time::Now(),
            base::Time::FromMillisecondsSinceUnixEpoch(1000000.0));
}

TEST_F(MainThreadSchedulerImplTest, CompositingAfterInput) {
  Vector<String> run_order;

  // Input tasks don't cause compositor tasks to be prioritized unless an input
  // event was handled.
  PostTestTasks(&run_order, "P1 T1 C1 C2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "T1", "C1", "C2"));
  run_order.clear();

  // Tasks with input events cause compositor tasks to be prioritized until a
  // BeginMainFrame runs.
  PostTestTasks(&run_order, "T1 P1 PD1 C1 C2 CM1 C2 T2 CM2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "PD1", "C1", "C2", "CM1",
                                              "T1", "C2", "T2", "CM2"));
  run_order.clear();

  // Input tasks and compositor tasks will be interleaved because they have the
  // same priority.
  PostTestTasks(&run_order, "T1 PD1 C1 PD2 C2 CM1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("PD1", "C1", "PD2", "C2", "CM1", "T1"));
  run_order.clear();
}

TEST_F(MainThreadSchedulerImplTest,
       CompositorNotPrioritizedAfterContinuousInput) {
  Vector<String> run_order;

  // rAF-aligned input should not cause the next frame to be prioritized.
  PostTestTasks(&run_order, "P1 T1 CI1 T2 CI2 T3 CM1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "T1", "CI1", "T2", "CI2",
                                              "T3", "CM1"));
  run_order.clear();

  // Continuous input that runs outside of rAF should not cause the next frame
  // to be prioritized.
  PostTestTasks(&run_order, "PC1 T1 CM1 T2 CM2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("PC1", "T1", "CM1", "T2", "CM2"));
  run_order.clear();
}

TEST_F(MainThreadSchedulerImplTest, TaskQueueReferenceClearedOnShutdown) {
  // Ensure that the scheduler clears its references to a task queue after
  // |shutdown| and doesn't try to update its policies.
  scoped_refptr<MainThreadTaskQueue> queue1 =
      scheduler_->NewThrottleableTaskQueueForTest(nullptr);
  scoped_refptr<MainThreadTaskQueue> queue2 =
      scheduler_->NewThrottleableTaskQueueForTest(nullptr);

  EXPECT_TRUE(queue1->IsQueueEnabled());
  EXPECT_TRUE(queue2->IsQueueEnabled());

  scheduler_->OnShutdownTaskQueue(queue1);

  auto pause_handle = scheduler_->PauseScheduler();

  // queue2 should be disabled, as it is a regular queue and nothing should
  // change for queue1 because it was shut down.
  EXPECT_TRUE(queue1->IsQueueEnabled());
  EXPECT_FALSE(queue2->IsQueueEnabled());
}

TEST_F(MainThreadSchedulerImplTest, MicrotaskCheckpointTiming) {
  base::RunLoop().RunUntilIdle();

  base::TimeTicks start_time = Now();
  RecordingTaskTimeObserver observer;

  const base::TimeDelta kTaskTime = base::Milliseconds(100);
  const base::TimeDelta kMicrotaskTime = base::Milliseconds(200);
  default_task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&MainThreadSchedulerImplTest::AdvanceMockTickClockBy,
                    base::Unretained(this), kTaskTime));
  scheduler_->on_microtask_checkpoint_ =
      WTF::BindOnce(&MainThreadSchedulerImplTest::AdvanceMockTickClockBy,
                    base::Unretained(this), kMicrotaskTime);

  scheduler_->AddTaskTimeObserver(&observer);
  base::RunLoop().RunUntilIdle();
  scheduler_->RemoveTaskTimeObserver(&observer);

  // Expect that the duration of microtask is counted as a part of the preceding
  // task.
  ASSERT_EQ(1u, observer.result().size());
  EXPECT_EQ(start_time, observer.result().front().first);
  EXPECT_EQ(start_time + kTaskTime + kMicrotaskTime,
            observer.result().front().second);
}

TEST_F(MainThreadSchedulerImplTest, NonWakingTaskQueue) {
  std::vector<std::pair<std::string, base::TimeTicks>> log;
  base::TimeTicks start = scheduler_->NowTicks();

  scheduler_->DefaultTaskQueue()->GetTaskRunnerWithDefaultTaskType()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::pair<std::string, base::TimeTicks>>* log,
             const base::TickClock* clock) {
            log->emplace_back("regular (immediate)", clock->NowTicks());
          },
          &log, scheduler_->GetTickClock()));
  scheduler_->NonWakingTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<std::pair<std::string, base::TimeTicks>>* log,
             const base::TickClock* clock) {
            log->emplace_back("non-waking", clock->NowTicks());
          },
          &log, scheduler_->GetTickClock()),
      base::Seconds(3));
  scheduler_->DefaultTaskQueue()
      ->GetTaskRunnerWithDefaultTaskType()
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              [](std::vector<std::pair<std::string, base::TimeTicks>>* log,
                 const base::TickClock* clock) {
                log->emplace_back("regular (delayed)", clock->NowTicks());
              },
              &log, scheduler_->GetTickClock()),
          base::Seconds(5));

  test_task_runner_->FastForwardUntilNoTasksRemain();

  // Check that the non-waking task runner didn't generate an unnecessary
  // wake-up.
  // Note: the exact order of these tasks is not fixed and depends on the time
  // domain iteration order.
  EXPECT_THAT(
      log, testing::UnorderedElementsAre(
               std::make_pair("regular (immediate)", start),
               std::make_pair("non-waking", start + base::Seconds(5)),
               std::make_pair("regular (delayed)", start + base::Seconds(5))));
}

class BestEffortPriorityForFindInPageExperimentTest
    : public MainThreadSchedulerImplTest {
 public:
  BestEffortPriorityForFindInPageExperimentTest()
      : MainThreadSchedulerImplTest({kBestEffortPriorityForFindInPage}, {}) {}
};

TEST_F(BestEffortPriorityForFindInPageExperimentTest,
       FindInPageTasksAreBestEffortPriorityUnderExperiment) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "F1 D1 F2 D2 F3 D3");
  EnableIdleTasks();
  EXPECT_EQ(scheduler_->find_in_page_priority(),
            TaskPriority::kBestEffortPriority);
  base::RunLoop().RunUntilIdle();
  // Find-in-page tasks have "best-effort" priority, so they will be done after
  // the default tasks (which have normal priority).
  EXPECT_THAT(run_order,
              testing::ElementsAre("D1", "D2", "D3", "F1", "F2", "F3"));
}

TEST_F(MainThreadSchedulerImplTest, FindInPageTasksAreVeryHighPriority) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 F1 F2 F3");
  EnableIdleTasks();
  EXPECT_EQ(
      scheduler_->find_in_page_priority(),
      FindInPageBudgetPoolController::kFindInPageBudgetNotExhaustedPriority);
  base::RunLoop().RunUntilIdle();
  // Find-in-page tasks have very high task priority, so we will do them before
  // the default tasks.
  EXPECT_THAT(run_order,
              testing::ElementsAre("F1", "F2", "F3", "D1", "D2", "D3"));
}

TEST_F(MainThreadSchedulerImplTest, FindInPageTasksChangeToNormalPriority) {
  EXPECT_EQ(
      scheduler_->find_in_page_priority(),
      FindInPageBudgetPoolController::kFindInPageBudgetNotExhaustedPriority);
  EnableIdleTasks();
  // Simulate a really long find-in-page task that takes 30% of CPU time
  // (300ms out of 1000 ms).
  base::TimeTicks task_start_time = Now();
  base::TimeTicks task_end_time = task_start_time + base::Milliseconds(300);
  FakeTask fake_task;
  fake_task.set_enqueue_order(
      base::sequence_manager::EnqueueOrder::FromIntForTesting(42));
  FakeTaskTiming task_timing(task_start_time, task_end_time);
  scheduler_->OnTaskStarted(find_in_page_task_queue(), fake_task, task_timing);
  AdvanceMockTickClockTo(task_start_time + base::Milliseconds(1000));
  scheduler_->OnTaskCompleted(find_in_page_task_queue()->AsWeakPtr(), fake_task,
                              &task_timing, nullptr);

  // Now the find-in-page tasks have normal priority (same priority as default
  // tasks, so we will do them in order).
  EXPECT_EQ(scheduler_->find_in_page_priority(),
            FindInPageBudgetPoolController::kFindInPageBudgetExhaustedPriority);
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 D2 F1 F2 D3 F3");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("D1", "D2", "F1", "F2", "D3", "F3"));
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorStaysAtNormalPriority) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  DoMainFrame();
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_FirstCompositorTaskSetToVeryHighPriority) {
  AdvanceTimeWithTask(kDelayForHighPriorityRendering);

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 C2 P1");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "C1", "C2", "D1", "D2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // The next compositor task is the BeginMainFrame, after which the priority is
  // returned to normal.
  PostTestTasks(&run_order, "CM");
  base::RunLoop().RunUntilIdle();

  run_order.clear();
  PostTestTasks(&run_order, "C1 D1 D2 C2 P1");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "C1", "D1", "D2", "C2"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_FirstCompositorTaskStaysAtNormalPriority) {
  // A short task should not cause compositor tasks to be prioritized.
  AdvanceTimeWithTask(base::Milliseconds(5));

  Vector<String> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2 P1");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("P1", "D1", "C1", "D2", "C2", "I1"));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, ThrottleHandleThrottlesQueue) {
  EXPECT_FALSE(throttleable_task_queue()->IsThrottled());
  {
    MainThreadTaskQueue::ThrottleHandle handle =
        throttleable_task_queue()->Throttle();
    EXPECT_TRUE(throttleable_task_queue()->IsThrottled());
    {
      MainThreadTaskQueue::ThrottleHandle handle_2 =
          throttleable_task_queue()->Throttle();
      EXPECT_TRUE(throttleable_task_queue()->IsThrottled());
    }
    EXPECT_TRUE(throttleable_task_queue()->IsThrottled());
  }
  EXPECT_FALSE(throttleable_task_queue()->IsThrottled());
}

class PrioritizeCompositingAfterDelayTest : public MainThreadSchedulerImplTest {
 public:
  PrioritizeCompositingAfterDelayTest()
      : MainThreadSchedulerImplTest({::base::test::FeatureRefAndParams(
            kPrioritizeCompositingAfterDelayTrials,
            {{"PreFCP", "120"}, {"PostFCP", "80"}})}) {}
};

TEST_F(PrioritizeCompositingAfterDelayTest, PreFCP) {
  scheduler_->SetCurrentUseCase(UseCase::kEarlyLoading);
  AdvanceTimeWithTask(base::Milliseconds(119));
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 CM1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "D1", "CM1"));

  AdvanceTimeWithTask(base::Milliseconds(121));
  run_order.clear();
  PostTestTasks(&run_order, "D1 CM1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "CM1", "D1"));
}

TEST_F(PrioritizeCompositingAfterDelayTest, PostFCP) {
  scheduler_->SetCurrentUseCase(UseCase::kNone);
  AdvanceTimeWithTask(base::Milliseconds(79));
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 CM1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "D1", "CM1"));

  AdvanceTimeWithTask(base::Milliseconds(81));
  run_order.clear();
  PostTestTasks(&run_order, "D1 CM1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "CM1", "D1"));
}

TEST_F(PrioritizeCompositingAfterDelayTest, DuringCompositorGesture) {
  scheduler_->SetCurrentUseCase(UseCase::kCompositorGesture);
  AdvanceTimeWithTask(base::Milliseconds(99));
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 CM1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "D1", "CM1"));

  AdvanceTimeWithTask(base::Milliseconds(101));
  run_order.clear();
  PostTestTasks(&run_order, "P1 D1 CM1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("P1", "CM1", "D1"));
}

class ThreadedScrollPreventRenderingStarvationTest
    : public MainThreadSchedulerImplTest,
      public ::testing::WithParamInterface<int> {
 public:
  ThreadedScrollPreventRenderingStarvationTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kThreadedScrollPreventRenderingStarvation,
          base::FieldTrialParams(
              {{"threshold_ms", base::NumberToString(GetParam())}})}},
        {});
  }
};

TEST_P(ThreadedScrollPreventRenderingStarvationTest, CompositorPriority) {
  SimulateEnteringCompositorGestureUseCase();

  // Compositor task queues should initially have low priority.
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 C2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());

  // The priority should remain low up to the timeout.
  AdvanceTimeWithTask(base::Milliseconds(GetParam() - 1));
  // The policy has a max duration, so simulate a longer scroll (multiple
  // updates) with another scroll start.
  SimulateEnteringCompositorGestureUseCase();

  run_order.clear();
  PostTestTasks(&run_order, "D1 D2 C1 C2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());

  // Reaching the configurable delay should boost the compositor TQ priority
  // until the next frame.
  run_order.clear();
  AdvanceTimeWithTask(base::Milliseconds(1));
  PostTestTasks(&run_order, "D1 C1 D2 C2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("C1", "C2", "D1", "D2"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());

  // The next BeginMainFrame (CM1) should reset the frame delay counter so that
  // the compositor task queues drop back down to low priority.
  PostTestTasks(&run_order, "CM1");
  base::RunLoop().RunUntilIdle();

  run_order.clear();
  PostTestTasks(&run_order, "D1 C1 D2 C2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("D1", "D2", "C1", "C2"));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_P(ThreadedScrollPreventRenderingStarvationTest,
       CompositorPriorityWithRenderBlockingTaskStarvation) {
  // The starved-by-render-blocking-tasks bit isn't cleared when we change use
  // cases, so start out in scrolling use case.
  SimulateEnteringCompositorGestureUseCase();
  SimulateRenderBlockingTask(
      MainThreadSchedulerImpl::kRenderBlockingStarvationThreshold);

  // The use case will have been cleared because the policy timeout will have
  // been reached.
  SimulateEnteringCompositorGestureUseCase();

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 C1 D2 R1 C2");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());

  if (base::Milliseconds(GetParam()) >
      MainThreadSchedulerImpl::kRenderBlockingStarvationThreshold) {
    // No anti-starvation should kick in.
    EXPECT_THAT(run_order, testing::ElementsAre("R1", "D1", "D2", "C1", "C2"));

    // Advance far enough to trigger the render-blocking anti-starvation.
    run_order.clear();
    SimulateRenderBlockingTask(
        base::Milliseconds(GetParam()) -
        MainThreadSchedulerImpl::kRenderBlockingStarvationThreshold);
    SimulateEnteringCompositorGestureUseCase();

    PostTestTasks(&run_order, "D1 C1 D2 R1 C2");
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
  }

  EXPECT_THAT(run_order, testing::ElementsAre("C1", "R1", "C2", "D1", "D2"));
}

INSTANTIATE_TEST_SUITE_P(,
                         ThreadedScrollPreventRenderingStarvationTest,
                         testing::Values(100, 250, 500, 600));

TEST_F(MainThreadSchedulerImplTest, RenderBlockingTaskPriority) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 CM1 R1 R2 R3");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("R1", "R2", "R3", "D1", "CM1"));
}

TEST_F(MainThreadSchedulerImplTest, RenderBlockingAndDiscreteInput) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 CM1 R1 PD1 R2 R3");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("PD1", "CM1", "R1", "R2", "R3", "D1"));
}

TEST_F(MainThreadSchedulerImplTest, RenderBlockingStarvationPrevention) {
  SimulateRenderBlockingTask(
      MainThreadSchedulerImpl::kRenderBlockingStarvationThreshold);
  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 R1 CM1 R2 R3");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("R1", "CM1", "R2", "R3", "D1"));
}

TEST_F(MainThreadSchedulerImplTest,
       RenderBlockingStarvationPreventionDoesNotAffectCompositorGestures) {
  SimulateEnteringCompositorGestureUseCase();
  SimulateRenderBlockingTask(
      MainThreadSchedulerImpl::kRenderBlockingStarvationThreshold);

  // The use case will have been cleared because the policy timeout will have
  // been reached.
  SimulateEnteringCompositorGestureUseCase();

  Vector<String> run_order;
  PostTestTasks(&run_order, "D1 R1 CM1 R2 R3");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
  EXPECT_THAT(run_order, testing::ElementsAre("R1", "R2", "R3", "D1", "CM1"));
}

TEST_F(MainThreadSchedulerImplTest, DetachRunningTaskQueue) {
  scoped_refptr<MainThreadTaskQueue> queue =
      scheduler_->NewThrottleableTaskQueueForTest(nullptr);
  base::WeakPtr<MainThreadTaskQueue> weak_queue = queue->AsWeakPtr();
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      queue->GetTaskRunnerWithDefaultTaskType();
  queue = nullptr;

  task_runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                          weak_queue->DetachTaskQueue();
                        }));

  EXPECT_TRUE(weak_queue);
  // `queue` is deleted while running its last task, but sequence manager should
  // keep the underlying queue alive while its needed, so this shouldn't crash.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(weak_queue);
}

TEST_F(MainThreadSchedulerImplTest, PrioritizeUrgentMessageIPCTasks) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 D1 T2");
  // Default TQ tasks after this are prioritized until the there are no more
  // pending urgent messages, which happens in the "D5" task.
  default_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                   run_order.push_back("D2a");
                                   scheduler_->OnUrgentMessageReceived();
                                 }));
  default_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                   run_order.push_back("D2b");
                                   scheduler_->OnUrgentMessageReceived();
                                 }));
  default_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                   run_order.push_back("D2c");
                                   scheduler_->OnUrgentMessageProcessed();
                                 }));
  PostTestTasks(&run_order, "T3 T4 T5 D3 D4");
  default_task_runner_->PostTask(FROM_HERE, base::BindLambdaForTesting([&]() {
                                   run_order.push_back("D5");
                                   scheduler_->OnUrgentMessageProcessed();
                                 }));
  PostTestTasks(&run_order, "T6 D6");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre("T1", "D1", "T2", "D2a", "D2b", "D2c", "D3",
                                   "D4", "D5", "T3", "T4", "T5", "T6", "D6"));
}

TEST_F(MainThreadSchedulerImplTest, UrgentMessageAndCompositorPriority) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "T1 T2 D1 PD1 C1");
  // Simulate receiving an urgent message while running a BeginMainFrame to make
  // sure the policy reflects both.
  compositor_task_runner_->PostTask(FROM_HERE,
                                    base::BindLambdaForTesting([&]() {
                                      scheduler_->OnUrgentMessageReceived();
                                      DoMainFrame();
                                      run_order.push_back("CM");
                                    }));
  PostTestTasks(&run_order, "C2 C3 D2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("PD1", "C1", "CM", "D1", "D2",
                                              "T1", "T2", "C2", "C3"));
}

class DeferRendererTasksAfterInputTest
    : public MainThreadSchedulerImplTest,
      public ::testing::WithParamInterface<features::TaskDeferralPolicy>,
      public WebSchedulingTestHelper::Delegate {
 public:
  static std::string GetFieldTrialParamName(
      features::TaskDeferralPolicy policy) {
    switch (policy) {
      case features::TaskDeferralPolicy::kMinimalTypes:
        return "minimal-types";
      case features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes:
        return "non-user-blocking-deferrable-types";
      case features::TaskDeferralPolicy::kNonUserBlockingTypes:
        return "non-user-blocking-types";
      case features::TaskDeferralPolicy::kAllDeferrableTypes:
        return "all-deferrable-types";
      case features::TaskDeferralPolicy::kAllTypes:
        return "all-types";
    }
  }

  DeferRendererTasksAfterInputTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kDeferRendererTasksAfterInput,
          base::FieldTrialParams(
              {{"policy", GetFieldTrialParamName(GetParam())}})}},
        {});
  }

  void SetUp() override {
    MainThreadSchedulerImplTest::SetUp();
    web_scheduling_test_helper_ =
        std::make_unique<WebSchedulingTestHelper>(*this);
  }

  void TearDown() override {
    MainThreadSchedulerImplTest::TearDown();
    web_scheduling_test_helper_.reset();
  }

  FrameOrWorkerScheduler& GetFrameOrWorkerScheduler() override {
    return *main_frame_scheduler_.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType task_type) override {
    return main_frame_scheduler_->GetTaskRunner(task_type);
  }

 protected:
  using TestTaskSpecEntry = WebSchedulingTestHelper::TestTaskSpecEntry;
  using WebSchedulingParams = WebSchedulingTestHelper::WebSchedulingParams;

  std::unique_ptr<WebSchedulingTestHelper> web_scheduling_test_helper_;
};

TEST_P(DeferRendererTasksAfterInputTest, TaskDeferral) {
  Vector<String> run_order;

  // Simulate a long idle period starting.
  scheduler_->BeginFrameNotExpectedSoon();

  // Post potentially deferrable tasks.
  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "F1", .type_info = TaskType::kDOMManipulation},
      {.descriptor = "F2", .type_info = TaskType::kPostedMessage},
      {.descriptor = "F3", .type_info = TaskType::kInternalMediaRealTime},
      {.descriptor = "F4", .type_info = TaskType::kJavascriptTimerImmediate},
      {.descriptor = "BG1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kBackgroundPriority})},
      {.descriptor = "UV1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UB1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  // The input task will run first and change the UseCase.
  PostTestTasks(&run_order, "PD1 D1 I1");

  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);

  // The main frame task will reset the UseCase and unblock the deferred queues.
  PostTestTasks(&run_order, "CM1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);

  switch (GetParam()) {
    case features::TaskDeferralPolicy::kMinimalTypes:
      EXPECT_THAT(run_order,
                  testing::ElementsAre("PD1", "UB1", "F2", "F3", "F4", "UV1",
                                       "D1", "CM1", "F1", "BG1", "I1"));
      break;
    case features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes:
      EXPECT_THAT(run_order,
                  testing::ElementsAre("PD1", "UB1", "F3", "D1", "CM1", "F1",
                                       "F2", "F4", "UV1", "BG1", "I1"));
      break;
    case features::TaskDeferralPolicy::kAllDeferrableTypes:
      EXPECT_THAT(run_order,
                  testing::ElementsAre("PD1", "F3", "D1", "CM1", "UB1", "F1",
                                       "F2", "F4", "UV1", "BG1", "I1"));
      break;
    case features::TaskDeferralPolicy::kNonUserBlockingTypes:
      EXPECT_THAT(run_order,
                  testing::ElementsAre("PD1", "UB1", "D1", "CM1", "F1", "F2",
                                       "F3", "F4", "UV1", "BG1", "I1"));
      break;
    case features::TaskDeferralPolicy::kAllTypes:
      EXPECT_THAT(run_order,
                  testing::ElementsAre("PD1", "D1", "CM1", "UB1", "F1", "F2",
                                       "F3", "F4", "UV1", "BG1", "I1"));
      break;
  }
}

TEST_P(DeferRendererTasksAfterInputTest, DynamicPriorityTaskDeferral) {
  Vector<String> run_order;

  PostTestTasks(&run_order, "PD1");
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);
  EXPECT_THAT(run_order, testing::ElementsAre("PD1"));

  // Post potentially deferrable tasks.
  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "UV1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserVisiblePriority})},
      {.descriptor = "UB1",
       .type_info = WebSchedulingParams(
           {.queue_type = WebSchedulingQueueType::kTaskQueue,
            .priority = WebSchedulingPriority::kUserBlockingPriority})}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  web_scheduling_test_helper_
      ->GetWebSchedulingTaskQueue(WebSchedulingQueueType::kTaskQueue,
                                  WebSchedulingPriority::kUserBlockingPriority)
      ->SetPriority(WebSchedulingPriority::kBackgroundPriority);
  web_scheduling_test_helper_
      ->GetWebSchedulingTaskQueue(WebSchedulingQueueType::kTaskQueue,
                                  WebSchedulingPriority::kUserVisiblePriority)
      ->SetPriority(WebSchedulingPriority::kUserBlockingPriority);

  // Run whatever isn't deferrable.
  base::RunLoop().RunUntilIdle();

  // The main frame task will reset the UseCase and unblock the deferred queues.
  PostTestTasks(&run_order, "CM1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);

  // UB1, which is now background priority, should be deferred by every policy.
  // UV1, which is now user-blocking priority, will should only be deferred for
  // the all-types and all-deferrable policies.
  switch (GetParam()) {
    case features::TaskDeferralPolicy::kMinimalTypes:
    case features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes:
    case features::TaskDeferralPolicy::kNonUserBlockingTypes:
      EXPECT_THAT(run_order, testing::ElementsAre("PD1", "UV1", "CM1", "UB1"));
      break;
    case features::TaskDeferralPolicy::kAllDeferrableTypes:
    case features::TaskDeferralPolicy::kAllTypes:
      EXPECT_THAT(run_order, testing::ElementsAre("PD1", "CM1", "UV1", "UB1"));
      break;
  }
}

TEST_P(DeferRendererTasksAfterInputTest, TaskDeferralTimeout) {
  Vector<String> run_order;

  Vector<TestTaskSpecEntry> test_spec = {
      {.descriptor = "F1", .type_info = TaskType::kDOMManipulation}};
  web_scheduling_test_helper_->PostTestTasks(&run_order, test_spec);

  PostTestTasks(&run_order, "PD1 D1");
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);
  EXPECT_THAT(run_order, testing::ElementsAre("PD1", "D1"));

  // Simulate reaching the discrete input deferral timeout.
  run_order.clear();
  test_task_runner_->FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
  EXPECT_THAT(run_order, testing::ElementsAre("F1"));
}

TEST_P(DeferRendererTasksAfterInputTest,
       DiscreteInputUseCaseDependsOnFrameRequested) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scheduler_->DidHandleInputEventOnMainThread(
            FakeInputEvent(WebInputEvent::Type::kMouseUp),
            WebInputEventResult::kHandledApplication,
            /*frame_requested=*/false);
      }));
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);

  input_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scheduler_->DidHandleInputEventOnMainThread(
            FakeInputEvent(WebInputEvent::Type::kMouseUp),
            WebInputEventResult::kHandledApplication,
            /*frame_requested=*/true);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);
}

TEST_P(DeferRendererTasksAfterInputTest,
       DiscreteInputUseCaseIgnoresContinuous) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scheduler_->DidHandleInputEventOnMainThread(
            FakeInputEvent(WebInputEvent::Type::kMouseMove),
            WebInputEventResult::kHandledApplication,
            /*frame_requested=*/false);
      }));
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
}

TEST_P(DeferRendererTasksAfterInputTest, UseCaseTimeout) {
  Vector<String> run_order;
  PostTestTasks(&run_order, "PD1");
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);

  test_task_runner_->AdvanceMockTickClock(
      UserModel::kDiscreteInputResponseDeadline);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
}

TEST_P(DeferRendererTasksAfterInputTest, TouchStartAndDiscreteInput) {
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::Type::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(ForceUpdatePolicyAndGetCurrentUseCase(), UseCase::kTouchstart);

  Vector<String> run_order;
  PostTestTasks(&run_order, "PD1");
  base::RunLoop().RunUntilIdle();
  // The touchstart use case should take precedent.
  EXPECT_EQ(CurrentUseCase(), UseCase::kTouchstart);
  EXPECT_THAT(run_order, testing::ElementsAre("PD1"));
}

TEST_P(DeferRendererTasksAfterInputTest, DiscreteInputDuringContinuousGesture) {
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseDown,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::Type::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kMainThreadCustomInputHandling);

  // Actually handling the mousedown event should transition to discrete input.
  input_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scheduler_->DidHandleInputEventOnMainThread(
            FakeInputEvent(WebInputEvent::Type::kMouseDown,
                           blink::WebInputEvent::kLeftButtonDown),
            WebInputEventResult::kHandledApplication,
            /*frame_requested=*/true);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);

  // Handling the mousemove shouldn't change anything. Note: This is necessary
  // to bring the pending event count to 0 so that the use case gets cleared
  // after the second timeout.
  input_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scheduler_->DidHandleInputEventOnMainThread(
            FakeInputEvent(WebInputEvent::Type::kMouseMove,
                           blink::WebInputEvent::kLeftButtonDown),
            WebInputEventResult::kHandledApplication,
            /*frame_requested=*/true);
      }));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);

  // Fast forwarding past the discrete input policy timeout should then fall
  // back to the previous policy, since that has a longer timeout.
  EXPECT_LT(UserModel::kDiscreteInputResponseDeadline,
            UserModel::kGestureEstimationLimit);
  test_task_runner_->AdvanceMockTickClock(
      UserModel::kDiscreteInputResponseDeadline);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kMainThreadCustomInputHandling);

  // Fast forwarding past the continuous gesture timeout should then reset the
  // use case.
  test_task_runner_->AdvanceMockTickClock(
      UserModel::kGestureEstimationLimit -
      UserModel::kDiscreteInputResponseDeadline);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kNone);
}

TEST_P(DeferRendererTasksAfterInputTest, DiscreteInputDoesNotChangeRAILMode) {
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameContentfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsWaitingForMainFrameMeaningfulPaint)
      .WillByDefault(Return(true));
  ON_CALL(*page_scheduler_, IsMainFrameLoading).WillByDefault(Return(true));
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(ForceUpdatePolicyAndGetCurrentUseCase(), UseCase::kEarlyLoading);
  EXPECT_EQ(GetRAILMode(), RAILMode::kLoad);

  Vector<String> run_order;
  PostTestTasks(&run_order, "PD1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kDiscreteInputResponse);
  EXPECT_EQ(GetRAILMode(), RAILMode::kLoad);

  PostTestTasks(&run_order, "CM1");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(CurrentUseCase(), UseCase::kEarlyLoading);
  EXPECT_EQ(GetRAILMode(), RAILMode::kLoad);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DeferRendererTasksAfterInputTest,
    testing::Values(
        features::TaskDeferralPolicy::kMinimalTypes,
        features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes,
        features::TaskDeferralPolicy::kNonUserBlockingTypes,
        features::TaskDeferralPolicy::kAllDeferrableTypes,
        features::TaskDeferralPolicy::kAllTypes),
    [](const testing::TestParamInfo<features::TaskDeferralPolicy>& info) {
      switch (info.param) {
        case features::TaskDeferralPolicy::kMinimalTypes:
          return "MinimalTypes";
        case features::TaskDeferralPolicy::kNonUserBlockingDeferrableTypes:
          return "NonUserBlockingDeferrableTypes";
        case features::TaskDeferralPolicy::kNonUserBlockingTypes:
          return "NonUserBlockingTypes";
        case features::TaskDeferralPolicy::kAllDeferrableTypes:
          return "AllDeferrableTypes";
        case features::TaskDeferralPolicy::kAllTypes:
          return "AllTypes";
      }
    });

class DiscreteInputMatchesResponsivenessMetricsTest
    : public MainThreadSchedulerImplTest,
      public ::testing::WithParamInterface<bool> {
 public:
  DiscreteInputMatchesResponsivenessMetricsTest() {
    if (GetParam()) {
      feature_list_.Reset();
      feature_list_.InitWithFeatures(
          {{features::
                kBlinkSchedulerDiscreteInputMatchesResponsivenessMetrics}},
          {});
    }
  }
};

TEST_P(DiscreteInputMatchesResponsivenessMetricsTest, TestPolicy) {
  Vector<String> run_order;

  // This will not be considered discrete iff the feature is enabled.
  input_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scheduler_->DidHandleInputEventOnMainThread(
            FakeInputEvent(WebInputEvent::Type::kMouseLeave),
            WebInputEventResult::kHandledApplication,
            /*frame_requested=*/true);
        run_order.push_back("I1");
      }));
  PostTestTasks(&run_order, "D1 D2 CM1");
  base::RunLoop().RunUntilIdle();

  if (GetParam()) {
    EXPECT_THAT(run_order, testing::ElementsAre("I1", "D1", "D2", "CM1"));
  } else {
    EXPECT_THAT(run_order, testing::ElementsAre("I1", "CM1", "D1", "D2"));
  }

  run_order.clear();
  // This shouldn't be considered discrete in either case.
  input_task_runner_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        scheduler_->DidHandleInputEventOnMainThread(
            FakeInputEvent(WebInputEvent::Type::kTouchMove),
            WebInputEventResult::kHandledApplication,
            /*frame_requested=*/true);
        run_order.push_back("I1");
      }));
  PostTestTasks(&run_order, "D1 D2 CM1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre("I1", "D1", "D2", "CM1"));
}

INSTANTIATE_TEST_SUITE_P(,
                         DiscreteInputMatchesResponsivenessMetricsTest,
                         testing::Values(true, false),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

}  // namespace main_thread_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
