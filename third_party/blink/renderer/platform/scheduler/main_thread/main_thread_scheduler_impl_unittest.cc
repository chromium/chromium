// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/sequence_manager/test/fake_task.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/launching_process_state.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_touch_event.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/common/throttling/budget_pool.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/auto_advancing_virtual_time_domain.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_task_queue_controller.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using base::sequence_manager::TaskQueue;

namespace blink {
namespace scheduler {
// To avoid symbol collisions in jumbo builds.
namespace main_thread_scheduler_impl_unittest {

using testing::Mock;
using InputEventState = WebThreadScheduler::InputEventState;
using base::sequence_manager::FakeTask;
using base::sequence_manager::FakeTaskTiming;

class FakeInputEvent : public blink::WebInputEvent {
 public:
  explicit FakeInputEvent(blink::WebInputEvent::Type event_type,
                          int modifiers = WebInputEvent::kNoModifiers)
      : WebInputEvent(sizeof(FakeInputEvent),
                      event_type,
                      modifiers,
                      WebInputEvent::GetStaticTimeStampForTests()) {}
};

class FakeTouchEvent : public blink::WebTouchEvent {
 public:
  explicit FakeTouchEvent(
      blink::WebInputEvent::Type event_type,
      DispatchType dispatch_type = blink::WebInputEvent::kBlocking)
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
      DispatchType dispatch_type = blink::WebInputEvent::kBlocking)
      : WebMouseWheelEvent(event_type,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests()) {
    this->dispatch_type = dispatch_type;
  }
};

void AppendToVectorTestTask(std::vector<std::string>* vector,
                            std::string value) {
  vector->push_back(value);
}

void AppendToVectorIdleTestTask(std::vector<std::string>* vector,
                                std::string value,
                                base::TimeTicks deadline) {
  AppendToVectorTestTask(vector, value);
}

void NullTask() {}

void AppendToVectorReentrantTask(base::SingleThreadTaskRunner* task_runner,
                                 std::vector<int>* vector,
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
    std::vector<base::TimeTicks>* deadlines,
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

void WillBeginFrameIdleTask(WebThreadScheduler* scheduler,
                            uint64_t sequence_number,
                            const base::TickClock* clock,
                            base::TimeTicks deadline) {
  scheduler->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, sequence_number, clock->NowTicks(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
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
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
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

void AnticipationTestTask(MainThreadSchedulerImpl* scheduler,
                          SimulateInputType simulate_input,
                          bool* is_anticipated_before,
                          bool* is_anticipated_after) {
  *is_anticipated_before = scheduler->IsHighPriorityWorkAnticipated();
  switch (simulate_input) {
    case SimulateInputType::kNone:
      break;

    case SimulateInputType::kTouchStart:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeTouchEvent(blink::WebInputEvent::kTouchStart),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::kTouchEnd:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::kTouchEnd),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::kGestureScrollBegin:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;

    case SimulateInputType::kGestureScrollEnd:
      scheduler->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::kGestureScrollEnd),
          InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
      break;
  }
  *is_anticipated_after = scheduler->IsHighPriorityWorkAnticipated();
}

class MainThreadSchedulerImplForTest : public MainThreadSchedulerImpl {
 public:
  using MainThreadSchedulerImpl::CompositorTaskQueue;
  using MainThreadSchedulerImpl::ControlTaskQueue;
  using MainThreadSchedulerImpl::DefaultTaskQueue;
  using MainThreadSchedulerImpl::EstimateLongestJankFreeTaskDuration;
  using MainThreadSchedulerImpl::InputTaskQueue;
  using MainThreadSchedulerImpl::OnIdlePeriodEnded;
  using MainThreadSchedulerImpl::OnIdlePeriodStarted;
  using MainThreadSchedulerImpl::OnPendingTasksChanged;
  using MainThreadSchedulerImpl::V8TaskQueue;
  using MainThreadSchedulerImpl::VirtualTimeControlTaskQueue;
  using MainThreadSchedulerImpl::SetHaveSeenABlockingGestureForTesting;

  MainThreadSchedulerImplForTest(
      std::unique_ptr<base::sequence_manager::SequenceManager> manager,
      base::Optional<base::Time> initial_virtual_time)
      : MainThreadSchedulerImpl(std::move(manager), initial_virtual_time),
        update_policy_count_(0) {}

  void UpdatePolicyLocked(UpdateType update_type) override {
    update_policy_count_++;
    MainThreadSchedulerImpl::UpdatePolicyLocked(update_type);

    std::string use_case = MainThreadSchedulerImpl::UseCaseToString(
        main_thread_only().current_use_case);
    if (main_thread_only().blocking_input_expected_soon) {
      use_cases_.push_back(use_case + " blocking input expected");
    } else {
      use_cases_.push_back(use_case);
    }
  }

  void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                        bool is_disjoint_window) override {
    MainThreadSchedulerImpl::OnQueueingTimeForWindowEstimated(
        queueing_time, is_disjoint_window);
    expected_queueing_times_.push_back(queueing_time);
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

  void RemoveRAILModeObserver(WebRAILModeObserver const* observer) {
    main_thread_only().rail_mode_observers.RemoveObserver(observer);
  }

  bool waiting_for_meaningful_paint() const {
    base::AutoLock lock(any_thread_lock_);
    return any_thread().waiting_for_meaningful_paint;
  }

  VirtualTimePolicy virtual_time_policy() const {
    return main_thread_only().virtual_time_policy;
  }

  const std::vector<base::TimeDelta>& expected_queueing_times() const {
    return expected_queueing_times_;
  }

  int update_policy_count_;
  std::vector<std::string> use_cases_;
  std::vector<base::TimeDelta> expected_queueing_times_;
};

// Lets gtest print human readable Policy values.
::std::ostream& operator<<(::std::ostream& os, const UseCase& use_case) {
  return os << MainThreadSchedulerImpl::UseCaseToString(use_case);
}

class MainThreadSchedulerImplTest : public testing::Test {
 public:
  MainThreadSchedulerImplTest(std::vector<base::Feature> features_to_enable,
                              std::vector<base::Feature> features_to_disable) {
    feature_list_.InitWithFeatures(features_to_enable, features_to_disable);
  }

  MainThreadSchedulerImplTest()
      : MainThreadSchedulerImplTest({kHighPriorityInputOnMainThread},
                                    {kPrioritizeCompositingAfterInput}) {}

  ~MainThreadSchedulerImplTest() override = default;

  void SetUp() override {
    CreateTestTaskRunner();
    Initialize(std::make_unique<MainThreadSchedulerImplForTest>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, test_task_runner_->GetMockTickClock()),
        base::nullopt));
  }

  void CreateTestTaskRunner() {
    test_task_runner_ = base::WrapRefCounted(new base::TestMockTimeTaskRunner(
        base::TestMockTimeTaskRunner::Type::kBoundToThread));
    // A null clock triggers some assertions.
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(5));
  }

  void Initialize(std::unique_ptr<MainThreadSchedulerImplForTest> scheduler) {
    scheduler_ = std::move(scheduler);

    if (kLaunchingProcessIsBackgrounded) {
      scheduler_->SetRendererBackgrounded(false);
      // Reset the policy count as foregrounding would force an initial update.
      scheduler_->update_policy_count_ = 0;
      scheduler_->use_cases_.clear();
    }

    default_task_runner_ = scheduler_->DefaultTaskQueue()->task_runner();
    compositor_task_runner_ = scheduler_->CompositorTaskQueue()->task_runner();
    input_task_runner_ = scheduler_->InputTaskQueue()->task_runner();
    idle_task_runner_ = scheduler_->IdleTaskRunner();
    v8_task_runner_ = scheduler_->V8TaskQueue()->task_runner();

    page_scheduler_ =
        std::make_unique<PageSchedulerImpl>(nullptr, scheduler_.get());
    main_frame_scheduler_ =
        FrameSchedulerImpl::Create(page_scheduler_.get(), nullptr, nullptr,
                                   FrameScheduler::FrameType::kMainFrame);

    auto* frame_task_queue_controller =
        main_frame_scheduler_->FrameTaskQueueControllerForTest();
    loading_task_queue_ = frame_task_queue_controller->LoadingTaskQueue();
    loading_control_task_runner_ =
        frame_task_queue_controller->LoadingControlTaskQueue()->task_runner();
    auto queue_traits = main_frame_scheduler_->ThrottleableTaskQueueTraits();
    timer_task_queue_ =
        frame_task_queue_controller->NonLoadingTaskQueue(queue_traits);
    timer_task_runner_ = timer_task_queue_->task_runner();
  }

  void TearDown() override {
    main_frame_scheduler_.reset();
    page_scheduler_.reset();
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
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidCommitFrameToCompositor();
  }

  void DoMainFrameOnCriticalPath() {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
  }

  void ForceBlockingInputToBeExpectedSoon() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollEnd),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    test_task_runner_->AdvanceMockTickClock(
        priority_escalation_after_input_duration() * 2);
    scheduler_->ForceUpdatePolicy();
  }

  void SimulateExpensiveTasks(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    // Simulate a bunch of expensive tasks.
    for (int i = 0; i < 10; i++) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&base::TestMockTimeTaskRunner::AdvanceMockTickClock,
                         test_task_runner_,
                         base::TimeDelta::FromMilliseconds(500)));
    }
    test_task_runner_->FastForwardUntilNoTasksRemain();
  }

  enum class TouchEventPolicy {
    kSendTouchStart,
    kDontSendTouchStart,
  };

  void SimulateCompositorGestureStart(TouchEventPolicy touch_event_policy) {
    if (touch_event_policy == TouchEventPolicy::kSendTouchStart) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeTouchEvent(blink::WebInputEvent::kTouchStart),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    }
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  }

  // Simulate a gesture where there is an active compositor scroll, but no
  // scroll updates are generated. Instead, the main thread handles
  // non-canceleable touch events, making this an effectively main thread
  // driven gesture.
  void SimulateMainThreadGestureWithoutScrollUpdates() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeTouchEvent(blink::WebInputEvent::kTouchStart),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  }

  // Simulate a gesture where the main thread handles touch events but does not
  // preventDefault(), allowing the gesture to turn into a compositor driven
  // gesture. This function also verifies the necessary policy updates are
  // scheduled.
  void SimulateMainThreadGestureWithoutPreventDefault() {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeTouchEvent(blink::WebInputEvent::kTouchStart),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    // Touchstart policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kTouchstart, ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureTapCancel),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    // Main thread gesture policy update.
    EXPECT_TRUE(scheduler_->PolicyNeedsUpdateForTesting());
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling,
              ForceUpdatePolicyAndGetCurrentUseCase());
    EXPECT_FALSE(scheduler_->PolicyNeedsUpdateForTesting());

    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchScrollStarted),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
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
          FakeTouchEvent(blink::WebInputEvent::kTouchStart),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeTouchEvent(blink::WebInputEvent::kTouchStart),
          WebInputEventResult::kHandledSystem);

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::kTouchMove),
          WebInputEventResult::kHandledSystem);

      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(blink::WebInputEvent::kTouchMove),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(blink::WebInputEvent::kTouchMove),
          WebInputEventResult::kHandledSystem);
    }
    if (gesture_type != blink::WebInputEvent::kUndefined) {
      scheduler_->DidHandleInputEventOnCompositorThread(
          FakeInputEvent(gesture_type),
          InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
      scheduler_->DidHandleInputEventOnMainThread(
          FakeInputEvent(gesture_type), WebInputEventResult::kHandledSystem);
    }
  }

  void SimulateMainThreadInputHandlingCompositorTask(
      base::TimeDelta begin_main_frame_duration) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
    test_task_runner_->AdvanceMockTickClock(begin_main_frame_duration);
    scheduler_->DidHandleInputEventOnMainThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
        WebInputEventResult::kHandledApplication);
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

  void SimulateTimerTask(base::TimeDelta duration) {
    test_task_runner_->AdvanceMockTickClock(duration);
    simulate_timer_task_ran_ = true;
  }

  void EnableIdleTasks() { DoMainFrame(); }

  UseCase CurrentUseCase() {
    return scheduler_->main_thread_only().current_use_case;
  }

  UseCase ForceUpdatePolicyAndGetCurrentUseCase() {
    scheduler_->ForceUpdatePolicy();
    return scheduler_->main_thread_only().current_use_case;
  }

  v8::RAILMode GetRAILMode() {
    return scheduler_->main_thread_only().current_policy.rail_mode();
  }

  bool BeginFrameNotExpectedSoon() {
    return scheduler_->main_thread_only().begin_frame_not_expected_soon;
  }

  bool BlockingInputExpectedSoon() {
    return scheduler_->main_thread_only().blocking_input_expected_soon;
  }

  bool HaveSeenABeginMainframe() {
    return scheduler_->main_thread_only().have_seen_a_begin_main_frame;
  }

  bool LoadingTasksSeemExpensive() {
    return scheduler_->main_thread_only().loading_tasks_seem_expensive;
  }

  bool TimerTasksSeemExpensive() {
    return scheduler_->main_thread_only().timer_tasks_seem_expensive;
  }

  base::TimeTicks EstimatedNextFrameBegin() {
    return scheduler_->main_thread_only().estimated_next_frame_begin;
  }

  bool HaveSeenABlockingGesture() {
    base::AutoLock lock(scheduler_->any_thread_lock_);
    return scheduler_->any_thread().have_seen_a_blocking_gesture;
  }

  void AdvanceTimeWithTask(double duration_seconds) {
    base::TimeDelta duration = base::TimeDelta::FromSecondsD(duration_seconds);
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
    scheduler_->OnTaskStarted(fake_queue.get(), FakeTask(),
                              FakeTaskTiming(start, base::TimeTicks()));
    std::move(task).Run();
    base::TimeTicks end = Now();
    scheduler_->OnTaskCompleted(fake_queue.get(), FakeTask(),
                                FakeTaskTiming(start, end));
  }

  void RunSlowCompositorTask() {
    // Run a long compositor task so that compositor tasks appear to be running
    // slow and thus compositor tasks will not be prioritized.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MainThreadSchedulerImplTest::SimulateMainThreadCompositorTask,
            base::Unretained(this), base::TimeDelta::FromMilliseconds(1000)));
    base::RunLoop().RunUntilIdle();
  }

  // Helper for posting several tasks of specific types. |task_descriptor| is a
  // string with space delimited task identifiers. The first letter of each
  // task identifier specifies the task type:
  // - 'D': Default task
  // - 'C': Compositor task
  // - 'P': Input task
  // - 'L': Loading task
  // - 'M': Loading Control task
  // - 'I': Idle task
  // - 'T': Timer task
  // - 'V': kV8 task
  void PostTestTasks(std::vector<std::string>* run_order,
                     const std::string& task_descriptor) {
    std::istringstream stream(task_descriptor);
    while (!stream.eof()) {
      std::string task;
      stream >> task;
      switch (task[0]) {
        case 'D':
          default_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        case 'C':
          compositor_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        case 'P':
          input_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        case 'L':
          loading_task_queue_->task_runner()->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        case 'M':
          loading_control_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        case 'I':
          idle_task_runner_->PostIdleTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorIdleTestTask, run_order, task));
          break;
        case 'T':
          timer_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        case 'V':
          v8_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&AppendToVectorTestTask, run_order, task));
          break;
        default:
          NOTREACHED();
      }
    }
  }

 protected:
  static base::TimeDelta priority_escalation_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kGestureEstimationLimitMillis);
  }

  static base::TimeDelta subsequent_input_expected_after_input_duration() {
    return base::TimeDelta::FromMilliseconds(
        UserModel::kExpectSubsequentGestureMillis);
  }

  static base::TimeDelta maximum_idle_period_duration() {
    return base::TimeDelta::FromMilliseconds(
        IdleHelper::kMaximumIdlePeriodMillis);
  }

  static base::TimeDelta end_idle_when_hidden_delay() {
    return base::TimeDelta::FromMilliseconds(
        MainThreadSchedulerImpl::kEndIdleWhenHiddenDelayMillis);
  }

  static base::TimeDelta rails_response_time() {
    return base::TimeDelta::FromMilliseconds(
        MainThreadSchedulerImpl::kRailsResponseTimeMillis);
  }

  template <typename E>
  static void CallForEachEnumValue(E first,
                                   E last,
                                   const char* (*function)(E)) {
    for (E val = first; val < last;
         val = static_cast<E>(static_cast<int>(val) + 1)) {
      (*function)(val);
    }
  }

  static void CheckAllUseCaseToString() {
    CallForEachEnumValue<UseCase>(UseCase::kFirstUseCase, UseCase::kCount,
                                  &MainThreadSchedulerImpl::UseCaseToString);
  }

  static scoped_refptr<TaskQueue> ThrottleableTaskQueue(
      FrameSchedulerImpl* scheduler) {
    auto* frame_task_queue_controller =
        scheduler->FrameTaskQueueControllerForTest();
    auto queue_traits = FrameSchedulerImpl::ThrottleableTaskQueueTraits();
    return frame_task_queue_controller->NonLoadingTaskQueue(queue_traits);
  }

  QueueingTimeEstimator* queueing_time_estimator() {
    return &scheduler_->queueing_time_estimator_;
  }

  base::test::ScopedFeatureList feature_list_;

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;

  std::unique_ptr<MainThreadSchedulerImplForTest> scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> main_frame_scheduler_;

  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  scoped_refptr<TaskQueue> loading_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> loading_control_task_runner_;
  scoped_refptr<SingleThreadIdleTaskRunner> idle_task_runner_;
  scoped_refptr<TaskQueue> timer_task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> timer_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> v8_task_runner_;
  bool simulate_timer_task_ran_;
  uint64_t next_begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;

  DISALLOW_COPY_AND_ASSIGN(MainThreadSchedulerImplTest);
};

class MainThreadSchedulerImplTestWithoutTouchstartInputHeuristics
    : public MainThreadSchedulerImplTest {
 public:
  MainThreadSchedulerImplTestWithoutTouchstartInputHeuristics()
      : MainThreadSchedulerImplTest({kDisableTouchstartInputHeuristics,
                                     kDisableNonTouchstartInputHeuristics},
                                    {kPrioritizeCompositingAfterInput}) {}
  ~MainThreadSchedulerImplTestWithoutTouchstartInputHeuristics() override =
      default;
};

class MainThreadSchedulerImplTestWithoutNonTouchstartInputHeuristics
    : public MainThreadSchedulerImplTest {
 public:
  MainThreadSchedulerImplTestWithoutNonTouchstartInputHeuristics()
      : MainThreadSchedulerImplTest({kDisableNonTouchstartInputHeuristics},
                                    {kPrioritizeCompositingAfterInput}) {}
  ~MainThreadSchedulerImplTestWithoutNonTouchstartInputHeuristics() override =
      default;
};

TEST_F(MainThreadSchedulerImplTest, TestPostDefaultTask) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 D2 D3 D4");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("D3"), std::string("D4")));
}

TEST_F(MainThreadSchedulerImplTest, TestPostDefaultAndCompositor) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1 P1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::Contains("D1"));
  EXPECT_THAT(run_order, testing::Contains("C1"));
  EXPECT_THAT(run_order, testing::Contains("P1"));
}

TEST_F(MainThreadSchedulerImplTest, TestRentrantTask) {
  int count = 0;
  std::vector<int> run_order;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(AppendToVectorReentrantTask,
                                base::RetainedRef(default_task_runner_),
                                &run_order, &count, 5));
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order, testing::ElementsAre(0, 1, 2, 3, 4));
}

TEST_F(MainThreadSchedulerImplTest, TestPostIdleTask) {
  int run_count = 0;
  base::TimeTicks expected_deadline =
      Now() + base::TimeDelta::FromMilliseconds(2300);
  base::TimeTicks deadline_in_task;

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(100));
  idle_task_runner_->PostIdleTask(
      FROM_HERE, base::BindOnce(&IdleTestTask, &run_count, &deadline_in_task));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run yet as no WillBeginFrame.

  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Shouldn't run as no DidCommitFrameToCompositor.

  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(1200));
  scheduler_->DidCommitFrameToCompositor();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // We missed the deadline.

  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(800));
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
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  DoMainFrame();

  // End the idle period early (after 500ms), and send a WillBeginFrame which
  // specifies that the next idle period should end 1000ms from now.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(500));
  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);  // Not currently in an idle period.

  // Trigger the start of the idle period before the task to end the previous
  // idle period has been triggered.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(400));
  scheduler_->DidCommitFrameToCompositor();

  // Post a task which simulates running until after the previous end idle
  // period delayed task was scheduled for
  scheduler_->DefaultTaskQueue()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(NullTask));
  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(300));
  EXPECT_EQ(1, run_count);  // We should still be in the new idle period.
}

TEST_F(MainThreadSchedulerImplTest, TestDefaultPolicy) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 P1 C1 D2 P2 C2");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // High-priority input is enabled and input tasks are processed first.
  // One compositing event is prioritized after an input event but still
  // has lower priority than input event.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("P1"), std::string("P2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("C1"), std::string("D2"),
                                   std::string("C2"), std::string("I1")));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestDefaultPolicyWithSlowCompositor) {
  RunSlowCompositorTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 P1 D2 C2");

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Even with slow compositor input tasks are handled first.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("P1"), std::string("L1"),
                                   std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("C1"),
                                   std::string("C2"), std::string("I1")));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTestWithoutNonTouchstartInputHeuristics,
       TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("C1"), std::string("D2"),
                                   std::string("C2"), std::string("I1")));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutScrollUpdates) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureWithoutScrollUpdates();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1")));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutPreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureWithoutPreventDefault();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("C1"),
                                   std::string("C2"), std::string("I1")));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_LongGestureDuration) {
  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  base::TimeTicks loop_end_time =
      Now() + base::TimeDelta::FromMilliseconds(
                  UserModel::kMedianGestureDurationMillis * 2);

  // The UseCase::kCompositorGesture usecase initially deprioritizes
  // compositor tasks (see
  // TestCompositorPolicy_CompositorHandlesInput_WithTouchHandler) but if the
  // gesture is long enough, compositor tasks get prioritized again.
  while (Now() < loop_end_time) {
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
    test_task_runner_->AdvanceMockTickClock(
        base::TimeDelta::FromMilliseconds(16));
    base::RunLoop().RunUntilIdle();
  }

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2")));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_CompositorHandlesInput_WithoutTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("C1"),
                                   std::string("C2"), std::string("I1")));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1")));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_WithoutTouchHandler) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  EnableIdleTasks();
  SimulateMainThreadGestureStart(TouchEventPolicy::kDontSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1")));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_PreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledApplication);
  base::RunLoop().RunUntilIdle();
  // Because the main thread is performing custom input handling, we let all
  // tasks run. However compositing tasks are still given priority.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("L1"), std::string("D1"),
                                   std::string("D2"), std::string("I1")));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTest,
    TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_NoPreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();
  // Because we are still waiting for the touchstart to be processed,
  // non-essential tasks like loading tasks are blocked.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kTouchstart, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTestWithoutTouchstartInputHeuristics,
    TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_NoPreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("C1"), std::string("D2"),
                                   std::string("C2"), std::string("I1")));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTestWithoutNonTouchstartInputHeuristics,
    TestCompositorPolicy_MainThreadHandlesInput_SingleEvent_NoPreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();
  // Because we are still waiting for the touchstart to be processed,
  // non-essential tasks like loading tasks are blocked.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kTouchstart, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestCompositorPolicy_DidAnimateForInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->DidAnimateForInputOnCompositorThread();
  // Note DidAnimateForInputOnCompositorThread does not by itself trigger a
  // policy update.
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("C1"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, Navigation_ResetsTaskCostEstimations) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrame();
  // A navigation occurs which creates a new Document thus resetting the task
  // cost estimations.
  scheduler_->DidStartProvisionalLoad(true);
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollUpdate);

  PostTestTasks(&run_order, "C1 T1");

  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimersDontRunWhenMainThreadScrolling) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrame();
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollUpdate);

  PostTestTasks(&run_order, "C1 T1");

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());

  EXPECT_THAT(run_order, testing::ElementsAre(std::string("C1")));
}

class MainThreadSchedulerImplTestWithoutExpensiveTaskBlocking
    : public MainThreadSchedulerImplTest {
 public:
  MainThreadSchedulerImplTestWithoutExpensiveTaskBlocking()
      : MainThreadSchedulerImplTest({kDisableExpensiveTaskBlocking}, {}) {}
  ~MainThreadSchedulerImplTestWithoutExpensiveTaskBlocking() override = default;
};

TEST_F(MainThreadSchedulerImplTestWithoutExpensiveTaskBlocking,
       ExpensiveTimersRunWhenMainThreadScrolling) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrame();
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollUpdate);

  PostTestTasks(&run_order, "C1 T1");

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimersDoRunWhenMainThreadInputHandling) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrame();
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kUndefined);

  PostTestTasks(&run_order, "C1 T1");

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimersDoRunWhenMainThreadScrolling_AndOnCriticalPath) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateExpensiveTasks(timer_task_runner_);
  DoMainFrameOnCriticalPath();
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);

  PostTestTasks(&run_order, "C1 T1");

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("T1")));
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicy_Compositor) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Animation or meta events like TapDown/FlingCancel shouldn't affect the
  // priority.
  run_order.clear();
  scheduler_->DidAnimateForInputOnCompositorThread();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingCancel),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureTapDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("T1"),
                                   std::string("T2")));
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicy_MainThread) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2 T1 T2");

  // Observation of touchstart should defer execution of timer, idle and loading
  // tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Meta events like TapDown/FlingCancel shouldn't affect the priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingCancel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingCancel),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureTapDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kGestureTapDown),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Action events like ScrollBegin will kick us back into compositor priority,
  // allowing service of the timer, loading and idle queues.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();

  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("T1"),
                                   std::string("T2")));
}

// TODO(alexclarke): Reenable once we've reinstaed the Loading
// UseCase.
TEST_F(MainThreadSchedulerImplTest, DISABLED_LoadingUseCase) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 T1 L1 D2 C2 T2 L2");

  scheduler_->DidStartProvisionalLoad(true);
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  // In loading policy, loading tasks are prioritized other others.
  std::string loading_policy_expected[] = {
      std::string("D1"), std::string("L1"), std::string("D2"),
      std::string("L2"), std::string("C1"), std::string("T1"),
      std::string("C2"), std::string("T2"), std::string("I1")};
  EXPECT_THAT(run_order, testing::ElementsAreArray(loading_policy_expected));
  EXPECT_EQ(UseCase::kLoading, CurrentUseCase());

  // Advance 15s and try again, the loading policy should have ended and the
  // task order should return to the NONE use case where loading tasks are no
  // longer prioritized.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(150000));
  run_order.clear();
  PostTestTasks(&run_order, "I1 D1 C1 T1 L1 D2 C2 T2 L2");
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();

  std::string default_order_expected[] = {
      std::string("D1"), std::string("C1"), std::string("T1"),
      std::string("L1"), std::string("D2"), std::string("C2"),
      std::string("T2"), std::string("L2"), std::string("I1")};
  EXPECT_THAT(run_order, testing::ElementsAreArray(default_order_expected));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresMouseMove_WhenMouseUp) {
  RunSlowCompositorTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_IgnoresMouseMove_WhenMouseUp) {
  RunSlowCompositorTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseMove_WhenMouseDown) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  // Note that currently the compositor will never consume mouse move events,
  // but this test reflects what should happen if that was the case.
  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks deprioritized.
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("C1"), std::string("C2"),
                                   std::string("I1")));
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseMove_WhenMouseDown_AfterMouseWheel) {
  // Simulate a main thread driven mouse wheel scroll gesture.
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollUpdate);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());

  // Now start a main thread mouse touch gesture. It should be detected as main
  // thread custom input handling.
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");
  EnableIdleTasks();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseDown,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseMove,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
}

TEST_F(MainThreadSchedulerImplTest, EventForwardedToMainThread_MouseClick) {
  // A mouse click should be detected as main thread input handling, which means
  // we won't try to defer expensive tasks because of one. We can, however,
  // prioritize compositing/input handling.
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");
  EnableIdleTasks();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseDown,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kMouseUp,
                     blink::WebInputEvent::kLeftButtonDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_MouseWheel) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::kMouseWheel),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("C1"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_MouseWheel_PreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized (since they are fast).
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_NoPreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());
}

TEST_F(
    MainThreadSchedulerImplTest,
    EventForwardedToMainThreadAndBackToCompositor_MouseWheel_NoPreventDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeMouseWheelEvent(blink::WebInputEvent::kMouseWheel),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("D2"),
                                   std::string("C1"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventConsumedOnCompositorThread_IgnoresKeyboardEvents) {
  RunSlowCompositorTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kKeyDown),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       EventForwardedToMainThread_IgnoresKeyboardEvents) {
  RunSlowCompositorTask();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "I1 D1 C1 D2 C2");

  EnableIdleTasks();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kKeyDown),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  // Note compositor tasks are not prioritized.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("D2"), std::string("C2"),
                                   std::string("I1")));
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  // Note compositor tasks are not prioritized.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kKeyDown),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest,
       TestMainthreadScrollingUseCaseDoesNotStarveDefaultTasks) {
  SimulateMainThreadGestureStart(TouchEventPolicy::kDontSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);
  EnableIdleTasks();

  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");

  for (int i = 0; i < 20; i++) {
    compositor_task_runner_->PostTask(FROM_HERE, base::BindOnce(&NullTask));
  }
  PostTestTasks(&run_order, "C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  // Ensure that the default D1 task gets to run at some point before the final
  // C2 compositor task.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("D1"),
                                   std::string("C2")));
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicyEnds_CompositorHandlesInput) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());

  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicyEnds_MainThreadHandlesInput) {
  SimulateMainThreadGestureStart(TouchEventPolicy::kDontSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling,
            ForceUpdatePolicyAndGetCurrentUseCase());

  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, TestTouchstartPolicyEndsAfterTimeout) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  run_order.clear();
  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));

  // Don't post any compositor tasks to simulate a very long running event
  // handler.
  PostTestTasks(&run_order, "D1 D2");

  // Touchstart policy mode should have ended now that the clock has advanced.
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1"),
                                   std::string("D2")));
}

TEST_F(MainThreadSchedulerImplTest,
       TestTouchstartPolicyEndsAfterConsecutiveTouchmoves) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 D1 C1 D2 C2");

  // Observation of touchstart should defer execution of idle and loading tasks.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("C2"),
                                   std::string("D1"), std::string("D2")));

  // Receiving the first touchmove will not affect scheduler priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  // Receiving the second touchmove will kick us back into compositor priority.
  run_order.clear();
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("L1")));
}

TEST_F(MainThreadSchedulerImplTest, TestIsHighPriorityWorkAnticipated) {
  bool is_anticipated_before = false;
  bool is_anticipated_after = false;

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kNone,
                                &is_anticipated_before, &is_anticipated_after));
  base::RunLoop().RunUntilIdle();
  // In its default state, without input receipt, the scheduler should indicate
  // that no high-priority is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);

  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kTouchStart,
                                &is_anticipated_before, &is_anticipated_after));
  bool dummy;
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kTouchEnd, &dummy, &dummy));
  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                     SimulateInputType::kGestureScrollBegin, &dummy, &dummy));
  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                     SimulateInputType::kGestureScrollEnd, &dummy, &dummy));

  base::RunLoop().RunUntilIdle();
  // When input is received, the scheduler should indicate that high-priority
  // work is anticipated.
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_TRUE(is_anticipated_after);

  test_task_runner_->AdvanceMockTickClock(
      priority_escalation_after_input_duration() * 2);
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kNone,
                                &is_anticipated_before, &is_anticipated_after));
  base::RunLoop().RunUntilIdle();
  // Without additional input, the scheduler should go into NONE
  // use case but with scrolling expected where high-priority work is still
  // anticipated.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_TRUE(is_anticipated_before);
  EXPECT_TRUE(is_anticipated_after);

  test_task_runner_->AdvanceMockTickClock(
      subsequent_input_expected_after_input_duration() * 2);
  default_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AnticipationTestTask, scheduler_.get(),
                                SimulateInputType::kNone,
                                &is_anticipated_before, &is_anticipated_after));
  base::RunLoop().RunUntilIdle();
  // Eventually the scheduler should go into the default use case where
  // high-priority work is no longer anticipated.
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_FALSE(is_anticipated_before);
  EXPECT_FALSE(is_anticipated_after);
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
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_TRUE(scheduler_->ShouldYieldForHighPriorityWork());
  base::RunLoop().RunUntilIdle();
}

TEST_F(MainThreadSchedulerImplTest, SlowMainThreadInputEvent) {
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());

  // An input event should bump us into input priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // Simulate the input event being queued for a very long time. The compositor
  // task we post here represents the enqueued input task.
  test_task_runner_->AdvanceMockTickClock(
      priority_escalation_after_input_duration() * 2);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
  base::RunLoop().RunUntilIdle();

  // Even though we exceeded the input priority escalation period, we should
  // still be in main thread gesture since the input remains queued.
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // After the escalation period ends we should go back into normal mode.
  test_task_runner_->FastForwardBy(priority_escalation_after_input_duration() *
                                   2);
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
  scheduler_->ScheduleDelayedPolicyUpdate(Now(),
                                          base::TimeDelta::FromMilliseconds(1));
  scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();

  test_task_runner_->FastForwardUntilNoTasksRemain();
  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, OneUrgentAndOnePendingDelayedUpdatePolicy) {
  scheduler_->EnsureUrgentPolicyUpdatePostedOnMainThread();
  scheduler_->ScheduleDelayedPolicyUpdate(Now(),
                                          base::TimeDelta::FromMilliseconds(1));

  test_task_runner_->FastForwardUntilNoTasksRemain();
  // We expect both the urgent and the delayed updates to run.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, UpdatePolicyCountTriggeredByOneInputEvent) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  test_task_runner_->AdvanceMockTickClock(base::TimeDelta::FromSeconds(1));
  base::RunLoop().RunUntilIdle();
  // We finally expect a delayed policy update 100ms later.
  EXPECT_EQ(2, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest,
       UpdatePolicyCountTriggeredByThreeInputEvents) {
  // We expect DidHandleInputEventOnCompositorThread to post
  // an urgent policy update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart,
                     blink::WebInputEvent::kEventNonBlocking),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // The second call to DidHandleInputEventOnCompositorThread should not post
  // a policy update because we are already in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  // The third call to DidHandleInputEventOnCompositorThread should post a
  // policy update because the awaiting_touch_start_response_ flag changed.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect DidHandleInputEvent to trigger a policy update.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // We finally expect a delayed policy update.
  EXPECT_EQ(3, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest,
       UpdatePolicyCountTriggeredByTwoInputEventsWithALongSeparatingDelay) {
  // We expect DidHandleInputEventOnCompositorThread to post an urgent policy
  // update.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart,
                     blink::WebInputEvent::kEventNonBlocking),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // We expect a delayed policy update.
  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect the second call to DidHandleInputEventOnCompositorThread to post
  // an urgent policy update because we are no longer in compositor priority.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, scheduler_->update_policy_count_);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      WebInputEventResult::kHandledSystem);
  EXPECT_EQ(3, scheduler_->update_policy_count_);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  // We finally expect a delayed policy update.
  EXPECT_EQ(4, scheduler_->update_policy_count_);
}

TEST_F(MainThreadSchedulerImplTest, EnsureUpdatePolicyNotTriggeredTooOften) {
  EXPECT_EQ(0, scheduler_->update_policy_count_);
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  EXPECT_EQ(1, scheduler_->update_policy_count_);

  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  // We expect the first call to IsHighPriorityWorkAnticipated to be called
  // after receiving an input event (but before the UpdateTask was processed) to
  // call UpdatePolicy.
  EXPECT_EQ(1, scheduler_->update_policy_count_);
  scheduler_->IsHighPriorityWorkAnticipated();
  EXPECT_EQ(2, scheduler_->update_policy_count_);
  // Subsequent calls should not call UpdatePolicy.
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->IsHighPriorityWorkAnticipated();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();
  scheduler_->ShouldYieldForHighPriorityWork();

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollEnd),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kTouchEnd),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

  scheduler_->DidHandleInputEventOnMainThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kTouchMove),
      WebInputEventResult::kHandledSystem);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kTouchEnd),
      WebInputEventResult::kHandledSystem);

  EXPECT_EQ(2, scheduler_->update_policy_count_);

  // We expect both the urgent and the delayed updates to run in addition to the
  // earlier updated cause by IsHighPriorityWorkAnticipated, a final update
  // transitions from 'not_scrolling touchstart expected' to 'not_scrolling'.
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(
      scheduler_->use_cases_,
      testing::ElementsAre(
          std::string("none"), std::string("compositor_gesture"),
          std::string("compositor_gesture blocking input expected"),
          std::string("none blocking input expected"), std::string("none")));
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

class MainThreadSchedulerImplWithMessageLoopTest
    : public MainThreadSchedulerImplTest {
 public:
  MainThreadSchedulerImplWithMessageLoopTest()
      : message_loop_(new base::MessageLoop()) {}
  ~MainThreadSchedulerImplWithMessageLoopTest() override = default;

  void SetUp() override {
    clock_.Advance(base::TimeDelta::FromMilliseconds(5));
    Initialize(std::make_unique<MainThreadSchedulerImplForTest>(
        base::sequence_manager::SequenceManagerForTest::Create(
            message_loop_.get(), message_loop_->task_runner(), &clock_),
        base::nullopt));
  }

  // Needed for EnableIdleTasks().
  base::TimeTicks Now() override { return clock_.NowTicks(); }

  void PostFromNestedRunloop(
      std::vector<std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>*
          tasks) {
    for (std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>& pair : *tasks) {
      if (pair.second) {
        idle_task_runner_->PostIdleTask(FROM_HERE, std::move(pair.first));
      } else {
        idle_task_runner_->PostNonNestableIdleTask(FROM_HERE,
                                                   std::move(pair.first));
      }
    }
    EnableIdleTasks();
    base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  base::SimpleTestTickClock clock_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadSchedulerImplWithMessageLoopTest);
};

TEST_F(MainThreadSchedulerImplWithMessageLoopTest,
       NonNestableIdleTaskDoesntExecuteInNestedLoop) {
  std::vector<std::string> order;
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("1")));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("2")));

  std::vector<std::pair<SingleThreadIdleTaskRunner::IdleTask, bool>>
      tasks_to_post_from_nested_loop;
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("3")),
      false));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("4")),
      true));
  tasks_to_post_from_nested_loop.push_back(std::make_pair(
      base::BindOnce(&AppendToVectorIdleTestTask, &order, std::string("5")),
      true));

  default_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MainThreadSchedulerImplWithMessageLoopTest::PostFromNestedRunloop,
          base::Unretained(this),
          base::Unretained(&tasks_to_post_from_nested_loop)));

  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  // Note we expect task 3 to run last because it's non-nestable.
  EXPECT_THAT(order, testing::ElementsAre(std::string("1"), std::string("2"),
                                          std::string("4"), std::string("5"),
                                          std::string("3")));
}

TEST_F(MainThreadSchedulerImplTest, TestBeginMainFrameNotExpectedUntil) {
  base::TimeDelta ten_millis(base::TimeDelta::FromMilliseconds(10));
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
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(30);
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
  base::TimeDelta pending_task_delay = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks deadline_in_task;
  int run_count = 0;

  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        pending_task_delay);

  // Advance clock until after delayed task was meant to be run.
  test_task_runner_->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(20));

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
  std::vector<base::TimeTicks> actual_deadlines;
  int run_count = 0;

  g_max_idle_task_reposts = 3;
  base::TimeTicks clock_before = Now();
  base::TimeDelta idle_task_runtime(base::TimeDelta::FromMilliseconds(10));
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
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, run_count);

  // The long idle period should start after the touchstart policy has finished.
  test_task_runner_->FastForwardBy(priority_escalation_after_input_duration());
  EXPECT_EQ(1, run_count);
}

void TestCanExceedIdleDeadlineIfRequiredTask(ThreadScheduler* scheduler,
                                             bool* can_exceed_idle_deadline_out,
                                             int* run_count,
                                             base::TimeTicks deadline) {
  *can_exceed_idle_deadline_out = scheduler->CanExceedIdleDeadlineIfRequired();
  (*run_count)++;
}

TEST_F(MainThreadSchedulerImplTest, CanExceedIdleDeadlineIfRequired) {
  int run_count = 0;
  bool can_exceed_idle_deadline = false;

  // Should return false if not in an idle period.
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());

  // Should return false for short idle periods.
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                     &can_exceed_idle_deadline, &run_count));
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Should return false for a long idle period which is shortened due to a
  // pending delayed task.
  default_task_runner_->PostDelayedTask(FROM_HERE, base::BindOnce(&NullTask),
                                        base::TimeDelta::FromMilliseconds(10));
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                     &can_exceed_idle_deadline, &run_count));
  scheduler_->BeginFrameNotExpectedSoon();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, run_count);
  EXPECT_FALSE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  test_task_runner_->AdvanceMockTickClock(maximum_idle_period_duration());
  idle_task_runner_->PostIdleTask(
      FROM_HERE,
      base::BindOnce(&TestCanExceedIdleDeadlineIfRequiredTask, scheduler_.get(),
                     &can_exceed_idle_deadline, &run_count));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, run_count);
  EXPECT_TRUE(can_exceed_idle_deadline);

  // Next long idle period will be for the maximum time, so
  // CanExceedIdleDeadlineIfRequired should return true.
  scheduler_->WillBeginFrame(viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL));
  EXPECT_FALSE(scheduler_->CanExceedIdleDeadlineIfRequired());
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
                                   base::TimeDelta::FromMilliseconds(10));
  EXPECT_EQ(2, run_count);
}

TEST_F(MainThreadSchedulerImplTest, TimerQueueEnabledByDefault) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(MainThreadSchedulerImplTest, StopAndResumeRenderer) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle = scheduler_->PauseRenderer();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());

  pause_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(MainThreadSchedulerImplTest, StopAndThrottleTimerQueue) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle = scheduler_->PauseRenderer();
  base::RunLoop().RunUntilIdle();
  scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
      timer_task_queue_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

TEST_F(MainThreadSchedulerImplTest, ThrottleAndPauseRenderer) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  scheduler_->task_queue_throttler()->IncreaseThrottleRefCount(
      timer_task_queue_.get());
  base::RunLoop().RunUntilIdle();
  auto pause_handle = scheduler_->PauseRenderer();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

TEST_F(MainThreadSchedulerImplTest, MultipleStopsNeedMultipleResumes) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 T2");

  auto pause_handle1 = scheduler_->PauseRenderer();
  auto pause_handle2 = scheduler_->PauseRenderer();
  auto pause_handle3 = scheduler_->PauseRenderer();
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
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("T2")));
}

TEST_F(MainThreadSchedulerImplTest, PauseRenderer) {
  // Tasks in some queues don't fire when the renderer is paused.
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1 L1 I1 T1");
  auto pause_handle = scheduler_->PauseRenderer();
  EnableIdleTasks();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("I1")));

  // Tasks are executed when renderer is resumed.
  run_order.clear();
  pause_handle.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("T1")));
}

TEST_F(MainThreadSchedulerImplTest, UseCaseToString) {
  CheckAllUseCaseToString();
}

TEST_F(MainThreadSchedulerImplTest, MismatchedDidHandleInputEventOnMainThread) {
  // This should not DCHECK because there was no corresponding compositor side
  // call to DidHandleInputEventOnCompositorThread with
  // INPUT_EVENT_ACK_STATE_NOT_CONSUMED. There are legitimate reasons for the
  // compositor to not be there and we don't want to make debugging impossible.
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kGestureFlingStart),
      WebInputEventResult::kHandledSystem);
}

TEST_F(MainThreadSchedulerImplTest, BeginMainFrameOnCriticalPath) {
  ASSERT_FALSE(scheduler_->BeginMainFrameOnCriticalPath());

  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(1000),
      viz::BeginFrameArgs::NORMAL);
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
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order, testing::ElementsAre());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveLoadingTasksNotBlockedTillFirstBeginMainFrame) {
  std::vector<std::string> run_order;

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  SimulateExpensiveTasks(loading_task_queue_->task_runner());
  ForceBlockingInputToBeExpectedSoon();
  PostTestTasks(&run_order, "L1 D1");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_FALSE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1")));

  // Emit a BeginMainFrame, and the loading task should get blocked.
  DoMainFrame();
  run_order.clear();

  PostTestTasks(&run_order, "L1 D1");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveLoadingTasksNotBlockedIfNoTouchHandler) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(false);
  DoMainFrame();
  SimulateExpensiveTasks(loading_task_queue_->task_runner());
  ForceBlockingInputToBeExpectedSoon();
  PostTestTasks(&run_order, "L1 D1");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_FALSE(BlockingInputExpectedSoon());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("L1"), std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimerTaskBlocked_UseCase_NONE_PreviousCompositorGesture) {
  std::vector<std::string> run_order;

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  ForceBlockingInputToBeExpectedSoon();

  PostTestTasks(&run_order, "T1 D1");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimerTaskNotBlocked_UseCase_NONE_PreviousMainThreadGesture) {
  std::vector<std::string> run_order;

  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);

  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling,
            ForceUpdatePolicyAndGetCurrentUseCase());

  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kTouchEnd),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  scheduler_->DidHandleInputEventOnMainThread(
      FakeInputEvent(blink::WebInputEvent::kTouchEnd),
      WebInputEventResult::kHandledSystem);

  test_task_runner_->AdvanceMockTickClock(
      priority_escalation_after_input_duration() * 2);
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());

  PostTestTasks(&run_order, "T1 D1");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T1"), std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimerTaskBlocked_UseCase_kCompositorGesture) {
  std::vector<std::string> run_order;

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  ForceBlockingInputToBeExpectedSoon();
  scheduler_->DidAnimateForInputOnCompositorThread();

  PostTestTasks(&run_order, "T1 D1");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimerTaskBlocked_EvenIfBeginMainFrameNotExpectedSoon) {
  std::vector<std::string> run_order;

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  DoMainFrame();
  SimulateExpensiveTasks(timer_task_runner_);
  ForceBlockingInputToBeExpectedSoon();
  scheduler_->BeginFrameNotExpectedSoon();

  PostTestTasks(&run_order, "T1 D1");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_TRUE(HaveSeenABeginMainframe());
  EXPECT_FALSE(LoadingTasksSeemExpensive());
  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveLoadingTasksBlockedIfChildFrameNavigationExpected) {
  std::vector<std::string> run_order;

  DoMainFrame();
  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  SimulateExpensiveTasks(loading_task_queue_->task_runner());
  ForceBlockingInputToBeExpectedSoon();

  PostTestTasks(&run_order, "L1 D1");
  base::RunLoop().RunUntilIdle();

  // The expensive loading task gets blocked.
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("D1")));
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveLoadingTasksNotBlockedDuringMainThreadGestures) {
  std::vector<std::string> run_order;

  SimulateExpensiveTasks(loading_task_queue_->task_runner());

  // Loading tasks should not be disabled during main thread user interactions.
  PostTestTasks(&run_order, "C1 L1");

  // Trigger main_thread_gesture UseCase
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  EXPECT_TRUE(LoadingTasksSeemExpensive());
  EXPECT_FALSE(TimerTasksSeemExpensive());
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C1"), std::string("L1")));
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, GetRAILMode());
}

TEST_F(MainThreadSchedulerImplTest, ModeratelyExpensiveTimer_NotBlocked) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kTouchMove);
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 20; i++) {
    simulate_timer_task_ran_ = false;

    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadInputHandlingCompositorTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(8)));
    timer_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::SimulateTimerTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(4)));

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(simulate_timer_task_ran_) << " i = " << i;
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase())
        << " i = " << i;
    EXPECT_FALSE(LoadingTasksSeemExpensive()) << " i = " << i;
    EXPECT_FALSE(TimerTasksSeemExpensive()) << " i = " << i;

    base::TimeDelta time_till_next_frame = EstimatedNextFrameBegin() - Now();
    if (time_till_next_frame > base::TimeDelta())
      test_task_runner_->AdvanceMockTickClock(time_till_next_frame);
  }
}

TEST_F(MainThreadSchedulerImplTest,
       FourtyMsTimer_NotBlocked_CompositorScrolling) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 20; i++) {
    simulate_timer_task_ran_ = false;

    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidAnimateForInputOnCompositorThread();

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MainThreadSchedulerImplTest::SimulateMainThreadCompositorTask,
            base::Unretained(this), base::TimeDelta::FromMilliseconds(8)));
    timer_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::SimulateTimerTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(40)));

    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(simulate_timer_task_ran_) << " i = " << i;
    EXPECT_EQ(UseCase::kCompositorGesture, CurrentUseCase()) << " i = " << i;
    EXPECT_FALSE(LoadingTasksSeemExpensive()) << " i = " << i;
    EXPECT_FALSE(TimerTasksSeemExpensive()) << " i = " << i;

    base::TimeDelta time_till_next_frame = EstimatedNextFrameBegin() - Now();
    if (time_till_next_frame > base::TimeDelta())
      test_task_runner_->AdvanceMockTickClock(time_till_next_frame);
  }
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimer_NotBlocked_UseCase_MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kTouchMove);
  base::RunLoop().RunUntilIdle();
  for (int i = 0; i < 20; i++) {
    simulate_timer_task_ran_ = false;

    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = false;
    scheduler_->WillBeginFrame(begin_frame_args);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadInputHandlingCompositorTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(8)));
    timer_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::SimulateTimerTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase())
        << " i = " << i;
    EXPECT_FALSE(LoadingTasksSeemExpensive()) << " i = " << i;
    if (i == 0) {
      EXPECT_FALSE(TimerTasksSeemExpensive()) << " i = " << i;
    } else {
      EXPECT_TRUE(TimerTasksSeemExpensive()) << " i = " << i;
    }
    EXPECT_TRUE(simulate_timer_task_ran_) << " i = " << i;

    base::TimeDelta time_till_next_frame = EstimatedNextFrameBegin() - Now();
    if (time_till_next_frame > base::TimeDelta())
      test_task_runner_->AdvanceMockTickClock(time_till_next_frame);
  }
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_NONE) {
  EXPECT_EQ(UseCase::kNone, CurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_kCompositorGesture) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

// TODO(alexclarke): Reenable once we've reinstaed the Loading
// UseCase.
TEST_F(MainThreadSchedulerImplTest,
       DISABLED_EstimateLongestJankFreeTaskDuration_UseCase_) {
  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(UseCase::kLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(rails_response_time(),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_MAIN_THREAD_GESTURE) {
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollUpdate);
  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
      viz::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MainThreadSchedulerImplTest::
                         SimulateMainThreadInputHandlingCompositorTask,
                     base::Unretained(this),
                     base::TimeDelta::FromMilliseconds(5)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(
    MainThreadSchedulerImplTest,
    EstimateLongestJankFreeTaskDuration_UseCase_MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
      viz::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = false;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MainThreadSchedulerImplTest::
                         SimulateMainThreadInputHandlingCompositorTask,
                     base::Unretained(this),
                     base::TimeDelta::FromMilliseconds(5)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

TEST_F(MainThreadSchedulerImplTest,
       EstimateLongestJankFreeTaskDuration_UseCase_SYNCHRONIZED_GESTURE) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);

  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
      viz::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = true;
  scheduler_->WillBeginFrame(begin_frame_args);

  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MainThreadSchedulerImplTest::SimulateMainThreadCompositorTask,
          base::Unretained(this), base::TimeDelta::FromMilliseconds(5)));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase());

  // 16ms frame - 5ms compositor work = 11ms for other stuff.
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(11),
            scheduler_->EstimateLongestJankFreeTaskDuration());
}

class PageSchedulerImplForTest : public PageSchedulerImpl {
 public:
  explicit PageSchedulerImplForTest(MainThreadSchedulerImpl* scheduler)
      : PageSchedulerImpl(nullptr, scheduler) {}
  ~PageSchedulerImplForTest() override = default;

  void ReportIntervention(const std::string& message) override {
    interventions_.push_back(message);
  }

  const std::vector<std::string>& Interventions() const {
    return interventions_;
  }

  MOCK_METHOD1(RequestBeginMainFrameNotExpected, bool(bool));

 private:
  std::vector<std::string> interventions_;

  DISALLOW_COPY_AND_ASSIGN(PageSchedulerImplForTest);
};

namespace {
void SlowCountingTask(size_t* count,
                      scoped_refptr<base::TestMockTimeTaskRunner> task_runner,
                      int task_duration,
                      scoped_refptr<base::SingleThreadTaskRunner> timer_queue) {
  task_runner->AdvanceMockTickClock(
      base::TimeDelta::FromMilliseconds(task_duration));
  if (++(*count) < 500) {
    timer_queue->PostTask(FROM_HERE,
                          base::BindOnce(SlowCountingTask, count, task_runner,
                                         task_duration, timer_queue));
  }
}
}  // namespace

TEST_F(MainThreadSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_task_expensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::kDontSendTouchStart);

  base::TimeTicks first_throttled_run_time =
      TaskQueueThrottler::AlignedThrottledRunTime(Now());

  size_t count = 0;
  // With the compositor task taking 10ms, there is not enough time to run this
  // 7ms timer task in the 16ms frame.
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &count, test_task_runner_, 7,
                                timer_task_runner_));

  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;

    // We expect the queue to get throttled on the second iteration which is
    // when the system realizes the task is expensive.
    bool expect_queue_throttled = (i > 0);
    EXPECT_EQ(expect_queue_throttled,
              scheduler_->task_queue_throttler()->IsThrottled(
                  timer_task_queue_.get()))
        << "i = " << i;

    if (expect_queue_throttled) {
      EXPECT_GE(count, 2u);
    } else {
      EXPECT_LE(count, 2u);
    }

    // The task runs twice before the system realizes it's too expensive.
    bool throttled_task_has_run = count > 2;
    bool throttled_task_expected_to_have_run =
        (Now() > first_throttled_run_time);
    EXPECT_EQ(throttled_task_expected_to_have_run, throttled_task_has_run)
        << "i = " << i << " count = " << count;
  }

  // Task is throttled but not completely blocked.
  EXPECT_EQ(12u, count);
}

TEST_F(MainThreadSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_TimersStopped) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  base::TimeTicks first_throttled_run_time =
      TaskQueueThrottler::AlignedThrottledRunTime(Now());

  size_t count = 0;
  // With the compositor task taking 10ms, there is not enough time to run this
  // 7ms timer task in the 16ms frame.
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &count, test_task_runner_, 7,
                                timer_task_runner_));

  std::unique_ptr<WebThreadScheduler::RendererPauseHandle> paused;
  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;

    // Before the policy is updated the queue will be enabled. Subsequently it
    // will be disabled until the throttled queue is pumped.
    bool expect_queue_enabled = (i == 0) || (Now() > first_throttled_run_time);
    if (paused)
      expect_queue_enabled = false;
    EXPECT_EQ(expect_queue_enabled, timer_task_queue_->IsQueueEnabled())
        << "i = " << i;

    // After we've run any expensive tasks suspend the queue.  The throttling
    // helper should /not/ re-enable this queue under any circumstances while
    // timers are paused.
    if (count > 0 && !paused) {
      EXPECT_EQ(2u, count);
      paused = scheduler_->PauseRenderer();
    }
  }

  // Make sure the timer queue stayed paused!
  EXPECT_EQ(2u, count);
}

TEST_F(MainThreadSchedulerImplTest,
       SYNCHRONIZED_GESTURE_TimerTaskThrottling_task_not_expensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  size_t count = 0;
  // With the compositor task taking 10ms, there is enough time to run this 6ms
  // timer task in the 16ms frame.
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &count, test_task_runner_, 6,
                                timer_task_runner_));

  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
    EXPECT_TRUE(timer_task_queue_->IsQueueEnabled()) << "i = " << i;
  }

  // Task is not throttled.
  EXPECT_EQ(500u, count);
}

TEST_F(MainThreadSchedulerImplTest,
       ExpensiveTimerTaskBlocked_SYNCHRONIZED_GESTURE_GestureExpected) {
  SimulateExpensiveTasks(timer_task_runner_);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  ForceBlockingInputToBeExpectedSoon();

  // Bump us into SYNCHRONIZED_GESTURE.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

  viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
      base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
      viz::BeginFrameArgs::NORMAL);
  begin_frame_args.on_critical_path = true;
  scheduler_->WillBeginFrame(begin_frame_args);

  EXPECT_EQ(UseCase::kSynchronizedGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());

  EXPECT_TRUE(TimerTasksSeemExpensive());
  EXPECT_TRUE(BlockingInputExpectedSoon());
  EXPECT_FALSE(timer_task_queue_->IsQueueEnabled());
}

TEST_F(MainThreadSchedulerImplTest, DenyLongIdleDuringTouchStart) {
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
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
  now += base::TimeDelta::FromMilliseconds(500);
  EXPECT_FALSE(idle_delegate->CanEnterLongIdlePeriod(now, &next_time_to_check));
  EXPECT_GE(next_time_to_check, base::TimeDelta());
}

TEST_F(MainThreadSchedulerImplTest,
       TestCompositorPolicy_TouchStartDuringFling) {
  scheduler_->SetHasVisibleRenderWidgetWithTouchHandler(true);
  scheduler_->DidAnimateForInputOnCompositorThread();
  // Note DidAnimateForInputOnCompositorThread does not by itself trigger a
  // policy update.
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());

  // Make sure TouchStart causes a policy change.
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeTouchEvent(blink::WebInputEvent::kTouchStart),
      InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);
  EXPECT_EQ(UseCase::kTouchstart, ForceUpdatePolicyAndGetCurrentUseCase());
}

TEST_F(MainThreadSchedulerImplTest, SYNCHRONIZED_GESTURE_CompositingExpensive) {
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  std::vector<std::string> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
  }

  // Timer tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskQueue::kNormalPriority,
            scheduler_->CompositorTaskQueue()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(MainThreadSchedulerImplTest, MAIN_THREAD_CUSTOM_INPUT_HANDLING) {
  SimulateMainThreadGestureStart(TouchEventPolicy::kSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. To avoid starvation, compositing tasks
  // should therefore not get prioritized.
  std::vector<std::string> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kTouchMove),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kMainThreadCustomInputHandling, CurrentUseCase())
        << "i = " << i;
  }

  // Timer tasks should not have been starved by the expensive compositor
  // tasks.
  EXPECT_EQ(TaskQueue::kNormalPriority,
            scheduler_->CompositorTaskQueue()->GetQueuePriority());
  EXPECT_EQ(1000u, run_order.size());
}

TEST_F(MainThreadSchedulerImplTest, MAIN_THREAD_GESTURE) {
  SimulateMainThreadGestureStart(TouchEventPolicy::kDontSendTouchStart,
                                 blink::WebInputEvent::kGestureScrollBegin);

  // With the compositor task taking 20ms, there is not enough time to run
  // other tasks in the same 16ms frame. However because this is a main thread
  // gesture instead of custom main thread input handling, we allow the timer
  // tasks to be starved.
  std::vector<std::string> run_order;
  for (int i = 0; i < 1000; i++)
    PostTestTasks(&run_order, "T1");

  for (int i = 0; i < 100; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_FORWARDED_TO_MAIN_THREAD);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(20)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kMainThreadGesture, CurrentUseCase()) << "i = " << i;
  }

  EXPECT_EQ(TaskQueue::kHighestPriority,
            scheduler_->CompositorTaskQueue()->GetQueuePriority());
  EXPECT_EQ(279u, run_order.size());
}

class MockRAILModeObserver : public WebRAILModeObserver {
 public:
  MOCK_METHOD1(OnRAILModeChanged, void(v8::RAILMode rail_mode));
};

TEST_F(MainThreadSchedulerImplTest, TestResponseRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_RESPONSE));

  scheduler_->SetHaveSeenABlockingGestureForTesting(true);
  ForceBlockingInputToBeExpectedSoon();
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_RESPONSE, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestAnimateRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_ANIMATION)).Times(0);

  EXPECT_FALSE(BeginFrameNotExpectedSoon());
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestIdleRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_ANIMATION));
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_IDLE));

  scheduler_->SetAllRenderWidgetsHidden(true);
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_IDLE, GetRAILMode());
  scheduler_->SetAllRenderWidgetsHidden(false);
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, TestLoadRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_ANIMATION));
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_LOAD));

  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(v8::PERFORMANCE_LOAD, GetRAILMode());
  EXPECT_EQ(UseCase::kLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_EQ(UseCase::kNone, ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, InputTerminatesLoadRAILMode) {
  MockRAILModeObserver observer;
  scheduler_->AddRAILModeObserver(&observer);
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_ANIMATION));
  EXPECT_CALL(observer, OnRAILModeChanged(v8::PERFORMANCE_LOAD));

  scheduler_->DidStartProvisionalLoad(true);
  EXPECT_EQ(v8::PERFORMANCE_LOAD, GetRAILMode());
  EXPECT_EQ(UseCase::kLoading, ForceUpdatePolicyAndGetCurrentUseCase());
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollBegin),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  scheduler_->DidHandleInputEventOnCompositorThread(
      FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
      InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);
  EXPECT_EQ(UseCase::kCompositorGesture,
            ForceUpdatePolicyAndGetCurrentUseCase());
  EXPECT_EQ(v8::PERFORMANCE_ANIMATION, GetRAILMode());
  scheduler_->RemoveRAILModeObserver(&observer);
}

TEST_F(MainThreadSchedulerImplTest, UnthrottledTaskRunner) {
  // Ensure neither suspension nor timer task throttling affects an unthrottled
  // task runner.
  SimulateCompositorGestureStart(TouchEventPolicy::kSendTouchStart);
  scoped_refptr<TaskQueue> unthrottled_task_queue =
      scheduler_->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kUnthrottled));

  size_t timer_count = 0;
  size_t unthrottled_count = 0;
  timer_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(SlowCountingTask, &timer_count,
                                test_task_runner_, 7, timer_task_runner_));
  unthrottled_task_queue->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(SlowCountingTask, &unthrottled_count, test_task_runner_, 7,
                     unthrottled_task_queue->task_runner()));
  auto handle = scheduler_->PauseRenderer();

  for (int i = 0; i < 1000; i++) {
    viz::BeginFrameArgs begin_frame_args = viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, Now(),
        base::TimeTicks(), base::TimeDelta::FromMilliseconds(16),
        viz::BeginFrameArgs::NORMAL);
    begin_frame_args.on_critical_path = true;
    scheduler_->WillBeginFrame(begin_frame_args);
    scheduler_->DidHandleInputEventOnCompositorThread(
        FakeInputEvent(blink::WebInputEvent::kGestureScrollUpdate),
        InputEventState::EVENT_CONSUMED_BY_COMPOSITOR);

    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MainThreadSchedulerImplTest::
                           SimulateMainThreadCompositorAndQuitRunLoopTask,
                       base::Unretained(this),
                       base::TimeDelta::FromMilliseconds(10)));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(UseCase::kSynchronizedGesture, CurrentUseCase()) << "i = " << i;
  }

  EXPECT_EQ(0u, timer_count);
  EXPECT_EQ(500u, unthrottled_count);
}

TEST_F(MainThreadSchedulerImplTest,
       VirtualTimePolicyDoesNotAffectNewTimerTaskQueueIfVirtualTimeNotEnabled) {
  scheduler_->SetVirtualTimePolicy(
      PageSchedulerImpl::VirtualTimePolicy::kPause);
  scoped_refptr<MainThreadTaskQueue> timer_tq = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
  EXPECT_FALSE(timer_tq->HasActiveFence());
}

TEST_F(MainThreadSchedulerImplTest, EnableVirtualTime) {
  EXPECT_FALSE(scheduler_->IsVirtualTimeEnabled());
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_TRUE(scheduler_->IsVirtualTimeEnabled());
  scoped_refptr<MainThreadTaskQueue> loading_tq =
      scheduler_->NewLoadingTaskQueue(
          MainThreadTaskQueue::QueueType::kFrameLoading, nullptr);
  scoped_refptr<TaskQueue> loading_control_tq = scheduler_->NewLoadingTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameLoadingControl, nullptr);
  scoped_refptr<MainThreadTaskQueue> timer_tq = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
  scoped_refptr<MainThreadTaskQueue> unthrottled_tq =
      scheduler_->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kUnthrottled));

  EXPECT_EQ(scheduler_->DefaultTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_->CompositorTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(loading_task_queue_->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(timer_task_queue_->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_->VirtualTimeControlTaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_->V8TaskQueue()->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());

  // The main control task queue remains in the real time domain.
  EXPECT_EQ(scheduler_->ControlTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());

  EXPECT_EQ(loading_tq->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(loading_control_tq->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(timer_tq->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(unthrottled_tq->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());

  EXPECT_EQ(scheduler_
                ->NewLoadingTaskQueue(
                    MainThreadTaskQueue::QueueType::kFrameLoading, nullptr)
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_
                ->NewTimerTaskQueue(
                    MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr)
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_
                ->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                    MainThreadTaskQueue::QueueType::kUnthrottled))
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
  EXPECT_EQ(scheduler_
                ->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
                    MainThreadTaskQueue::QueueType::kTest))
                ->GetTimeDomain(),
            scheduler_->GetVirtualTimeDomain());
}

TEST_F(MainThreadSchedulerImplTest, EnableVirtualTimeAfterThrottling) {
  std::unique_ptr<PageSchedulerImpl> page_scheduler =
      base::WrapUnique(new PageSchedulerImpl(nullptr, scheduler_.get()));
  scheduler_->AddPageScheduler(page_scheduler.get());

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  TaskQueue* timer_tq = ThrottleableTaskQueue(frame_scheduler.get()).get();

  frame_scheduler->SetCrossOrigin(true);
  frame_scheduler->SetFrameVisible(false);
  EXPECT_TRUE(scheduler_->task_queue_throttler()->IsThrottled(timer_tq));

  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  EXPECT_EQ(timer_tq->GetTimeDomain(), scheduler_->GetVirtualTimeDomain());
  EXPECT_FALSE(scheduler_->task_queue_throttler()->IsThrottled(timer_tq));
}

TEST_F(MainThreadSchedulerImplTest, DisableVirtualTimeForTesting) {
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);

  scoped_refptr<MainThreadTaskQueue> timer_tq = scheduler_->NewTimerTaskQueue(
      MainThreadTaskQueue::QueueType::kFrameThrottleable, nullptr);
  scoped_refptr<MainThreadTaskQueue> unthrottled_tq =
      scheduler_->NewTaskQueue(MainThreadTaskQueue::QueueCreationParams(
          MainThreadTaskQueue::QueueType::kUnthrottled));

  scheduler_->DisableVirtualTimeForTesting();
  EXPECT_EQ(scheduler_->DefaultTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(scheduler_->CompositorTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(loading_task_queue_->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(timer_task_queue_->GetTimeDomain(), scheduler_->real_time_domain());
  EXPECT_EQ(scheduler_->ControlTaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_EQ(scheduler_->V8TaskQueue()->GetTimeDomain(),
            scheduler_->real_time_domain());
  EXPECT_FALSE(scheduler_->VirtualTimeControlTaskQueue());
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimePauser) {
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  scheduler_->SetVirtualTimePolicy(
      PageSchedulerImpl::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser =
      scheduler_->CreateWebScopedVirtualTimePauser(
          "test", WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);

  base::TimeTicks before = scheduler_->GetVirtualTimeDomain()->Now();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  pauser.PauseVirtualTime();
  EXPECT_FALSE(scheduler_->VirtualTimeAllowedToAdvance());

  pauser.UnpauseVirtualTime();
  EXPECT_TRUE(scheduler_->VirtualTimeAllowedToAdvance());
  base::TimeTicks after = scheduler_->GetVirtualTimeDomain()->Now();
  EXPECT_EQ(after, before);
}

TEST_F(MainThreadSchedulerImplTest, VirtualTimePauserNonInstantTask) {
  scheduler_->EnableVirtualTime(
      MainThreadSchedulerImpl::BaseTimeOverridePolicy::DO_NOT_OVERRIDE);
  scheduler_->SetVirtualTimePolicy(
      PageSchedulerImpl::VirtualTimePolicy::kDeterministicLoading);

  WebScopedVirtualTimePauser pauser =
      scheduler_->CreateWebScopedVirtualTimePauser(
          "test", WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);

  base::TimeTicks before = scheduler_->GetVirtualTimeDomain()->Now();
  pauser.PauseVirtualTime();
  pauser.UnpauseVirtualTime();
  base::TimeTicks after = scheduler_->GetVirtualTimeDomain()->Now();
  EXPECT_GT(after, before);
}

TEST_F(MainThreadSchedulerImplTest, Tracing) {
  // This test sets renderer scheduler to some non-trivial state
  // (by posting tasks, creating child schedulers, etc) and converts it into a
  // traced value. This test checks that no internal checks fire during this.

  std::unique_ptr<PageSchedulerImpl> page_scheduler1 =
      base::WrapUnique(new PageSchedulerImpl(nullptr, scheduler_.get()));
  scheduler_->AddPageScheduler(page_scheduler1.get());

  std::unique_ptr<FrameSchedulerImpl> frame_scheduler =
      FrameSchedulerImpl::Create(page_scheduler1.get(), nullptr, nullptr,
                                 FrameScheduler::FrameType::kSubframe);

  std::unique_ptr<PageSchedulerImpl> page_scheduler2 =
      base::WrapUnique(new PageSchedulerImpl(nullptr, scheduler_.get()));
  scheduler_->AddPageScheduler(page_scheduler2.get());

  CPUTimeBudgetPool* time_budget_pool =
      scheduler_->task_queue_throttler()->CreateCPUTimeBudgetPool("test");

  time_budget_pool->AddQueue(base::TimeTicks(), timer_task_queue_.get());

  timer_task_runner_->PostTask(FROM_HERE, base::BindOnce(NullTask));

  loading_task_queue_->task_runner()->PostDelayedTask(
      FROM_HERE, base::BindOnce(NullTask),
      base::TimeDelta::FromMilliseconds(10));

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> value =
      scheduler_->AsValue(base::TimeTicks());
  EXPECT_TRUE(value);
  EXPECT_FALSE(value->ToString().empty());
}

void RecordingTimeTestTask(
    std::vector<base::TimeTicks>* run_times,
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
  run_times->push_back(task_runner->GetMockTickClock()->NowTicks());
}

TEST_F(MainThreadSchedulerImplTest,
       DefaultTimerTasksAreThrottledWhenBackgrounded) {
  std::vector<base::TimeTicks> run_times;

  scheduler_->SetRendererBackgrounded(true);
  timer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RecordingTimeTestTask, &run_times, test_task_runner_));

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(1100));
  // It's expected to run every "absolute" second.
  EXPECT_THAT(run_times, testing::ElementsAre(base::TimeTicks() +
                                              base::TimeDelta::FromSeconds(1)));
  run_times.clear();

  base::TimeTicks posting_time = Now();
  timer_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RecordingTimeTestTask, &run_times, test_task_runner_),
      base::TimeDelta::FromMilliseconds(200));

  scheduler_->SetRendererBackgrounded(false);

  test_task_runner_->FastForwardBy(base::TimeDelta::FromMilliseconds(400));
  EXPECT_THAT(run_times,
              testing::ElementsAre(posting_time +
                                   base::TimeDelta::FromMilliseconds(200)));
}

//                  Nav Start     Nav Start            assert
//                     |             |                   |
//                     v             v                   v
//    ------------------------------------------------------------>
//     |---long task---|---1s task---|-----long task ----|
//
//                     (---MaxEQT1---)
//                                   (---MaxEQT2---)
//
// --- EQT untracked---|             |---EQT unflushed-----
//
// MaxEQT1 = 500ms is recorded and observed in histogram.
// MaxEQT2 is recorded but not yet in histogram for not being flushed.
TEST_F(MainThreadSchedulerImplTest,
       MaxQueueingTimeMetricRecordedOnlyDuringNavigation) {
  base::HistogramTester tester;
  // Start with a long task whose queueing time will be ignored.
  AdvanceTimeWithTask(10);
  // Navigation start.
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  // The max queueing time of the following task will be recorded.
  AdvanceTimeWithTask(1);
  // The smaller queuing time will be ignored.
  AdvanceTimeWithTask(0.5);
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  // Add another long task after navigation start but without navigation end.
  // This value won't be recorded as there is not navigation.
  AdvanceTimeWithTask(10);
  // The expected queueing time of 1s task in 1s window is 500ms.
  tester.ExpectUniqueSample("RendererScheduler.MaxQueueingTime", 500, 1);
}

// Only the max of all the queueing times is recorded.
TEST_F(MainThreadSchedulerImplTest, MaxQueueingTimeMetricRecordTheMax) {
  base::HistogramTester tester;
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  // The smaller queuing time will be ignored.
  AdvanceTimeWithTask(0.5);
  // The max queueing time of the following task will be recorded.
  AdvanceTimeWithTask(1);
  // The smaller queuing time will be ignored.
  AdvanceTimeWithTask(0.5);
  scheduler_->DidCommitProvisionalLoad(false, false, false);
  tester.ExpectUniqueSample("RendererScheduler.MaxQueueingTime", 500, 1);
}

TEST_F(MainThreadSchedulerImplTest, DidCommitProvisionalLoad) {
  scheduler_->OnFirstMeaningfulPaint();
  EXPECT_FALSE(scheduler_->waiting_for_meaningful_paint());

  // Check that we only clear state for main frame navigations that are either
  // not history inert or are reloads.
  scheduler_->DidCommitProvisionalLoad(false /* is_web_history_inert_commit */,
                                       false /* is_reload */,
                                       false /* is_main_frame */);
  EXPECT_FALSE(scheduler_->waiting_for_meaningful_paint());

  scheduler_->OnFirstMeaningfulPaint();
  scheduler_->DidCommitProvisionalLoad(false /* is_web_history_inert_commit */,
                                       false /* is_reload */,
                                       true /* is_main_frame */);
  EXPECT_TRUE(scheduler_->waiting_for_meaningful_paint());  // State cleared.

  scheduler_->OnFirstMeaningfulPaint();
  scheduler_->DidCommitProvisionalLoad(false /* is_web_history_inert_commit */,
                                       true /* is_reload */,
                                       false /* is_main_frame */);
  EXPECT_FALSE(scheduler_->waiting_for_meaningful_paint());

  scheduler_->OnFirstMeaningfulPaint();
  scheduler_->DidCommitProvisionalLoad(false /* is_web_history_inert_commit */,
                                       true /* is_reload */,
                                       true /* is_main_frame */);
  EXPECT_TRUE(scheduler_->waiting_for_meaningful_paint());  // State cleared.

  scheduler_->OnFirstMeaningfulPaint();
  scheduler_->DidCommitProvisionalLoad(true /* is_web_history_inert_commit */,
                                       false /* is_reload */,
                                       false /* is_main_frame */);
  EXPECT_FALSE(scheduler_->waiting_for_meaningful_paint());

  scheduler_->OnFirstMeaningfulPaint();
  scheduler_->DidCommitProvisionalLoad(true /* is_web_history_inert_commit */,
                                       false /* is_reload */,
                                       true /* is_main_frame */);
  EXPECT_FALSE(scheduler_->waiting_for_meaningful_paint());

  scheduler_->OnFirstMeaningfulPaint();
  scheduler_->DidCommitProvisionalLoad(true /* is_web_history_inert_commit */,
                                       true /* is_reload */,
                                       false /* is_main_frame */);
  EXPECT_FALSE(scheduler_->waiting_for_meaningful_paint());

  scheduler_->OnFirstMeaningfulPaint();
  scheduler_->DidCommitProvisionalLoad(true /* is_web_history_inert_commit */,
                                       true /* is_reload */,
                                       true /* is_main_frame */);
  EXPECT_TRUE(scheduler_->waiting_for_meaningful_paint());  // State cleared.
}

TEST_F(MainThreadSchedulerImplTest, LoadingControlTasks) {
  // Expect control loading tasks (M) to jump ahead of any regular loading
  // tasks (L).
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "L1 L2 M1 L3 L4 M2 L5 L6");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("M1"), std::string("M2"),
                                   std::string("L1"), std::string("L2"),
                                   std::string("L3"), std::string("L4"),
                                   std::string("L5"), std::string("L6")));
}

TEST_F(MainThreadSchedulerImplTest, RequestBeginMainFrameNotExpected) {
  std::unique_ptr<PageSchedulerImplForTest> page_scheduler =
      std::make_unique<PageSchedulerImplForTest>(scheduler_.get());
  scheduler_->AddPageScheduler(page_scheduler.get());

  scheduler_->OnPendingTasksChanged(true);
  EXPECT_CALL(*page_scheduler, RequestBeginMainFrameNotExpected(true))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler.get());

  scheduler_->OnPendingTasksChanged(false);
  EXPECT_CALL(*page_scheduler, RequestBeginMainFrameNotExpected(false))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler.get());
}

TEST_F(MainThreadSchedulerImplTest,
       RequestBeginMainFrameNotExpected_MultipleCalls) {
  std::unique_ptr<PageSchedulerImplForTest> page_scheduler =
      std::make_unique<PageSchedulerImplForTest>(scheduler_.get());
  scheduler_->AddPageScheduler(page_scheduler.get());

  scheduler_->OnPendingTasksChanged(true);
  scheduler_->OnPendingTasksChanged(true);
  // Multiple calls should result in only one call.
  EXPECT_CALL(*page_scheduler, RequestBeginMainFrameNotExpected(true))
      .Times(1)
      .WillRepeatedly(testing::Return(true));
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(page_scheduler.get());
}

#if defined(OS_ANDROID)
TEST_F(MainThreadSchedulerImplTest, PauseTimersForAndroidWebView) {
  // Tasks in some queues don't fire when the timers are paused.
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "D1 C1 L1 I1 T1");
  scheduler_->PauseTimersForAndroidWebView();
  EnableIdleTasks();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("D1"), std::string("C1"),
                                   std::string("L1"), std::string("I1")));
  // The rest queued tasks fire when the timers are resumed.
  run_order.clear();
  scheduler_->ResumeTimersForAndroidWebView();
  test_task_runner_->FastForwardUntilNoTasksRemain();
  EXPECT_THAT(run_order, testing::ElementsAre(std::string("T1")));
}
#endif  // defined(OS_ANDROID)

class MainThreadSchedulerImplWithInitalVirtualTimeTest
    : public MainThreadSchedulerImplTest {
 public:
  void SetUp() override {
    CreateTestTaskRunner();
    Initialize(std::make_unique<MainThreadSchedulerImplForTest>(
        base::sequence_manager::SequenceManagerForTest::Create(
            nullptr, test_task_runner_, test_task_runner_->GetMockTickClock()),
        base::Time::FromJsTime(1000000.0)));
  }
};

TEST_F(MainThreadSchedulerImplWithInitalVirtualTimeTest, VirtualTimeOverride) {
  EXPECT_TRUE(scheduler_->IsVirtualTimeEnabled());
  EXPECT_EQ(PageSchedulerImpl::VirtualTimePolicy::kPause,
            scheduler_->virtual_time_policy());
  EXPECT_EQ(base::Time::Now(), base::Time::FromJsTime(1000000.0));
}

TEST_F(MainThreadSchedulerImplTest, ShouldIgnoreTaskForUkm) {
  bool supports_thread_ticks = base::ThreadTicks::IsSupported();
  double sampling_rate;

  sampling_rate = 0.0001;
  EXPECT_FALSE(scheduler_->ShouldIgnoreTaskForUkm(true, &sampling_rate));
  if (supports_thread_ticks) {
    EXPECT_EQ(0.01, sampling_rate);
  } else {
    EXPECT_EQ(0.0001, sampling_rate);
  }

  sampling_rate = 0.0001;
  if (supports_thread_ticks) {
    EXPECT_TRUE(scheduler_->ShouldIgnoreTaskForUkm(false, &sampling_rate));
  } else {
    EXPECT_FALSE(scheduler_->ShouldIgnoreTaskForUkm(false, &sampling_rate));
    EXPECT_EQ(0.0001, sampling_rate);
  }
}

class CompositingExperimentWithExplicitSignalsTest
    : public MainThreadSchedulerImplTest {
 public:
  CompositingExperimentWithExplicitSignalsTest()
      : MainThreadSchedulerImplTest(
            {kHighPriorityInputOnMainThread, kPrioritizeCompositingAfterInput,
             kUseExplicitSignalForTriggeringCompositingPrioritization,
             kUseWillBeginMainFrameForCompositingPrioritization},
            {}) {}
};

TEST_F(CompositingExperimentWithExplicitSignalsTest, CompositingAfterInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "P1 T1 C1");
  base::RunLoop().RunUntilIdle();
  // Without an explicit signal nothing should be reordered.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("P1"), std::string("T1"),
                                   std::string("C1")));
  run_order.clear();

  scheduler_->OnMainFrameRequestedForInput();

  PostTestTasks(&run_order, "T2 C2 C3");
  base::RunLoop().RunUntilIdle();
  // When a signal is present, compositing tasks should be prioritized until
  // WillBeginMainFrame is received.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("C2"), std::string("C3"),
                                   std::string("T2")));
  run_order.clear();

  scheduler_->WillBeginFrame(viz::BeginFrameArgs());

  PostTestTasks(&run_order, "T3 C4 C5");
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("T3"), std::string("C4"),
                                   std::string("C5")));
  run_order.clear();
}

class CompositingExperimentWithImplicitSignalsTest
    : public MainThreadSchedulerImplTest {
 public:
  CompositingExperimentWithImplicitSignalsTest()
      : MainThreadSchedulerImplTest(
            {kHighPriorityInputOnMainThread, kPrioritizeCompositingAfterInput},
            {kHighestPriorityForCompositingAfterInput,
             kUseExplicitSignalForTriggeringCompositingPrioritization,
             kUseWillBeginMainFrameForCompositingPrioritization}) {}
};

TEST_F(CompositingExperimentWithImplicitSignalsTest, CompositingAfterInput) {
  std::vector<std::string> run_order;
  PostTestTasks(&run_order, "T1 C1 C2 P1 P2");
  base::RunLoop().RunUntilIdle();
  // One compositing task should be prioritized after input.
  EXPECT_THAT(run_order,
              testing::ElementsAre(std::string("P1"), std::string("P2"),
                                   std::string("C1"), std::string("T1"),
                                   std::string("C2")));
}

TEST_F(MainThreadSchedulerImplTest, EQTWithNestedLoop) {
  AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(100));

  RunTask(base::BindLambdaForTesting([&] {
    // After running a task for 10ms, start running a nested loop.
    // This contributes to the first step EQT by 1ms ((10ms)^2 / 2 / 50ms), and
    // the window EQT by 50us (1ms / 20).
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(10));
    scheduler_->OnBeginNestedRunLoop();

    // Leave the loop idle for 20ms.
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(20));

    RunTask(base::BindLambdaForTesting([&] {
      // Run a 30ms task in the nested loop.
      // This contributes to the first step EQT by 8ms ((30ms + 10ms) * 20ms / 2
      // / 50ms), and the window EQT by 400us (8ms / 20). Also, contributes to
      // the second step EQT by 1ms ((10ms)^2 / 2 / 50ms), and the window EQT by
      // 50us (1ms / 20).
      AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(30));
    }));

    // After 40ms idle duration, exit the nested loop.
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(40));
    scheduler_->OnExitNestedRunLoop();

    // The outer task ends after extra 50ms work.
    // This contributes to the third step EQT by 25ms ((50ms)^2 / 2 / 50ms), and
    // the window EQT by 1250us (25ms / 20).
    AdvanceMockTickClockBy(base::TimeDelta::FromMilliseconds(50));
  }));

  EXPECT_THAT(scheduler_->expected_queueing_times(),
              testing::ElementsAre(
                  base::TimeDelta::FromMicroseconds(400 + 50),
                  base::TimeDelta::FromMicroseconds(400 + 50 + 50),
                  base::TimeDelta::FromMicroseconds(400 + 50 + 50 + 1250)));
}

}  // namespace main_thread_scheduler_impl_unittest
}  // namespace scheduler
}  // namespace blink
