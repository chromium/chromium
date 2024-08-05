// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/widget/input/main_thread_event_queue.h"

#include <stddef.h>

#include <new>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/metrics/event_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/scheduler/test/web_mock_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_widget_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

// Simulate a 16ms frame signal.
const base::TimeDelta kFrameInterval = base::Milliseconds(16);

bool Equal(const WebTouchEvent& lhs, const WebTouchEvent& rhs) {
  auto tie = [](const WebTouchEvent& e) {
    return std::make_tuple(
        e.touches_length, e.dispatch_type, e.moved_beyond_slop_region,
        e.hovering, e.touch_start_or_first_touch_move, e.unique_touch_event_id,
        e.GetType(), e.TimeStamp(), e.FrameScale(), e.FrameTranslate(),
        e.GetModifiers());
  };
  if (tie(lhs) != tie(rhs))
    return false;

  for (unsigned i = 0; i < lhs.touches_length; ++i) {
    auto touch_tie = [](const blink::WebTouchPoint& e) {
      return std::make_tuple(e.state, e.radius_x, e.radius_y, e.rotation_angle,
                             e.id, e.tilt_x, e.tilt_y, e.tangential_pressure,
                             e.twist, e.button, e.pointer_type, e.movement_x,
                             e.movement_y, e.is_raw_movement_event,
                             e.PositionInWidget(), e.PositionInScreen());
    };

    if (touch_tie(lhs.touches[i]) != touch_tie(rhs.touches[i]) ||
        (!std::isnan(lhs.touches[i].force) &&
         !std::isnan(rhs.touches[i].force) &&
         lhs.touches[i].force != rhs.touches[i].force))
      return false;
  }

  return true;
}

bool Equal(const WebMouseWheelEvent& lhs, const WebMouseWheelEvent& rhs) {
  auto tie = [](const WebMouseWheelEvent& e) {
    return std::make_tuple(
        e.delta_x, e.delta_y, e.wheel_ticks_x, e.wheel_ticks_y,
        e.acceleration_ratio_x, e.acceleration_ratio_y, e.phase,
        e.momentum_phase, e.rails_mode, e.dispatch_type, e.event_action,
        e.has_synthetic_phase, e.delta_units, e.click_count, e.menu_source_type,
        e.id, e.button, e.movement_x, e.movement_y, e.is_raw_movement_event,
        e.GetType(), e.TimeStamp(), e.FrameScale(), e.FrameTranslate(),
        e.GetModifiers(), e.PositionInWidget(), e.PositionInScreen());
  };
  return tie(lhs) == tie(rhs);
}

}  // namespace

class HandledTask {
 public:
  virtual ~HandledTask() = default;

  virtual blink::WebCoalescedInputEvent* taskAsEvent() = 0;
  virtual unsigned taskAsClosure() const = 0;
  virtual void Print(std::ostream* os) const = 0;

  friend void PrintTo(const HandledTask& task, std::ostream* os) {
    task.Print(os);
  }
};

class HandledEvent : public HandledTask {
 public:
  explicit HandledEvent(const blink::WebCoalescedInputEvent& event)
      : event_(event) {}
  ~HandledEvent() override = default;

  blink::WebCoalescedInputEvent* taskAsEvent() override { return &event_; }
  unsigned taskAsClosure() const override {
    NOTREACHED_IN_MIGRATION();
    return 0;
  }

  void Print(std::ostream* os) const override {
    *os << "event_type: " << event_.Event().GetType();
    if (WebInputEvent::IsTouchEventType(event_.Event().GetType())) {
      auto& touch_event = static_cast<const WebTouchEvent&>(event_.Event());
      *os << " touch_id: " << touch_event.unique_touch_event_id
          << " dispatch_type: " << touch_event.dispatch_type;
    }
  }

 private:
  blink::WebCoalescedInputEvent event_;
};

class HandledClosure : public HandledTask {
 public:
  explicit HandledClosure(unsigned closure_id) : closure_id_(closure_id) {}
  ~HandledClosure() override = default;

  blink::WebCoalescedInputEvent* taskAsEvent() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  unsigned taskAsClosure() const override { return closure_id_; }
  void Print(std::ostream* os) const override { NOTREACHED_IN_MIGRATION(); }

 private:
  unsigned closure_id_;
};

enum class CallbackReceivedState {
  kPending,
  kCalledWhileHandlingEvent,
  kCalledAfterHandleEvent,
};

void PrintTo(CallbackReceivedState state, std::ostream* os) {
  const char* kCallbackReceivedStateToString[] = {
      "Pending", "CalledWhileHandlingEvent", "CalledAfterHandleEvent"};
  *os << kCallbackReceivedStateToString[static_cast<int>(state)];
}

class ReceivedCallback {
 public:
  ReceivedCallback()
      : ReceivedCallback(CallbackReceivedState::kPending, false, kNotFound) {}

  ReceivedCallback(CallbackReceivedState state,
                   bool coalesced_latency,
                   wtf_size_t after_handled_tasks = kNotFound)
      : state_(state),
        coalesced_latency_(coalesced_latency),
        after_handled_tasks_(after_handled_tasks) {}
  bool operator==(const ReceivedCallback& other) const {
    return state_ == other.state_ &&
           coalesced_latency_ == other.coalesced_latency_ &&
           after_handled_tasks_ == other.after_handled_tasks_;
  }
  friend void PrintTo(const ReceivedCallback& callback, std::ostream* os) {
    PrintTo(callback.state_, os);
    if (callback.coalesced_latency_) {
      *os << " coalesced";
    }
    *os << " after_handled_tasks=" << callback.after_handled_tasks_;
  }

 private:
  CallbackReceivedState state_;
  bool coalesced_latency_;
  // The number of handled tasks when the callback is run, for tests to check
  // the order of event handling and callbacks.
  wtf_size_t after_handled_tasks_;
};

class HandledEventCallbackTracker {
 public:
  explicit HandledEventCallbackTracker(
      const Vector<std::unique_ptr<HandledTask>>& handled_tasks)
      : handling_event_(false), handled_tasks_(handled_tasks) {
    weak_this_ = weak_ptr_factory_.GetWeakPtr();
  }

  HandledEventCallback GetCallback() {
    callbacks_received_.push_back(ReceivedCallback());
    HandledEventCallback callback =
        base::BindOnce(&HandledEventCallbackTracker::DidHandleEvent, weak_this_,
                       callbacks_received_.size() - 1);
    return callback;
  }

  void DidHandleEvent(wtf_size_t index,
                      blink::mojom::InputEventResultState ack_result,
                      const ui::LatencyInfo& latency,
                      mojom::blink::DidOverscrollParamsPtr params,
                      std::optional<cc::TouchAction> touch_action) {
    callbacks_received_[index] = ReceivedCallback(
        handling_event_ ? CallbackReceivedState::kCalledWhileHandlingEvent
                        : CallbackReceivedState::kCalledAfterHandleEvent,
        latency.coalesced(), handled_tasks_->size());
  }

  const Vector<ReceivedCallback>& GetReceivedCallbacks() const {
    return callbacks_received_;
  }

  bool handling_event_;

 private:
  Vector<ReceivedCallback> callbacks_received_;
  const raw_ref<const Vector<std::unique_ptr<HandledTask>>> handled_tasks_;
  base::WeakPtr<HandledEventCallbackTracker> weak_this_;
  base::WeakPtrFactory<HandledEventCallbackTracker> weak_ptr_factory_{this};
};

MATCHER_P3(IsHandledTouchEvent, event_type, touch_id, dispatch_type, "") {
  CHECK(WebInputEvent::IsTouchEventType(event_type));
  auto& event = static_cast<const WebTouchEvent&>(arg->taskAsEvent()->Event());
  return event.GetType() == event_type &&
         event.unique_touch_event_id == touch_id &&
         event.dispatch_type == dispatch_type;
}

class MockWidgetScheduler : public scheduler::FakeWidgetScheduler {
 public:
  MockWidgetScheduler() = default;

  MOCK_METHOD3(DidHandleInputEventOnMainThread,
               void(const WebInputEvent&, WebInputEventResult, bool));
};

class MainThreadEventQueueTest : public testing::Test,
                                 public testing::WithParamInterface<bool>,
                                 public MainThreadEventQueueClient,
                                 private ScopedUnblockTouchMoveEarlierForTest {
 public:
  MainThreadEventQueueTest()
      : ScopedUnblockTouchMoveEarlierForTest(GetParam()),
        main_task_runner_(new base::TestSimpleTaskRunner()) {
    widget_scheduler_ = base::MakeRefCounted<MockWidgetScheduler>();
    handler_callback_ =
        std::make_unique<HandledEventCallbackTracker>(handled_tasks_);
  }

  void SetUp() override {
    queue_ = base::MakeRefCounted<MainThreadEventQueue>(
        this, main_task_runner_, main_task_runner_, widget_scheduler_, true);
    queue_->ClearRafFallbackTimerForTesting();
  }

  void HandleEvent(const WebInputEvent& event,
                   blink::mojom::InputEventResultState ack_result) {
    base::AutoReset<bool> in_handle_event(&handler_callback_->handling_event_,
                                          true);
    queue_->HandleEvent(std::make_unique<blink::WebCoalescedInputEvent>(
                            event.Clone(), ui::LatencyInfo()),
                        MainThreadEventQueue::DispatchType::kBlocking,
                        ack_result, blink::WebInputEventAttribution(), nullptr,
                        handler_callback_->GetCallback());
  }

  void RunClosure(unsigned closure_id) {
    auto closure = std::make_unique<HandledClosure>(closure_id);
    handled_tasks_.push_back(std::move(closure));
  }

  void QueueClosure() {
    unsigned closure_id = ++closure_count_;
    queue_->QueueClosure(base::BindOnce(&MainThreadEventQueueTest::RunClosure,
                                        base::Unretained(this), closure_id));
  }

  MainThreadEventQueueTaskList& event_queue() {
    return queue_->shared_state_.events_;
  }

  bool needs_low_latency_until_pointer_up() {
    return queue_->needs_low_latency_until_pointer_up_;
  }

  bool last_touch_start_forced_nonblocking_due_to_fling() {
    return queue_->compositor_thread_only_
        .last_touch_start_forced_nonblocking_due_to_fling;
  }

  void RunPendingTasksWithSimulatedRaf() {
    while (needs_main_frame_ || main_task_runner_->HasPendingTask()) {
      main_task_runner_->RunUntilIdle();
      needs_main_frame_ = false;
      frame_time_ += kFrameInterval;
      queue_->DispatchRafAlignedInput(frame_time_);
    }
  }

  void RunSimulatedRafOnce() {
    if (needs_main_frame_) {
      needs_main_frame_ = false;
      frame_time_ += kFrameInterval;
      queue_->DispatchRafAlignedInput(frame_time_);
    }
  }

  void RunPendingTasksWithoutRaf() { main_task_runner_->RunUntilIdle(); }

  // MainThreadEventQueueClient overrides.
  bool HandleInputEvent(const blink::WebCoalescedInputEvent& event,
                        std::unique_ptr<cc::EventMetrics> metrics,
                        HandledEventCallback callback) override {
    if (will_handle_input_event_callback_) {
      will_handle_input_event_callback_.Run(event);
    }

    if (!handle_input_event_)
      return false;
    auto handled_event = std::make_unique<HandledEvent>(event);
    handled_tasks_.push_back(std::move(handled_event));
    std::move(callback).Run(main_thread_ack_state_, event.latency_info(),
                            nullptr, std::nullopt);
    return true;
  }
  void InputEventsDispatched(bool raf_aligned) override {
    if (raf_aligned)
      raf_aligned_events_dispatched_ = true;
    else
      non_raf_aligned_events_dispatched_ = true;
  }
  void SetNeedsMainFrame() override { needs_main_frame_ = true; }
  bool RequestedMainFramePending() override { return needs_main_frame_; }

  Vector<ReceivedCallback> GetAndResetCallbackResults() {
    std::unique_ptr<HandledEventCallbackTracker> callback =
        std::make_unique<HandledEventCallbackTracker>(handled_tasks_);
    handler_callback_.swap(callback);
    return callback->GetReceivedCallbacks();
  }

  void set_handle_input_event(bool handle) { handle_input_event_ = handle; }

  void set_main_thread_ack_state(blink::mojom::InputEventResultState state) {
    main_thread_ack_state_ = state;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  scoped_refptr<MockWidgetScheduler> widget_scheduler_;
  scoped_refptr<MainThreadEventQueue> queue_;
  Vector<std::unique_ptr<HandledTask>> handled_tasks_;
  std::unique_ptr<HandledEventCallbackTracker> handler_callback_;

  bool needs_main_frame_ = false;
  bool handle_input_event_ = true;
  bool raf_aligned_events_dispatched_ = false;
  bool non_raf_aligned_events_dispatched_ = false;
  base::TimeTicks frame_time_;
  blink::mojom::InputEventResultState main_thread_ack_state_ =
      blink::mojom::InputEventResultState::kNotConsumed;
  unsigned closure_count_ = 0;

  // This allows a test to simulate concurrent action in the compositor thread
  // when the main thread is dispatching events in the queue.
  base::RepeatingCallback<void(const blink::WebCoalescedInputEvent&)>
      will_handle_input_event_callback_;
};

INSTANTIATE_TEST_SUITE_P(All, MainThreadEventQueueTest, ::testing::Bool());

TEST_P(MainThreadEventQueueTest, ClientDoesntHandleInputEvent) {
  // Prevent MainThreadEventQueueClient::HandleInputEvent() from handling the
  // event, and have it return false. Then the MainThreadEventQueue should
  // call the handled callback.
  set_handle_input_event(false);

  // The blocking event used in this test is reported to the scheduler.
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(1);

  // Inject and try to dispatch an input event. This event is not considered
  // "non-blocking" which means the reply callback gets stored with the queued
  // event, and will be run when we work through the queue.
  SyntheticWebTouchEvent event;
  event.PressPoint(10, 10);
  event.MovePoint(0, 20, 20);
  WebMouseWheelEvent event2 = SyntheticWebMouseWheelEventBuilder::Build(
      10, 10, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel);
  HandleEvent(event2, blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();

  Vector<ReceivedCallback> received = GetAndResetCallbackResults();
  // We didn't handle the event in the client method.
  EXPECT_EQ(handled_tasks_.size(), 0u);
  // There's 1 reply callback for our 1 event.
  EXPECT_EQ(received.size(), 1u);
  // The event was queued and disaptched, then the callback was run when
  // the client failed to handle it. If this fails, the callback was run
  // by HandleEvent() without dispatching it (kCalledWhileHandlingEvent)
  // or was not called at all (kPending).
  EXPECT_THAT(received,
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false, 0)));
}

TEST_P(MainThreadEventQueueTest, NonBlockingWheel) {
  WebMouseWheelEvent kEvents[4] = {
      SyntheticWebMouseWheelEventBuilder::Build(
          10, 10, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel),
      SyntheticWebMouseWheelEventBuilder::Build(
          20, 20, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel),
      SyntheticWebMouseWheelEventBuilder::Build(
          30, 30, 0, 53, 1, ui::ScrollGranularity::kScrollByPixel),
      SyntheticWebMouseWheelEventBuilder::Build(
          30, 30, 0, 53, 1, ui::ScrollGranularity::kScrollByPixel),
  };

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  for (WebMouseWheelEvent& event : kEvents)
    HandleEvent(event, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  for (const auto& task : handled_tasks_) {
    EXPECT_EQ(2u, task->taskAsEvent()->CoalescedEventSize());
  }

  {
    EXPECT_EQ(kEvents[0].GetType(),
              handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_tasks_.at(0)->taskAsEvent()->EventPointer());
    EXPECT_EQ(WebInputEvent::DispatchType::kListenersNonBlockingPassive,
              last_wheel_event->dispatch_type);
    WebMouseWheelEvent coalesced_event = kEvents[0];
    coalesced_event.Coalesce(kEvents[1]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *last_wheel_event));
  }

  {
    WebMouseWheelEvent coalesced_event = kEvents[0];
    const auto& coalesced_events =
        handled_tasks_[0]->taskAsEvent()->GetCoalescedEventsPointers();
    const WebMouseWheelEvent* coalesced_wheel_event0 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[0].get());
    EXPECT_TRUE(Equal(coalesced_event, *coalesced_wheel_event0));

    coalesced_event = kEvents[1];
    const WebMouseWheelEvent* coalesced_wheel_event1 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[1].get());
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *coalesced_wheel_event1));
  }

  {
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_tasks_.at(1)->taskAsEvent()->EventPointer());
    WebMouseWheelEvent coalesced_event = kEvents[2];
    coalesced_event.Coalesce(kEvents[3]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *last_wheel_event));
  }

  {
    WebMouseWheelEvent coalesced_event = kEvents[2];
    const auto& coalesced_events =
        handled_tasks_[1]->taskAsEvent()->GetCoalescedEventsPointers();
    const WebMouseWheelEvent* coalesced_wheel_event0 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[0].get());
    EXPECT_TRUE(Equal(coalesced_event, *coalesced_wheel_event0));

    coalesced_event = kEvents[3];
    const WebMouseWheelEvent* coalesced_wheel_event1 =
        static_cast<const WebMouseWheelEvent*>(coalesced_events[1].get());
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *coalesced_wheel_event1));
  }
}

TEST_P(MainThreadEventQueueTest, NonBlockingTouch) {
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].SetModifiers(1);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_tasks_.size());

  EXPECT_EQ(kEvents[0].GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(0)->taskAsEvent()->EventPointer());
  SyntheticWebTouchEvent non_blocking_touch = kEvents[0];
  non_blocking_touch.dispatch_type =
      WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  EXPECT_TRUE(Equal(non_blocking_touch, *last_touch_event));

  {
    EXPECT_EQ(1u, handled_tasks_[0]->taskAsEvent()->CoalescedEventSize());
    const WebTouchEvent* coalesced_touch_event =
        static_cast<const WebTouchEvent*>(handled_tasks_[0]
                                              ->taskAsEvent()
                                              ->GetCoalescedEventsPointers()[0]
                                              .get());
    EXPECT_TRUE(Equal(kEvents[0], *coalesced_touch_event));
  }

  EXPECT_EQ(kEvents[1].GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(1)->taskAsEvent()->EventPointer());
  non_blocking_touch = kEvents[1];
  non_blocking_touch.dispatch_type =
      WebInputEvent::DispatchType::kListenersNonBlockingPassive;
  EXPECT_TRUE(Equal(non_blocking_touch, *last_touch_event));

  {
    EXPECT_EQ(1u, handled_tasks_[1]->taskAsEvent()->CoalescedEventSize());
    const WebTouchEvent* coalesced_touch_event =
        static_cast<const WebTouchEvent*>(handled_tasks_[1]
                                              ->taskAsEvent()
                                              ->GetCoalescedEventsPointers()[0]
                                              .get());
    EXPECT_TRUE(Equal(kEvents[1], *coalesced_touch_event));
  }

  {
    EXPECT_EQ(kEvents[2].GetType(),
              handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
    last_touch_event = static_cast<const WebTouchEvent*>(
        handled_tasks_.at(2)->taskAsEvent()->EventPointer());
    WebTouchEvent coalesced_event = kEvents[2];
    coalesced_event.Coalesce(kEvents[3]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *last_touch_event));
  }

  {
    EXPECT_EQ(2u, handled_tasks_[2]->taskAsEvent()->CoalescedEventSize());
    WebTouchEvent coalesced_event = kEvents[2];
    const auto& coalesced_events =
        handled_tasks_[2]->taskAsEvent()->GetCoalescedEventsPointers();
    const WebTouchEvent* coalesced_touch_event0 =
        static_cast<const WebTouchEvent*>(coalesced_events[0].get());
    EXPECT_TRUE(Equal(coalesced_event, *coalesced_touch_event0));

    coalesced_event = kEvents[3];
    const WebTouchEvent* coalesced_touch_event1 =
        static_cast<const WebTouchEvent*>(coalesced_events[1].get());
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *coalesced_touch_event1));
  }
}

TEST_P(MainThreadEventQueueTest, BlockingTouch) {
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(3);
  {
    // Ensure that coalescing takes place.
    HandleEvent(kEvents[0],
                blink::mojom::InputEventResultState::kSetNonBlocking);
    HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
    HandleEvent(kEvents[2], blink::mojom::InputEventResultState::kNotConsumed);
    HandleEvent(kEvents[3], blink::mojom::InputEventResultState::kNotConsumed);

    EXPECT_EQ(2u, event_queue().size());
    EXPECT_TRUE(main_task_runner_->HasPendingTask());
    RunPendingTasksWithSimulatedRaf();

    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledWhileHandlingEvent,
                             false, 0),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 2)));
    EXPECT_EQ(0u, event_queue().size());

    const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
        handled_tasks_.at(1)->taskAsEvent()->EventPointer());
    EXPECT_EQ(kEvents[1].unique_touch_event_id,
              last_touch_event->unique_touch_event_id);
  }

  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(kEvents[2], blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(kEvents[3], blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 2)));
}

TEST_P(MainThreadEventQueueTest, InterleavedEvents) {
  WebMouseWheelEvent kWheelEvents[2] = {
      SyntheticWebMouseWheelEventBuilder::Build(
          10, 10, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel),
      SyntheticWebMouseWheelEventBuilder::Build(
          20, 20, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel),
  };
  SyntheticWebTouchEvent kTouchEvents[2];
  kTouchEvents[0].PressPoint(10, 10);
  kTouchEvents[0].MovePoint(0, 20, 20);
  kTouchEvents[1].PressPoint(10, 10);
  kTouchEvents[1].MovePoint(0, 30, 30);

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  HandleEvent(kWheelEvents[0],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(kTouchEvents[0],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(kWheelEvents[1],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(kTouchEvents[1],
              blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  {
    EXPECT_EQ(kWheelEvents[0].GetType(),
              handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
    const WebMouseWheelEvent* last_wheel_event =
        static_cast<const WebMouseWheelEvent*>(
            handled_tasks_.at(0)->taskAsEvent()->EventPointer());
    EXPECT_EQ(WebInputEvent::DispatchType::kListenersNonBlockingPassive,
              last_wheel_event->dispatch_type);
    WebMouseWheelEvent coalesced_event = kWheelEvents[0];
    coalesced_event.Coalesce(kWheelEvents[1]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *last_wheel_event));
  }
  {
    EXPECT_EQ(kTouchEvents[0].GetType(),
              handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
    const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
        handled_tasks_.at(1)->taskAsEvent()->EventPointer());
    WebTouchEvent coalesced_event = kTouchEvents[0];
    coalesced_event.Coalesce(kTouchEvents[1]);
    coalesced_event.dispatch_type =
        WebInputEvent::DispatchType::kListenersNonBlockingPassive;
    EXPECT_TRUE(Equal(coalesced_event, *last_touch_event));
  }
}

TEST_P(MainThreadEventQueueTest, RafAlignedMouseInput) {
  WebMouseEvent mouseDown = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseDown, 10, 10, 0);

  WebMouseEvent mouseMove = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10, 0);

  WebMouseEvent mouseUp = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseUp, 10, 10, 0);

  WebMouseWheelEvent wheelEvents[3] = {
      SyntheticWebMouseWheelEventBuilder::Build(
          10, 10, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel),
      SyntheticWebMouseWheelEventBuilder::Build(
          20, 20, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel),
      SyntheticWebMouseWheelEventBuilder::Build(
          20, 20, 0, 53, 1, ui::ScrollGranularity::kScrollByPixel),
  };

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  // Simulate enqueing a discrete event, followed by continuous events and
  // then a discrete event. The last discrete event should flush the
  // continuous events so the aren't aligned to rAF and are processed
  // immediately.
  HandleEvent(mouseDown, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(mouseMove, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(wheelEvents[0],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(wheelEvents[1],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(mouseUp, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(4u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));

  // Simulate the rAF running before the PostTask occurs. The rAF
  // will consume everything.
  HandleEvent(mouseDown, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(wheelEvents[0],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  RunSimulatedRafOnce();
  EXPECT_FALSE(needs_main_frame_);
  EXPECT_EQ(0u, event_queue().size());
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 4)));

  // Simulate event consumption but no rAF signal. The mouse wheel events
  // should still be in the queue.
  handled_tasks_.clear();
  HandleEvent(mouseDown, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(wheelEvents[0],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(mouseUp, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(wheelEvents[2],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(wheelEvents[0],
              blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(5u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_EQ(2u, event_queue().size());
  RunSimulatedRafOnce();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));
  EXPECT_EQ(wheelEvents[2].GetModifiers(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetModifiers());
  EXPECT_EQ(wheelEvents[0].GetModifiers(),
            handled_tasks_.at(4)->taskAsEvent()->Event().GetModifiers());
}

TEST_P(MainThreadEventQueueTest, RafAlignedTouchInput) {
  SyntheticWebTouchEvent kEvents[3];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 50, 50);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].ReleasePoint(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(3);

  // Simulate enqueing a discrete event, followed by continuous events and
  // then a discrete event. The last discrete event should flush the
  // continuous events so the aren't aligned to rAF and are processed
  // immediately.
  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));

  // Simulate the rAF running before the PostTask occurs. The rAF
  // will consume everything.
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  RunSimulatedRafOnce();
  EXPECT_FALSE(needs_main_frame_);
  EXPECT_EQ(0u, event_queue().size());
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 3)));

  // Simulate event consumption but no rAF signal. The touch events
  // should still be in the queue.
  handled_tasks_.clear();
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_EQ(1u, event_queue().size());
  RunSimulatedRafOnce();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));

  // Simulate the touch move being discrete
  kEvents[0].touch_start_or_first_touch_move = true;
  kEvents[1].touch_start_or_first_touch_move = true;

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(
      GetAndResetCallbackResults(),
      testing::ElementsAre(
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 3),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 4),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 5)));
}

TEST_P(MainThreadEventQueueTest, RafAlignedTouchInputCoalescedMoves) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 50, 50);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[0].dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(4);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  {
    // Send a non-blocking input event and then blocking  event.
    // The events should coalesce together.
    HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
    EXPECT_EQ(1u, event_queue().size());
    EXPECT_FALSE(main_task_runner_->HasPendingTask());
    EXPECT_TRUE(needs_main_frame_);
    HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
    EXPECT_EQ(1u, event_queue().size());
    EXPECT_FALSE(main_task_runner_->HasPendingTask());
    EXPECT_TRUE(needs_main_frame_);
    RunPendingTasksWithSimulatedRaf();
    EXPECT_EQ(0u, event_queue().size());
    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledWhileHandlingEvent,
                             false, 0),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 1)));
  }

  // Send a non-cancelable ack required event, and then a non-ack
  // required event they should be coalesced together.
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 1)));

  // Send a non-ack required event, and then a non-cancelable ack
  // required event they should be coalesced together.
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 2)));
}

TEST_P(MainThreadEventQueueTest, RafAlignedTouchInputThrottlingMoves) {
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(3);

  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 50, 50);
  kEvents[0].dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[1].dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  // Send a non-cancelable touch move and then send it another one. The
  // second one shouldn't go out with the next rAF call and should be throttled.
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);

  // Event should still be in queue after handling a single rAF call.
  RunSimulatedRafOnce();
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);

  // And should eventually flush.
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 1)));
  EXPECT_EQ(0u, event_queue().size());
}

TEST_P(MainThreadEventQueueTest, LowLatency) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 50, 50);

  queue_->SetNeedsLowLatency(true);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());

  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10, 0);
  WebMouseWheelEvent mouse_wheel = SyntheticWebMouseWheelEventBuilder::Build(
      10, 10, 0, 53, 0, ui::ScrollGranularity::kScrollByPixel);

  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(mouse_wheel,
              blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 2)));
  EXPECT_EQ(0u, event_queue().size());

  // Now turn off low latency mode.
  queue_->SetNeedsLowLatency(false);
  for (SyntheticWebTouchEvent& event : kEvents)
    HandleEvent(event, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 4)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());

  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(mouse_wheel,
              blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(2u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_TRUE(needs_main_frame_);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 6)));
  EXPECT_EQ(0u, event_queue().size());
}

TEST_P(MainThreadEventQueueTest, BlockingTouchesDuringFling) {
  SyntheticWebTouchEvent kEvents;
  kEvents.PressPoint(10, 10);
  kEvents.touch_start_or_first_touch_move = true;

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(4);

  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  HandleEvent(kEvents,
              blink::mojom::InputEventResultState::kSetNonBlockingDueToFling);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 0)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(0)->taskAsEvent()->EventPointer());
  kEvents.dispatch_type =
      WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));

  kEvents.MovePoint(0, 30, 30);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  HandleEvent(kEvents,
              blink::mojom::InputEventResultState::kSetNonBlockingDueToFling);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledWhileHandlingEvent, false, 1)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  EXPECT_TRUE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(1)->taskAsEvent()->EventPointer());
  kEvents.dispatch_type =
      WebInputEvent::DispatchType::kListenersForcedNonBlockingDueToFling;
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));

  kEvents.MovePoint(0, 50, 50);
  kEvents.touch_start_or_first_touch_move = false;
  HandleEvent(kEvents,
              blink::mojom::InputEventResultState::kSetNonBlockingDueToFling);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false, 3)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::DispatchType::kBlocking);
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(2)->taskAsEvent()->EventPointer());
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));

  kEvents.ReleasePoint(0);
  HandleEvent(kEvents,
              blink::mojom::InputEventResultState::kSetNonBlockingDueToFling);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false, 4)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::DispatchType::kBlocking);
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(3)->taskAsEvent()->EventPointer());
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));
}

TEST_P(MainThreadEventQueueTest, BlockingTouchesOutsideFling) {
  SyntheticWebTouchEvent kEvents;
  kEvents.PressPoint(10, 10);
  kEvents.touch_start_or_first_touch_move = true;

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(4);

  HandleEvent(kEvents, blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false, 1)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::DispatchType::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  const WebTouchEvent* last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(0)->taskAsEvent()->EventPointer());
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));

  HandleEvent(kEvents, blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false, 2)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::DispatchType::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(1)->taskAsEvent()->EventPointer());
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));

  HandleEvent(kEvents, blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false, 3)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::DispatchType::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(2)->taskAsEvent()->EventPointer());
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));

  kEvents.MovePoint(0, 30, 30);
  HandleEvent(kEvents, blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(GetAndResetCallbackResults(),
              testing::Each(ReceivedCallback(
                  CallbackReceivedState::kCalledAfterHandleEvent, false, 4)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(4u, handled_tasks_.size());
  EXPECT_EQ(kEvents.GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(kEvents.dispatch_type, WebInputEvent::DispatchType::kBlocking);
  EXPECT_FALSE(last_touch_start_forced_nonblocking_due_to_fling());
  last_touch_event = static_cast<const WebTouchEvent*>(
      handled_tasks_.at(3)->taskAsEvent()->EventPointer());
  EXPECT_TRUE(Equal(kEvents, *last_touch_event));
}

TEST_P(MainThreadEventQueueTest, QueueingEventTimestampRecorded) {
  WebMouseEvent kEvent = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::Type::kMouseDown);
  // Set event timestamp to be in the past to simulate actual event
  // so that creation of event and queueing does not happen in the same tick.
  kEvent.SetTimeStamp(base::TimeTicks::Now() - base::Microseconds(10));

  HandleEvent(kEvent, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(1u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  RunPendingTasksWithoutRaf();
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(1u, handled_tasks_.size());

  EXPECT_EQ(kEvent.GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  const WebMouseEvent* kHandledEvent = static_cast<const WebMouseEvent*>(
      handled_tasks_.at(0)->taskAsEvent()->EventPointer());
  EXPECT_LT(kHandledEvent->TimeStamp(), kHandledEvent->QueuedTimeStamp());
}

TEST_P(MainThreadEventQueueTest, QueuingTwoClosures) {
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  QueueClosure();
  QueueClosure();
  EXPECT_EQ(2u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, handled_tasks_.at(0)->taskAsClosure());
  EXPECT_EQ(2u, handled_tasks_.at(1)->taskAsClosure());
}

TEST_P(MainThreadEventQueueTest, QueuingClosureWithRafEvent) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);

  // Simulate queueuing closure, event, closure, raf aligned event.
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  QueueClosure();
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(2);

  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  QueueClosure();
  EXPECT_EQ(3u, event_queue().size());
  EXPECT_TRUE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(4u, event_queue().size());

  EXPECT_TRUE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();

  // The queue should still have the rAF event.
  EXPECT_TRUE(needs_main_frame_);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();

  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(
      GetAndResetCallbackResults(),
      testing::ElementsAre(
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 2),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 4)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_EQ(1u, handled_tasks_.at(0)->taskAsClosure());
  EXPECT_EQ(kEvents[0].GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(2u, handled_tasks_.at(2)->taskAsClosure());
  EXPECT_EQ(kEvents[1].GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
}

TEST_P(MainThreadEventQueueTest, QueuingClosuresBetweenEvents) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].ReleasePoint(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(2);

  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  QueueClosure();
  QueueClosure();
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(4u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);
  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_THAT(
      GetAndResetCallbackResults(),
      testing::ElementsAre(
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 1),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 4)));
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);

  EXPECT_EQ(kEvents[0].GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(1u, handled_tasks_.at(1)->taskAsClosure());
  EXPECT_EQ(2u, handled_tasks_.at(2)->taskAsClosure());
  EXPECT_EQ(kEvents[1].GetType(),
            handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
}

TEST_P(MainThreadEventQueueTest, BlockingTouchMoveBecomesNonBlocking) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 20, 20);
  kEvents[1].SetModifiers(1);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 30);
  kEvents[1].dispatch_type = WebInputEvent::DispatchType::kEventNonBlocking;
  WebTouchEvent scroll_start(WebInputEvent::Type::kTouchScrollStarted,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(3);
  EXPECT_EQ(WebInputEvent::DispatchType::kBlocking, kEvents[0].dispatch_type);
  EXPECT_EQ(WebInputEvent::DispatchType::kEventNonBlocking,
            kEvents[1].dispatch_type);
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(scroll_start, blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(3u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(
      GetAndResetCallbackResults(),
      testing::ElementsAre(
          ReceivedCallback(
              CallbackReceivedState::kCalledAfterHandleEvent, false,
              RuntimeEnabledFeatures::UnblockTouchMoveEarlierEnabled() ? 0 : 1),
          ReceivedCallback(CallbackReceivedState::kCalledWhileHandlingEvent,
                           false, 0),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 3)));
  EXPECT_THAT(
      handled_tasks_,
      ::testing::ElementsAre(
          IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                              kEvents[0].unique_touch_event_id,
                              WebInputEvent::DispatchType::kEventNonBlocking),
          IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                              kEvents[1].unique_touch_event_id,
                              WebInputEvent::DispatchType::kEventNonBlocking),
          IsHandledTouchEvent(WebInputEvent::Type::kTouchScrollStarted,
                              scroll_start.unique_touch_event_id,
                              WebInputEvent::DispatchType::kBlocking)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
}

TEST_P(MainThreadEventQueueTest, BlockingTouchMoveWithTouchEnd) {
  SyntheticWebTouchEvent kEvents[2];
  kEvents[0].PressPoint(10, 10);
  kEvents[0].MovePoint(0, 20, 20);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].ReleasePoint(0);
  WebTouchEvent scroll_start(WebInputEvent::Type::kTouchScrollStarted,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(3);
  EXPECT_EQ(WebInputEvent::DispatchType::kBlocking, kEvents[0].dispatch_type);
  EXPECT_EQ(WebInputEvent::DispatchType::kBlocking, kEvents[1].dispatch_type);
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(scroll_start, blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(3u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(
      GetAndResetCallbackResults(),
      testing::ElementsAre(
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 1),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 2),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 3)));
  EXPECT_THAT(handled_tasks_,
              ::testing::ElementsAre(
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                      kEvents[0].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchEnd,
                                      kEvents[1].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchScrollStarted,
                                      scroll_start.unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking)));
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
}

TEST_P(MainThreadEventQueueTest,
       UnblockTouchMoveAfterTouchStartAndFirstTouchMoveNotConsumed) {
  SyntheticWebTouchEvent touch_start;
  touch_start.PressPoint(10, 10);
  touch_start.touch_start_or_first_touch_move = true;
  ASSERT_EQ(WebInputEvent::Type::kTouchStart, touch_start.GetType());
  ASSERT_EQ(WebInputEvent::DispatchType::kBlocking, touch_start.dispatch_type);

  SyntheticWebTouchEvent touch_moves[5];
  for (auto& touch_move : touch_moves) {
    touch_move.MovePoint(0, 20, 30);
    ASSERT_EQ(WebInputEvent::Type::kTouchMove, touch_move.GetType());
    ASSERT_EQ(WebInputEvent::DispatchType::kBlocking, touch_move.dispatch_type);
  }
  touch_moves[0].touch_start_or_first_touch_move = true;

  struct WillHandleInputEventCallback {
    STACK_ALLOCATED();

   public:
    void Run(const WebCoalescedInputEvent& event) {
      test.set_main_thread_ack_state(
          blink::mojom::InputEventResultState::kNotConsumed);
      if (event.Event().GetType() == WebInputEvent::Type::kTouchStart &&
          consume_touch_start) {
        test.set_main_thread_ack_state(
            blink::mojom::InputEventResultState::kConsumed);
      }
      auto touch_id = static_cast<const WebTouchEvent&>(event.Event())
                          .unique_touch_event_id;
      if (touch_id == touch_moves[0].unique_touch_event_id) {
        if (consume_first_touch_move) {
          test.set_main_thread_ack_state(
              blink::mojom::InputEventResultState::kConsumed);
        }
        // Simulates two new blocking touchmove events enqueued while the
        // first touchmove is being dispatched.
        test.HandleEvent(touch_moves[1],
                         blink::mojom::InputEventResultState::kNotConsumed);
        test.HandleEvent(touch_moves[2],
                         blink::mojom::InputEventResultState::kNotConsumed);
      } else if (touch_id == touch_moves[1].unique_touch_event_id) {
        // Simulates two new blocking touchmove events enqueued while the
        // second touchmove is being dispatched.
        test.HandleEvent(touch_moves[3],
                         blink::mojom::InputEventResultState::kNotConsumed);
        test.HandleEvent(touch_moves[4],
                         blink::mojom::InputEventResultState::kNotConsumed);
      }
    }

    MainThreadEventQueueTest& test;
    const SyntheticWebTouchEvent* touch_moves;
    bool consume_touch_start = false;
    bool consume_first_touch_move = false;
  };
  WillHandleInputEventCallback will_handle_input_event_callback{*this,
                                                                touch_moves};

  will_handle_input_event_callback_ =
      base::BindRepeating(&WillHandleInputEventCallback::Run,
                          base::Unretained(&will_handle_input_event_callback));

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(6);
  HandleEvent(touch_start, blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(touch_moves[0],
              blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(2u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_FALSE(needs_main_frame_);
  if (RuntimeEnabledFeatures::UnblockTouchMoveEarlierEnabled()) {
    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 1),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 2),
            // These callbacks were run just after handling the first touchmove.
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 2),
            // These callbacks were run just after handling the second
            // touchmove.
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 3),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 3)));
    EXPECT_THAT(
        handled_tasks_,
        ::testing::ElementsAre(
            // touch_start should remain blocking.
            IsHandledTouchEvent(WebInputEvent::Type::kTouchStart,
                                touch_start.unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            // touch_moves[0] should remain blocking.
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[0].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            // touch_moves[1] was unblocked while it was in the queue.
            // touch_moves[2] was coalesced into touch_moves[1].
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[1].unique_touch_event_id,
                                WebInputEvent::DispatchType::kEventNonBlocking),
            // touch_moves[3] was unblocked while it was in the queue.
            // touch_moves[4] was coalesced into touch_moves[3].
            IsHandledTouchEvent(
                WebInputEvent::Type::kTouchMove,
                touch_moves[3].unique_touch_event_id,
                WebInputEvent::DispatchType::kEventNonBlocking)));
  } else {
    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 1),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 3),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 3),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 4),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 4)));
    EXPECT_THAT(
        handled_tasks_,
        ::testing::ElementsAre(
            IsHandledTouchEvent(WebInputEvent::Type::kTouchStart,
                                touch_start.unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[0].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[1].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[3].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking)));
  }

  // Start another touch sequence, with the first touch_move consumed. This
  // is not in a standalone test case to test the last unblocking status won't
  // leak into this sequence.
  handled_tasks_.clear();
  will_handle_input_event_callback.consume_first_touch_move = true;
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(6);
  HandleEvent(touch_start, blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(touch_moves[0],
              blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(
      GetAndResetCallbackResults(),
      testing::ElementsAre(
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 1),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 2),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 3),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent, true,
                           3),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 4),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent, true,
                           4)));
  EXPECT_THAT(handled_tasks_,
              ::testing::ElementsAre(
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchStart,
                                      touch_start.unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                      touch_moves[0].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                      touch_moves[1].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                      touch_moves[3].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking)));

  // Start another touch sequence, with the touch start consumed.
  handled_tasks_.clear();
  will_handle_input_event_callback.consume_touch_start = true;
  will_handle_input_event_callback.consume_first_touch_move = false;
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(6);
  HandleEvent(touch_start, blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(touch_moves[0],
              blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();
  EXPECT_THAT(
      GetAndResetCallbackResults(),
      testing::ElementsAre(
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 1),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 2),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 3),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent, true,
                           3),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                           false, 4),
          ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent, true,
                           4)));
  EXPECT_THAT(handled_tasks_,
              ::testing::ElementsAre(
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchStart,
                                      touch_start.unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                      touch_moves[0].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                      touch_moves[1].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking),
                  IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                      touch_moves[3].unique_touch_event_id,
                                      WebInputEvent::DispatchType::kBlocking)));

  // Start another touch sequence, neither the touch start nor the first touch
  // move are consumed, like the first touch sequence.
  handled_tasks_.clear();
  will_handle_input_event_callback.consume_touch_start = false;
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(6);
  HandleEvent(touch_start, blink::mojom::InputEventResultState::kNotConsumed);
  HandleEvent(touch_moves[0],
              blink::mojom::InputEventResultState::kNotConsumed);
  RunPendingTasksWithSimulatedRaf();
  if (RuntimeEnabledFeatures::UnblockTouchMoveEarlierEnabled()) {
    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 1),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 3),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 3)));
    EXPECT_THAT(
        handled_tasks_,
        ::testing::ElementsAre(
            IsHandledTouchEvent(WebInputEvent::Type::kTouchStart,
                                touch_start.unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[0].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[1].unique_touch_event_id,
                                WebInputEvent::DispatchType::kEventNonBlocking),
            IsHandledTouchEvent(
                WebInputEvent::Type::kTouchMove,
                touch_moves[3].unique_touch_event_id,
                WebInputEvent::DispatchType::kEventNonBlocking)));
  } else {
    EXPECT_THAT(
        GetAndResetCallbackResults(),
        testing::ElementsAre(
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 1),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 2),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 3),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 3),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             false, 4),
            ReceivedCallback(CallbackReceivedState::kCalledAfterHandleEvent,
                             true, 4)));
    EXPECT_THAT(
        handled_tasks_,
        ::testing::ElementsAre(
            IsHandledTouchEvent(WebInputEvent::Type::kTouchStart,
                                touch_start.unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[0].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[1].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking),
            IsHandledTouchEvent(WebInputEvent::Type::kTouchMove,
                                touch_moves[3].unique_touch_event_id,
                                WebInputEvent::DispatchType::kBlocking)));
  }
}

TEST_P(MainThreadEventQueueTest, UnbufferedDispatchTouchEvent) {
  SyntheticWebTouchEvent kEvents[3];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].ReleasePoint(0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(3);

  EXPECT_EQ(WebInputEvent::DispatchType::kBlocking, kEvents[0].dispatch_type);
  EXPECT_EQ(WebInputEvent::DispatchType::kBlocking, kEvents[1].dispatch_type);
  HandleEvent(kEvents[0], blink::mojom::InputEventResultState::kNotConsumed);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(kEvents[1], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(kEvents[2], blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);
}

TEST_P(MainThreadEventQueueTest, PointerEventsCoalescing) {
  queue_->SetHasPointerRawUpdateEventHandlers(true);
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10, 0);
  SyntheticWebTouchEvent touch_move;
  touch_move.PressPoint(10, 10);
  touch_move.MovePoint(0, 50, 50);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(touch_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(4u, event_queue().size());

  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(touch_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(touch_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(4u, event_queue().size());

  main_task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, event_queue().size());

  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);
}

TEST_P(MainThreadEventQueueTest, PointerRawUpdateEvents) {
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10, 0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);

  queue_->SetHasPointerRawUpdateEventHandlers(true);
  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(2u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);

  queue_->SetHasPointerRawUpdateEventHandlers(false);
  SyntheticWebTouchEvent touch_move;
  touch_move.PressPoint(10, 10);
  touch_move.MovePoint(0, 50, 50);
  HandleEvent(touch_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);

  queue_->SetHasPointerRawUpdateEventHandlers(true);
  HandleEvent(touch_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(2u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);
}

TEST_P(MainThreadEventQueueTest, UnbufferedDispatchMouseEvent) {
  WebMouseEvent mouse_down = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseDown, 10, 10, 0);
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10, 0);
  WebMouseEvent mouse_up = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseUp, 10, 10, 0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  HandleEvent(mouse_down, blink::mojom::InputEventResultState::kSetNonBlocking);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_TRUE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);

  HandleEvent(mouse_up, blink::mojom::InputEventResultState::kSetNonBlocking);
  queue_->RequestUnbufferedInputEvents();
  EXPECT_EQ(1u, event_queue().size());
  RunPendingTasksWithSimulatedRaf();
  EXPECT_FALSE(needs_low_latency_until_pointer_up());
  EXPECT_FALSE(needs_main_frame_);
}

// This test verifies that the events marked with kRelativeMotionEvent modifier
// are not coalesced with other events. During pointer lock,
// kRelativeMotionEvent is sent to the Renderer only to update the new screen
// position. Events of this kind shouldn't be dispatched or coalesced.
TEST_P(MainThreadEventQueueTest, PointerEventsWithRelativeMotionCoalescing) {
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10, 0);

  EXPECT_FALSE(main_task_runner_->HasPendingTask());
  EXPECT_EQ(0u, event_queue().size());

  // Non blocking events are not reported to the scheduler.
  EXPECT_CALL(*widget_scheduler_, DidHandleInputEventOnMainThread(
                                      testing::_, testing::_, testing::_))
      .Times(0);

  queue_->SetHasPointerRawUpdateEventHandlers(true);

  // Inject two mouse move events. For each event injected, there will be two
  // events in the queue. One for kPointerRawUpdate and another kMouseMove
  // event.
  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(2u, event_queue().size());
  // When another event of the same kind is injected, it is coalesced with the
  // previous event, hence queue size doesn't change.
  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(2u, event_queue().size());

  // Inject a kRelativeMotionEvent, which cannot be coalesced. Thus, the queue
  // size should increase.
  WebMouseEvent fake_mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10,
      blink::WebInputEvent::Modifiers::kRelativeMotionEvent);
  HandleEvent(fake_mouse_move,
              blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(4u, event_queue().size());

  // Lastly inject another mouse move event. Since it cannot be coalesced with
  // previous event, which is a kRelativeMotionEvent, expect the queue size to
  // increase again.
  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);
  EXPECT_EQ(6u, event_queue().size());

  RunPendingTasksWithSimulatedRaf();
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_FALSE(needs_main_frame_);
  EXPECT_FALSE(main_task_runner_->HasPendingTask());

  // For the 4 events injected, verify that the queue size should be 6, that is
  // 3 kPointerRawUpdate events and 3 kMouseMove events.
  EXPECT_EQ(6u, handled_tasks_.size());
  {
    // The first event should have a |CoalescedEventSize| of 2, since two events
    // of the same kind are coalesced.
    EXPECT_EQ(WebInputEvent::Type::kPointerRawUpdate,
              handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
    EXPECT_EQ(2u, handled_tasks_.at(0)->taskAsEvent()->CoalescedEventSize());
  }
  {
    // The second event is a kRelativeMotionEvent, it cannot be coalesced, so
    // the |CoalescedEventSize| should be 1.
    EXPECT_EQ(WebInputEvent::Type::kPointerRawUpdate,
              handled_tasks_.at(1)->taskAsEvent()->Event().GetType());
    EXPECT_EQ(1u, handled_tasks_.at(1)->taskAsEvent()->CoalescedEventSize());
    EXPECT_EQ(blink::WebInputEvent::Modifiers::kRelativeMotionEvent,
              handled_tasks_.at(1)->taskAsEvent()->Event().GetModifiers());
  }
  {
    // The third event cannot be coalesced with the previous kPointerRawUpdate,
    // so |CoalescedEventSize| should be 1.
    EXPECT_EQ(WebInputEvent::Type::kPointerRawUpdate,
              handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
    EXPECT_EQ(1u, handled_tasks_.at(2)->taskAsEvent()->CoalescedEventSize());
  }
  {
    // The fourth event should have a |CoalescedEventSize| of 2, since two
    // events of the same kind are coalesced.
    EXPECT_EQ(WebInputEvent::Type::kMouseMove,
              handled_tasks_.at(3)->taskAsEvent()->Event().GetType());
    EXPECT_EQ(2u, handled_tasks_.at(3)->taskAsEvent()->CoalescedEventSize());
  }
  {
    // The fifth event is a kRelativeMotionEvent, it cannot be coalesced, so
    // the |CoalescedEventSize| should be 1.
    EXPECT_EQ(WebInputEvent::Type::kMouseMove,
              handled_tasks_.at(4)->taskAsEvent()->Event().GetType());
    EXPECT_EQ(1u, handled_tasks_.at(4)->taskAsEvent()->CoalescedEventSize());
    EXPECT_EQ(blink::WebInputEvent::Modifiers::kRelativeMotionEvent,
              handled_tasks_.at(4)->taskAsEvent()->Event().GetModifiers());
  }
  {
    // The sixth event cannot be coalesced with the previous kMouseMove,
    // so |CoalescedEventSize| should be 1.
    EXPECT_EQ(WebInputEvent::Type::kMouseMove,
              handled_tasks_.at(5)->taskAsEvent()->Event().GetType());
    EXPECT_EQ(1u, handled_tasks_.at(5)->taskAsEvent()->CoalescedEventSize());
  }
}

// Verifies that after rAF-aligned or non-rAF-aligned events are dispatched,
// clients are notified that the dispatch is done.
TEST_P(MainThreadEventQueueTest, InputEventsDispatchedNotified) {
  WebKeyboardEvent key_down(WebInputEvent::Type::kRawKeyDown, 0,
                            base::TimeTicks::Now());
  WebKeyboardEvent key_up(WebInputEvent::Type::kKeyUp, 0,
                          base::TimeTicks::Now());
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::Type::kMouseMove, 10, 10, 0);

  // Post two non-rAF-aligned events.
  HandleEvent(key_down, blink::mojom::InputEventResultState::kSetNonBlocking);
  HandleEvent(key_up, blink::mojom::InputEventResultState::kSetNonBlocking);

  // Post one rAF-aligned event.
  HandleEvent(mouse_move, blink::mojom::InputEventResultState::kSetNonBlocking);

  EXPECT_EQ(3u, event_queue().size());

  // Task runner should have a task queued to dispatch non-rAF-aligned events.
  EXPECT_TRUE(main_task_runner_->HasPendingTask());

  // A main frame should be needed to dispatch the rAF-aligned event.
  EXPECT_TRUE(needs_main_frame_);

  // Run pending tasks without invoking a rAF.
  RunPendingTasksWithoutRaf();

  // The client should be notified that non-rAF-aligned events are dispatched.
  // No notification for rAF-aligned events, yet.
  EXPECT_TRUE(non_raf_aligned_events_dispatched_);
  EXPECT_FALSE(raf_aligned_events_dispatched_);

  // No task should be pending in the task runner.
  EXPECT_FALSE(main_task_runner_->HasPendingTask());

  // A main frame is still needed.
  EXPECT_TRUE(needs_main_frame_);

  // The two non-rAF-alinged events should be handled out of the queue.
  EXPECT_EQ(1u, event_queue().size());
  EXPECT_EQ(2u, handled_tasks_.size());
  EXPECT_EQ(key_down.GetType(),
            handled_tasks_.at(0)->taskAsEvent()->Event().GetType());
  EXPECT_EQ(key_up.GetType(),
            handled_tasks_.at(1)->taskAsEvent()->Event().GetType());

  // Run pending tasks with a simulated rAF.
  RunPendingTasksWithSimulatedRaf();

  // Now, clients should be notified of rAF-aligned events dispatch.
  EXPECT_TRUE(raf_aligned_events_dispatched_);

  // No main frame should be needed anymore..
  EXPECT_FALSE(needs_main_frame_);

  // The rAF-alinged event should be handled out of the queue now.
  EXPECT_EQ(0u, event_queue().size());
  EXPECT_EQ(3u, handled_tasks_.size());
  EXPECT_EQ(mouse_move.GetType(),
            handled_tasks_.at(2)->taskAsEvent()->Event().GetType());
}

}  // namespace blink
