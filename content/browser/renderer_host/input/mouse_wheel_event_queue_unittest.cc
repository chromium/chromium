// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/base_event_utils.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

const float kWheelScrollX = 10;
const float kWheelScrollY = 12;
const float kWheelScrollGlobalX = 50;
const float kWheelScrollGlobalY = 72;

#define EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event)                    \
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin, event->GetType()); \
  EXPECT_EQ(kWheelScrollX, event->PositionInWidget().x);           \
  EXPECT_EQ(kWheelScrollY, event->PositionInWidget().y);           \
  EXPECT_EQ(kWheelScrollGlobalX, event->PositionInScreen().x);     \
  EXPECT_EQ(kWheelScrollGlobalY, event->PositionInScreen().y);     \
  EXPECT_EQ(scroll_units, event->data.scroll_begin.delta_hint_units);

#define EXPECT_GESTURE_SCROLL_BEGIN(event)                         \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);                         \
  EXPECT_FALSE(event->data.scroll_begin.synthetic);                \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kUnknownMomentum, \
            event->data.scroll_begin.inertial_phase);

#define EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(event)          \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);                     \
  EXPECT_FALSE(event->data.scroll_begin.synthetic);            \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kNonMomentum, \
            event->data.scroll_begin.inertial_phase);

#define EXPECT_SYNTHETIC_GESTURE_SCROLL_BEGIN(event)           \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);                     \
  EXPECT_TRUE(event->data.scroll_begin.synthetic);             \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kNonMomentum, \
            event->data.scroll_begin.inertial_phase);

#define EXPECT_INERTIAL_GESTURE_SCROLL_BEGIN(event)         \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);                  \
  EXPECT_FALSE(event->data.scroll_begin.synthetic);         \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum, \
            event->data.scroll_begin.inertial_phase);

#define EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_BEGIN(event) \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);                    \
  EXPECT_TRUE(event->data.scroll_begin.synthetic);            \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum,   \
            event->data.scroll_begin.inertial_phase);

#define EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event)                    \
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, event->GetType()); \
  EXPECT_EQ(scroll_units, event->data.scroll_update.delta_units);   \
  EXPECT_EQ(kWheelScrollX, event->PositionInWidget().x);            \
  EXPECT_EQ(kWheelScrollY, event->PositionInWidget().y);            \
  EXPECT_EQ(kWheelScrollGlobalX, event->PositionInScreen().x);      \
  EXPECT_EQ(kWheelScrollGlobalY, event->PositionInScreen().y);

#define EXPECT_GESTURE_SCROLL_UPDATE(event)                        \
  EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event);                        \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kUnknownMomentum, \
            event->data.scroll_update.inertial_phase);

#define EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(event)         \
  EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event);                    \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kNonMomentum, \
            event->data.scroll_update.inertial_phase);

#define EXPECT_INERTIAL_GESTURE_SCROLL_UPDATE(event)        \
  EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event);                 \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum, \
            event->data.scroll_update.inertial_phase);

#define EXPECT_GESTURE_SCROLL_END_IMPL(event)                    \
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, event->GetType()); \
  EXPECT_EQ(scroll_units, event->data.scroll_end.delta_units);   \
  EXPECT_EQ(kWheelScrollX, event->PositionInWidget().x);         \
  EXPECT_EQ(kWheelScrollY, event->PositionInWidget().y);         \
  EXPECT_EQ(kWheelScrollGlobalX, event->PositionInScreen().x);   \
  EXPECT_EQ(kWheelScrollGlobalY, event->PositionInScreen().y);

#define EXPECT_GESTURE_SCROLL_END(event)                           \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);                           \
  EXPECT_FALSE(event->data.scroll_end.synthetic);                  \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kUnknownMomentum, \
            event->data.scroll_end.inertial_phase);

#define EXPECT_GESTURE_SCROLL_END_WITH_PHASE(event)            \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);                       \
  EXPECT_FALSE(event->data.scroll_end.synthetic);              \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kNonMomentum, \
            event->data.scroll_end.inertial_phase);

#define EXPECT_SYNTHETIC_GESTURE_SCROLL_END(event)             \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);                       \
  EXPECT_TRUE(event->data.scroll_end.synthetic);               \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kNonMomentum, \
            event->data.scroll_end.inertial_phase);

#define EXPECT_INERTIAL_GESTURE_SCROLL_END(event)           \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);                    \
  EXPECT_FALSE(event->data.scroll_end.synthetic);           \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum, \
            event->data.scroll_end.inertial_phase);

#if defined(CHROME_OS)
#define EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_END(event) \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);                    \
  EXPECT_TRUE(event->data.scroll_end.synthetic);            \
  EXPECT_EQ(WebGestureEvent::InertialPhaseState::kMomentum, \
            event->data.scroll_end.inertial_phase);         \
  EXPECT_TRUE(event->data.scroll_end.generated_by_fling_controller);
#endif

#define EXPECT_MOUSE_WHEEL(event) \
  EXPECT_EQ(WebInputEvent::kMouseWheel, event->GetType());

}  // namespace

class MouseWheelEventQueueTest : public testing::Test,
                                 public MouseWheelEventQueueClient {
 public:
  MouseWheelEventQueueTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        acked_event_count_(0),
        last_acked_event_state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {
    queue_.reset(new MouseWheelEventQueue(this));
  }

  ~MouseWheelEventQueueTest() override {}

  // MouseWheelEventQueueClient
  void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& event) override {
    WebMouseWheelEvent* cloned_event = new WebMouseWheelEvent();
    std::unique_ptr<WebInputEvent> cloned_event_holder(cloned_event);
    *cloned_event = event.event;
    sent_events_.push_back(std::move(cloned_event_holder));
  }

  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& event,
      const ui::LatencyInfo& latency_info) override {
    WebGestureEvent* cloned_event = new WebGestureEvent();
    std::unique_ptr<WebInputEvent> cloned_event_holder(cloned_event);
    *cloned_event = event;
    if (event.GetType() == WebInputEvent::kGestureScrollBegin) {
      is_wheel_scroll_in_progress_ = true;
    } else if (event.GetType() == WebInputEvent::kGestureScrollEnd) {
      is_wheel_scroll_in_progress_ = false;
    }
    sent_events_.push_back(std::move(cloned_event_holder));
  }

  void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                            InputEventAckSource ack_source,
                            InputEventAckState ack_result) override {
    ++acked_event_count_;
    last_acked_event_ = event.event;
    last_acked_event_state_ = ack_result;
  }

  bool IsWheelScrollInProgress() override {
    return is_wheel_scroll_in_progress_;
  }

  bool IsAutoscrollInProgress() override { return false; }

 protected:
  size_t queued_event_count() const { return queue_->queued_size(); }

  bool event_in_flight() const { return queue_->event_in_flight(); }

  std::vector<std::unique_ptr<WebInputEvent>>& all_sent_events() {
    return sent_events_;
  }

  const std::unique_ptr<WebInputEvent>& sent_input_event(size_t index) {
    return sent_events_[index];
  }
  const WebGestureEvent* sent_gesture_event(size_t index) {
    return static_cast<WebGestureEvent*>(sent_events_[index].get());
  }

  const WebMouseWheelEvent& acked_event() const { return last_acked_event_; }

  size_t GetAndResetSentEventCount() {
    size_t count = sent_events_.size();
    sent_events_.clear();
    return count;
  }

  size_t GetAndResetAckedEventCount() {
    size_t count = acked_event_count_;
    acked_event_count_ = 0;
    return count;
  }

  void SendMouseWheelEventAck(InputEventAckState ack_result) {
    const MouseWheelEventWithLatencyInfo mouse_event_with_latency_info(
        queue_->get_wheel_event_awaiting_ack_for_testing(), ui::LatencyInfo());
    queue_->ProcessMouseWheelAck(InputEventAckSource::COMPOSITOR_THREAD,
                                 ack_result, mouse_event_with_latency_info);
  }
  void SendMouseWheel(float x,
                      float y,
                      float global_x,
                      float global_y,
                      float dX,
                      float dY,
                      int modifiers,
                      bool high_precision,
                      blink::WebMouseWheelEvent::Phase phase,
                      blink::WebMouseWheelEvent::Phase momentum_phase,
                      WebInputEvent::RailsMode rails_mode,
                      bool has_synthetic_phase = false) {
    WebMouseWheelEvent event = SyntheticWebMouseWheelEventBuilder::Build(
        x, y, global_x, global_y, dX, dY, modifiers,
        high_precision
            ? ui::input_types::ScrollGranularity::kScrollByPrecisePixel
            : ui::input_types::ScrollGranularity::kScrollByPixel);
    event.phase = phase;
    event.momentum_phase = momentum_phase;
    event.rails_mode = rails_mode;
    event.has_synthetic_phase = has_synthetic_phase;
    queue_->QueueEvent(MouseWheelEventWithLatencyInfo(event));
  }
  void SendMouseWheel(float x,
                      float y,
                      float global_x,
                      float global_y,
                      float dX,
                      float dY,
                      int modifiers,
                      bool high_precision,
                      blink::WebMouseWheelEvent::Phase phase,
                      blink::WebMouseWheelEvent::Phase momentum_phase,
                      bool has_synthetic_phase = false) {
    SendMouseWheel(x, y, global_x, global_y, dX, dY, modifiers, high_precision,
                   phase, momentum_phase, WebInputEvent::kRailsModeFree,
                   has_synthetic_phase);
  }

  void SendGestureEvent(WebInputEvent::Type type) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          ui::EventTimeForNow(),
                          blink::WebGestureDevice::kTouchscreen);
    queue_->OnGestureScrollEvent(
        GestureEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated(),
        delay);
    base::RunLoop().Run();
  }

  void GestureSendingTest(bool high_precision) {
    const ui::input_types::ScrollGranularity scroll_units =
        high_precision
            ? ui::input_types::ScrollGranularity::kScrollByPrecisePixel
            : ui::input_types::ScrollGranularity::kScrollByPixel;
    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 1, 1, 0, high_precision,
                   WebMouseWheelEvent::kPhaseBegan,
                   WebMouseWheelEvent::kPhaseNone);
    EXPECT_EQ(0U, queued_event_count());
    EXPECT_TRUE(event_in_flight());
    EXPECT_EQ(1U, GetAndResetSentEventCount());

    // The second mouse wheel should not be sent since one is already in
    // queue.
    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 5, 5, 0, high_precision,
                   WebMouseWheelEvent::kPhaseChanged,
                   WebMouseWheelEvent::kPhaseNone);
    EXPECT_EQ(1U, queued_event_count());
    EXPECT_TRUE(event_in_flight());
    EXPECT_EQ(0U, GetAndResetSentEventCount());

    // Receive an ACK for the mouse wheel event and release the next
    // mouse wheel event.
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(0U, queued_event_count());
    EXPECT_TRUE(event_in_flight());
    EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
    EXPECT_EQ(1U, GetAndResetAckedEventCount());
    EXPECT_EQ(3U, all_sent_events().size());
    EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
    EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
    EXPECT_MOUSE_WHEEL(sent_input_event(2));
    EXPECT_EQ(3U, GetAndResetSentEventCount());
  }

  void PhaseGestureSendingTest(bool high_precision) {
    const ui::input_types::ScrollGranularity scroll_units =
        high_precision
            ? ui::input_types::ScrollGranularity::kScrollByPrecisePixel
            : ui::input_types::ScrollGranularity::kScrollByPixel;

    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 1, 1, 0, high_precision,
                   WebMouseWheelEvent::kPhaseBegan,
                   WebMouseWheelEvent::kPhaseNone);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(2U, all_sent_events().size());
    EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
    EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
    EXPECT_EQ(2U, GetAndResetSentEventCount());

    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 5, 5, 0, high_precision,
                   WebMouseWheelEvent::kPhaseChanged,
                   WebMouseWheelEvent::kPhaseNone);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(1U, all_sent_events().size());
    EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(0));
    EXPECT_EQ(1U, GetAndResetSentEventCount());

    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 5, 5, 0, high_precision,
                   WebMouseWheelEvent::kPhaseNone,
                   WebMouseWheelEvent::kPhaseBegan);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    // A fling has started, no ScrollEnd/ScrollBegin is sent.
    EXPECT_EQ(1U, all_sent_events().size());
    EXPECT_INERTIAL_GESTURE_SCROLL_UPDATE(sent_gesture_event(0));
    EXPECT_EQ(1U, GetAndResetSentEventCount());

    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 5, 5, 0, high_precision,
                   WebMouseWheelEvent::kPhaseNone,
                   WebMouseWheelEvent::kPhaseChanged);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(1U, all_sent_events().size());
    EXPECT_INERTIAL_GESTURE_SCROLL_UPDATE(sent_gesture_event(0));
    EXPECT_EQ(1U, GetAndResetSentEventCount());

    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 0, 0, 0, high_precision,
                   WebMouseWheelEvent::kPhaseNone,
                   WebMouseWheelEvent::kPhaseEnded);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    // MomentumPhase is ended, the scroll is done, and GSE is sent
    // immediately.
    EXPECT_EQ(1U, all_sent_events().size());
    EXPECT_INERTIAL_GESTURE_SCROLL_END(sent_gesture_event(0));
    EXPECT_EQ(1U, GetAndResetSentEventCount());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<MouseWheelEventQueue> queue_;
  std::vector<std::unique_ptr<WebInputEvent>> sent_events_;
  size_t acked_event_count_;
  InputEventAckState last_acked_event_state_;
  WebMouseWheelEvent last_acked_event_;

 private:
  bool is_wheel_scroll_in_progress_ = false;
};

// Tests that mouse wheel events are queued properly.
TEST_F(MouseWheelEventQueueTest, Basic) {
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebMouseWheelEvent::kPhaseBegan,
                 WebMouseWheelEvent::kPhaseNone);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second mouse wheel should not be sent since one is already in queue.
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 5, 5, 0, false,
                 WebMouseWheelEvent::kPhaseChanged,
                 WebMouseWheelEvent::kPhaseNone);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the first mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());

  // Receive an ACK for the second mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
}

TEST_F(MouseWheelEventQueueTest, GestureSending) {
  GestureSendingTest(false);
}

TEST_F(MouseWheelEventQueueTest, GestureSendingPrecisePixels) {
  GestureSendingTest(true);
}

TEST_F(MouseWheelEventQueueTest, GestureSendingWithPhaseInformation) {
  PhaseGestureSendingTest(false);
}

TEST_F(MouseWheelEventQueueTest,
       GestureSendingWithPhaseInformationPrecisePixels) {
  PhaseGestureSendingTest(true);
}

// Tests that a Wheel event with synthetic momentumn_phase == PhaseEnded that is
// generated by the fling controller properly populates
// scroll_end.data.scroll_end.generated_by_fling_controller.
#if defined(CHROME_OS)
TEST_F(MouseWheelEventQueueTest, WheelEndWithMomentumPhaseEndedInformation) {
  const ui::input_types::ScrollGranularity scroll_units =
      ui::input_types::ScrollGranularity::kScrollByPrecisePixel;
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, true /* high_precision */,
                 WebMouseWheelEvent::kPhaseBegan,
                 WebMouseWheelEvent::kPhaseNone);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 0, 0, 0, true /* high_precision */,
                 WebMouseWheelEvent::kPhaseNone,
                 WebMouseWheelEvent::kPhaseEnded, true /*has_synthetic_phase*/);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(1U, all_sent_events().size());
  EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_END(sent_gesture_event(0));
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}
#endif  // defined(CHROME_OS)

TEST_F(MouseWheelEventQueueTest, GestureSendingInterrupted) {
  const ui::input_types::ScrollGranularity scroll_units =
      ui::input_types::ScrollGranularity::kScrollByPixel;
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebMouseWheelEvent::kPhaseBegan,
                 WebMouseWheelEvent::kPhaseNone);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  // When a touch based GSB arrives in the
  // middle of wheel scrolling sequence, a synthetic wheel event with zero
  // deltas and phase = |kPhaseEnded| will be sent.
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 0, 0, 0, false,
                 WebMouseWheelEvent::kPhaseEnded,
                 WebMouseWheelEvent::kPhaseNone);
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Ensure that a gesture scroll begin terminates the current scroll event.
  SendGestureEvent(WebInputEvent::kGestureScrollBegin);

  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_END_WITH_PHASE(sent_gesture_event(1));
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebMouseWheelEvent::kPhaseBegan,
                 WebMouseWheelEvent::kPhaseNone);

  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // New mouse wheel events won't cause gestures because a scroll
  // is already in progress by another device.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, all_sent_events().size());

  SendGestureEvent(WebInputEvent::kGestureScrollEnd);
  EXPECT_EQ(0U, all_sent_events().size());

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebMouseWheelEvent::kPhaseBegan,
                 WebMouseWheelEvent::kPhaseNone);

  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
  EXPECT_EQ(2U, GetAndResetSentEventCount());
}

TEST_F(MouseWheelEventQueueTest, GestureRailScrolling) {
  const ui::input_types::ScrollGranularity scroll_units =
      ui::input_types::ScrollGranularity::kScrollByPixel;
  SendMouseWheel(
      kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX, kWheelScrollGlobalY, 1,
      1, 0, false, WebMouseWheelEvent::kPhaseBegan,
      WebMouseWheelEvent::kPhaseNone, WebInputEvent::kRailsModeHorizontal);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
  EXPECT_EQ(1U, sent_gesture_event(1)->data.scroll_update.delta_x);
  EXPECT_EQ(0U, sent_gesture_event(1)->data.scroll_update.delta_y);
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  SendMouseWheel(
      kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX, kWheelScrollGlobalY, 1,
      1, 0, false, WebMouseWheelEvent::kPhaseChanged,
      WebMouseWheelEvent::kPhaseNone, WebInputEvent::kRailsModeVertical);

  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  size_t scroll_update_index = 0;
  EXPECT_EQ(1U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(0));

  EXPECT_EQ(
      0U, sent_gesture_event(scroll_update_index)->data.scroll_update.delta_x);
  EXPECT_EQ(
      1U, sent_gesture_event(scroll_update_index)->data.scroll_update.delta_y);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
}

TEST_F(MouseWheelEventQueueTest, WheelScrollLatching) {
  const ui::input_types::ScrollGranularity scroll_units =
      ui::input_types::ScrollGranularity::kScrollByPixel;
  SendMouseWheel(
      kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX, kWheelScrollGlobalY, 1,
      1, 0, false, WebMouseWheelEvent::kPhaseBegan,
      WebMouseWheelEvent::kPhaseNone, WebInputEvent::kRailsModeVertical);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
  EXPECT_EQ(0U, sent_gesture_event(1)->data.scroll_update.delta_x);
  EXPECT_EQ(1U, sent_gesture_event(1)->data.scroll_update.delta_y);
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  SendMouseWheel(
      kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX, kWheelScrollGlobalY, 1,
      1, 0, false, WebMouseWheelEvent::kPhaseChanged,
      WebMouseWheelEvent::kPhaseNone, WebInputEvent::kRailsModeVertical);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Scroll latching: no new scroll begin expected.
  EXPECT_EQ(1U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(0));
  EXPECT_EQ(0U, sent_gesture_event(0)->data.scroll_update.delta_x);
  EXPECT_EQ(1U, sent_gesture_event(0)->data.scroll_update.delta_y);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

TEST_F(MouseWheelEventQueueTest, WheelScrollingWasLatchedHistogramCheck) {
  base::HistogramTester histogram_tester;
  const char latching_histogram_name[] = "WheelScrolling.WasLatched";

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebMouseWheelEvent::kPhaseBegan,
                 WebMouseWheelEvent::kPhaseNone);
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  histogram_tester.ExpectBucketCount(latching_histogram_name, 0, 1);

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebMouseWheelEvent::kPhaseChanged,
                 WebMouseWheelEvent::kPhaseNone);
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    histogram_tester.ExpectBucketCount(latching_histogram_name, 0, 1);
    histogram_tester.ExpectBucketCount(latching_histogram_name, 1, 1);
}

#if defined(OS_MACOSX)
TEST_F(MouseWheelEventQueueTest, DoNotSwapXYForShiftScroll) {
  // Send an event with shift modifier, zero value for delta X, and no direction
  // for |rails_mode|. Do not swap the scroll direction.
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 0.0, 1.0, WebInputEvent::kShiftKey, false,
                 WebMouseWheelEvent::kPhaseBegan,
                 WebMouseWheelEvent::kPhaseNone, WebInputEvent::kRailsModeFree);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::kMouseWheel, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_EQ(0U, sent_gesture_event(1)->data.scroll_update.delta_x);
  EXPECT_EQ(1U, sent_gesture_event(1)->data.scroll_update.delta_y);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
}
#endif
}  // namespace content
