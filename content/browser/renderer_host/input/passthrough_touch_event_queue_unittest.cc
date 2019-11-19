// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/passthrough_touch_event_queue.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input/web_touch_event_traits.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/base_event_utils.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

const double kMinSecondsBetweenThrottledTouchmoves = 0.2;
const float kSlopLengthDips = 10;
const float kHalfSlopLengthDips = kSlopLengthDips / 2;

base::TimeDelta DefaultTouchTimeoutDelay() {
  return base::TimeDelta::FromMilliseconds(1);
}
}  // namespace

class PassthroughTouchEventQueueTest : public testing::Test,
                                       public PassthroughTouchEventQueueClient {
 public:
  PassthroughTouchEventQueueTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        acked_event_count_(0),
        last_acked_event_state_(INPUT_EVENT_ACK_STATE_UNKNOWN),
        slop_length_dips_(0) {}

  ~PassthroughTouchEventQueueTest() override {}

  // testing::Test
  void SetUp() override {
    ResetQueueWithConfig(PassthroughTouchEventQueue::Config());
    sent_events_ids_.clear();
  }

  void TearDown() override { queue_.reset(); }

  // PassthroughTouchEventQueueClient
  void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) override {
    sent_events_.push_back(event.event);
    sent_events_ids_.push_back(event.event.unique_touch_event_id);
    if (sync_ack_result_) {
      auto sync_ack_result = std::move(sync_ack_result_);
      SendTouchEventAckWithID(*sync_ack_result,
                              event.event.unique_touch_event_id);
    }
  }

  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override {
    ++acked_event_count_;
    if (followup_touch_event_) {
      std::unique_ptr<WebTouchEvent> followup_touch_event =
          std::move(followup_touch_event_);
      SendTouchEvent(*followup_touch_event);
    }
    if (followup_gesture_event_) {
      std::unique_ptr<WebGestureEvent> followup_gesture_event =
          std::move(followup_gesture_event_);
      queue_->OnGestureScrollEvent(GestureEventWithLatencyInfo(
          *followup_gesture_event, ui::LatencyInfo()));
    }
    last_acked_event_ = event.event;
    last_acked_event_state_ = ack_result;
  }

  void OnFilteringTouchEvent(const blink::WebTouchEvent& touch_event) override {
  }

  void FlushDeferredGestureQueue() override {}

 protected:
  void SetUpForTouchMoveSlopTesting(double slop_length_dips) {
    slop_length_dips_ = slop_length_dips;
  }

  void SetUpForTimeoutTesting(base::TimeDelta desktop_timeout_delay,
                              base::TimeDelta mobile_timeout_delay) {
    PassthroughTouchEventQueue::Config config;
    config.desktop_touch_ack_timeout_delay = desktop_timeout_delay;
    config.mobile_touch_ack_timeout_delay = mobile_timeout_delay;
    config.touch_ack_timeout_supported = true;
    ResetQueueWithConfig(config);
  }

  void SetUpForTimeoutTesting() {
    SetUpForTimeoutTesting(DefaultTouchTimeoutDelay(),
                           DefaultTouchTimeoutDelay());
  }

  void SetUpForSkipFilterTesting(const std::string& events_to_always_forward) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kSkipTouchEventFilter,
        {{features::kSkipTouchEventFilterTypeParamName,
          events_to_always_forward}});
    ResetQueueWithConfig(PassthroughTouchEventQueue::Config());
  }

  void SendTouchEvent(WebTouchEvent event) {
    if (slop_length_dips_) {
      event.moved_beyond_slop_region = false;
      if (WebTouchEventTraits::IsTouchSequenceStart(event))
        anchor_ = event.touches[0].PositionInWidget();
      if (event.GetType() == WebInputEvent::kTouchMove) {
        gfx::Vector2dF delta = anchor_ - event.touches[0].PositionInWidget();
        if (delta.LengthSquared() > slop_length_dips_ * slop_length_dips_)
          event.moved_beyond_slop_region = true;
      }
    } else {
      event.moved_beyond_slop_region =
          event.GetType() == WebInputEvent::kTouchMove;
    }
    queue_->QueueEvent(TouchEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendGestureEvent(WebInputEvent::Type type) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          ui::EventTimeForNow());
    queue_->OnGestureScrollEvent(
        GestureEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendTouchEventAck(InputEventAckState ack_result) {
    DCHECK(!sent_events_ids_.empty());
    queue_->ProcessTouchAck(InputEventAckSource::COMPOSITOR_THREAD, ack_result,
                            ui::LatencyInfo(), sent_events_ids_.front(), true);
    sent_events_ids_.pop_front();
  }

  void SendTouchEventAckLast(InputEventAckState ack_result) {
    DCHECK(!sent_events_ids_.empty());
    queue_->ProcessTouchAck(InputEventAckSource::COMPOSITOR_THREAD, ack_result,
                            ui::LatencyInfo(), sent_events_ids_.back(), true);
    sent_events_ids_.pop_back();
  }

  void SendTouchEventAckWithID(InputEventAckState ack_result,
                               int unique_event_id) {
    queue_->ProcessTouchAck(InputEventAckSource::COMPOSITOR_THREAD, ack_result,
                            ui::LatencyInfo(), unique_event_id, true);
    base::Erase(sent_events_ids_, unique_event_id);
  }

  void SendGestureEventAck(WebInputEvent::Type type,
                           InputEventAckState ack_result) {
    GestureEventWithLatencyInfo event(type, blink::WebInputEvent::kNoModifiers,
                                      ui::EventTimeForNow(), ui::LatencyInfo());
    queue_->OnGestureEventAck(event, ack_result);
  }

  void SetFollowupEvent(const WebTouchEvent& event) {
    followup_touch_event_.reset(new WebTouchEvent(event));
  }

  void SetFollowupEvent(const WebGestureEvent& event) {
    followup_gesture_event_.reset(new WebGestureEvent(event));
  }

  void SetSyncAckResult(InputEventAckState sync_ack_result) {
    sync_ack_result_.reset(new InputEventAckState(sync_ack_result));
  }

  void PressTouchPoint(float x, float y) {
    touch_event_.PressPoint(x, y, radius_x_, radius_y_);
    SendTouchEvent();
  }

  void MoveTouchPoint(int index, float x, float y) {
    touch_event_.MovePoint(index, x, y, radius_x_, radius_y_);
    SendTouchEvent();
  }

  void MoveTouchPoints(int index0,
                       float x0,
                       float y0,
                       int index1,
                       float x1,
                       float y1) {
    touch_event_.MovePoint(index0, x0, y0);
    touch_event_.MovePoint(index1, x1, y1);
    SendTouchEvent();
  }

  void ChangeTouchPointRadius(int index, float radius_x, float radius_y) {
    CHECK_GE(index, 0);
    CHECK_LT(index, touch_event_.kTouchesLengthCap);
    WebTouchPoint& point = touch_event_.touches[index];
    point.radius_x = radius_x;
    point.radius_y = radius_y;
    touch_event_.touches[index].state = WebTouchPoint::kStateMoved;
    touch_event_.moved_beyond_slop_region = true;
    WebTouchEventTraits::ResetType(WebInputEvent::kTouchMove,
                                   touch_event_.TimeStamp(), &touch_event_);
    SendTouchEvent();
  }

  void ChangeTouchPointRotationAngle(int index, float rotation_angle) {
    CHECK_GE(index, 0);
    CHECK_LT(index, touch_event_.kTouchesLengthCap);
    WebTouchPoint& point = touch_event_.touches[index];
    point.rotation_angle = rotation_angle;
    touch_event_.touches[index].state = WebTouchPoint::kStateMoved;
    touch_event_.moved_beyond_slop_region = true;
    WebTouchEventTraits::ResetType(WebInputEvent::kTouchMove,
                                   touch_event_.TimeStamp(), &touch_event_);
    SendTouchEvent();
  }

  void ChangeTouchPointForce(int index, float force) {
    CHECK_GE(index, 0);
    CHECK_LT(index, touch_event_.kTouchesLengthCap);
    WebTouchPoint& point = touch_event_.touches[index];
    point.force = force;
    touch_event_.touches[index].state = WebTouchPoint::kStateMoved;
    touch_event_.moved_beyond_slop_region = true;
    WebTouchEventTraits::ResetType(WebInputEvent::kTouchMove,
                                   touch_event_.TimeStamp(), &touch_event_);
    SendTouchEvent();
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
    SendTouchEvent();
  }

  void CancelTouchPoint(int index) {
    touch_event_.CancelPoint(index);
    SendTouchEvent();
  }

  void PrependTouchScrollNotification() {
    queue_->PrependTouchScrollNotification();
  }

  void AdvanceTouchTime(double seconds) {
    touch_event_.SetTimeStamp(touch_event_.TimeStamp() +
                              base::TimeDelta::FromSecondsD(seconds));
  }

  size_t GetAndResetAckedEventCount() {
    size_t count = acked_event_count_;
    acked_event_count_ = 0;
    return count;
  }

  size_t GetAndResetSentEventCount() {
    size_t count = sent_events_.size();
    sent_events_.clear();
    return count;
  }

  bool IsPendingAckTouchStart() const {
    return queue_->IsPendingAckTouchStart();
  }

  void OnHasTouchEventHandlers(bool has_handlers) {
    queue_->OnHasTouchEventHandlers(has_handlers);
  }

  void SetAckTimeoutDisabled() { queue_->SetAckTimeoutEnabled(false); }

  void SetIsMobileOptimizedSite(bool is_mobile_optimized) {
    queue_->SetIsMobileOptimizedSite(is_mobile_optimized);
  }

  bool IsTimeoutRunning() const { return queue_->IsTimeoutRunningForTesting(); }

  size_t queued_event_count() const { return queue_->SizeForTesting(); }

  const WebTouchEvent& acked_event() const { return last_acked_event_; }

  const WebTouchEvent& sent_event() const {
    DCHECK(!sent_events_.empty());
    return sent_events_.back();
  }

  const std::vector<WebTouchEvent>& all_sent_events() const {
    return sent_events_;
  }

  InputEventAckState acked_event_state() const {
    return last_acked_event_state_;
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), delay);
    run_loop.Run();
  }

  PassthroughTouchEventQueue::PreFilterResult FilterBeforeForwarding(
      const blink::WebTouchEvent& event) {
    return queue_->FilterBeforeForwarding(event);
  }

  int GetUniqueTouchEventID() { return sent_events_ids_.back(); }

  const float radius_x_ = 20.0f;
  const float radius_y_ = 20.0f;

 private:
  void SendTouchEvent() {
    SendTouchEvent(touch_event_);
    touch_event_.ResetPoints();
  }

  void ResetQueueWithConfig(const PassthroughTouchEventQueue::Config& config) {
    queue_.reset(new PassthroughTouchEventQueue(this, config));
    queue_->OnHasTouchEventHandlers(true);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<PassthroughTouchEventQueue> queue_;
  size_t acked_event_count_;
  WebTouchEvent last_acked_event_;
  std::vector<WebTouchEvent> sent_events_;
  InputEventAckState last_acked_event_state_;
  SyntheticWebTouchEvent touch_event_;
  std::unique_ptr<WebTouchEvent> followup_touch_event_;
  std::unique_ptr<WebGestureEvent> followup_gesture_event_;
  std::unique_ptr<InputEventAckState> sync_ack_result_;
  double slop_length_dips_;
  gfx::PointF anchor_;
  base::circular_deque<int> sent_events_ids_;
};

// Tests that touch-events are queued properly.
TEST_F(PassthroughTouchEventQueueTest, Basic) {
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second touch should be sent right away even though
  // we haven't received an ack for the touch start.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the first touch-event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::kTouchStart, acked_event().GetType());
  EXPECT_EQ(WebInputEvent::kBlocking, acked_event().dispatch_type);

  // Receive an ACK for the second touch-event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::kTouchMove, acked_event().GetType());
  EXPECT_EQ(WebInputEvent::kBlocking, acked_event().dispatch_type);
}

// Tests that touch-events with multiple points are queued properly.
TEST_F(PassthroughTouchEventQueueTest, BasicMultiTouch) {
  const size_t kPointerCount = 10;
  for (float i = 0; i < kPointerCount; ++i)
    PressTouchPoint(i, i);

  EXPECT_EQ(kPointerCount, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(kPointerCount, queued_event_count());

  for (int i = 0; i < static_cast<int>(kPointerCount); ++i)
    MoveTouchPoint(i, 1.f + i, 2.f + i);

  EXPECT_EQ(kPointerCount, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  // No coalescing happens in the queue since they are all sent immediately.
  EXPECT_EQ(2 * kPointerCount, queued_event_count());

  for (int i = 0; i < static_cast<int>(kPointerCount); ++i)
    ReleaseTouchPoint(kPointerCount - 1 - i);

  EXPECT_EQ(kPointerCount, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(kPointerCount * 3, queued_event_count());

  // Ack all presses.
  for (size_t i = 0; i < kPointerCount; ++i)
    SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(kPointerCount, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Ack the touch moves.
  for (size_t i = 0; i < kPointerCount; ++i)
    SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(kPointerCount, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Ack all releases.
  for (size_t i = 0; i < kPointerCount; ++i)
    SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(kPointerCount, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
}

// Tests that the touch-queue continues delivering events for an active touch
// sequence after all handlers are removed.
TEST_F(PassthroughTouchEventQueueTest,
       TouchesForwardedIfHandlerRemovedDuringSequence) {
  OnHasTouchEventHandlers(true);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Signal that all touch handlers have been removed.
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Process the ack for the sent touch, ensuring that it is honored (despite
  // the touch handler having been removed).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());

  // Try forwarding a new pointer. It should be forwarded as usual.
  PressTouchPoint(2, 2);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Further events for any pointer should be forwarded, even for pointers that
  // reported no consumer.
  MoveTouchPoint(1, 3, 3);
  ReleaseTouchPoint(1);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Events for the first pointer, that had a handler, should be forwarded.
  MoveTouchPoint(0, 4, 4);
  ReleaseTouchPoint(0);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());
}

// Tests that addition of a touch handler during a touch sequence will continue
// forwarding events.
TEST_F(PassthroughTouchEventQueueTest,
       ActiveSequenceStillForwardedWhenHandlersAdded) {
  OnHasTouchEventHandlers(false);

  // Send a touch-press event while there is no handler.
  PressTouchPoint(1, 1);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  OnHasTouchEventHandlers(true);

  // The remaining touch sequence should be forwarded.
  MoveTouchPoint(0, 5, 5);
  ReleaseTouchPoint(0);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(3U, queued_event_count());

  // A new touch sequence should continue forwarding.
  PressTouchPoint(1, 1);
  EXPECT_EQ(4U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

// Tests that removal of a touch handler during a touch sequence will not
// prevent the remaining sequence from being forwarded, even if another touch
// handler is registered during the same touch sequence.
TEST_F(PassthroughTouchEventQueueTest,
       ActiveSequenceDroppedWhenHandlersRemoved) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Queue a touch-move event.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Unregister all touch handlers.
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Repeated registration/unregstration of handlers should have no effect.
  OnHasTouchEventHandlers(true);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Clear the queue.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // TouchMove events should be dropped while there is no touch handler.
  MoveTouchPoint(0, 10, 10);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Simulate touch handler registration in the middle of a touch sequence.
  OnHasTouchEventHandlers(true);

  // The touch end for the interrupted sequence should be sent.
  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // A new touch sequence should be forwarded properly.
  PressTouchPoint(1, 1);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

// Tests that removal/addition of a touch handler without any intervening
// touch activity has no affect on touch forwarding.
TEST_F(PassthroughTouchEventQueueTest,
       ActiveSequenceUnaffectedByRepeatedHandlerRemovalAndAddition) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Simulate the case where the touchstart handler removes itself, and adds a
  // touchmove handler.
  OnHasTouchEventHandlers(false);
  OnHasTouchEventHandlers(true);

  // Queue a touch-move event, should be sent right away.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The ack should trigger forwarding of the touchmove, as if no touch
  // handler registration changes have occurred.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
}

// Tests that the touch-event queue is robust to redundant acks.
TEST_F(PassthroughTouchEventQueueTest, SpuriousAcksIgnored) {
  // Trigger a spurious ack.
  SendTouchEventAckWithID(INPUT_EVENT_ACK_STATE_CONSUMED, 0);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Send and ack a touch press.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Trigger a spurious ack.
  SendTouchEventAckWithID(INPUT_EVENT_ACK_STATE_CONSUMED, 3);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that touch-move events are not sent to the renderer even if the
// preceding touch-press event did not have a consumer.
TEST_F(PassthroughTouchEventQueueTest, NoConsumer) {
  // The first touch-press should reach the renderer.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second touch should be sent too.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the first touch-event and the first touch-move
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Send a release event. This should reach the renderer.
  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::kTouchMove, acked_event().GetType());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Send a press-event, followed by a move should be sent.
  PressTouchPoint(10, 10);
  MoveTouchPoint(0, 5, 5);

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 6, 5);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(3U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

TEST_F(PassthroughTouchEventQueueTest, AckTouchEventInReverse) {
  PressTouchPoint(1, 1);
  MoveTouchPoint(0, 5, 5);
  MoveTouchPoint(0, 15, 15);
  ReleaseTouchPoint(0);

  EXPECT_EQ(4U, GetAndResetSentEventCount());
  EXPECT_EQ(4U, queued_event_count());

  SendTouchEventAckLast(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(4U, queued_event_count());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kTouchStart, acked_event().GetType());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(3U, queued_event_count());

  SendTouchEventAckLast(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(3U, queued_event_count());

  SendTouchEventAckLast(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kTouchEnd, acked_event().GetType());
  EXPECT_EQ(3U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());
}

// Tests that touch-event's enqueued via a touch ack are properly handled.
TEST_F(PassthroughTouchEventQueueTest, AckWithFollowupEvents) {
  // Queue a touch down.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Create a touch event that will be queued synchronously by a touch ack.
  // Note, this will be triggered by all subsequent touch acks.
  WebTouchEvent followup_event(WebInputEvent::kTouchMove,
                               WebInputEvent::kNoModifiers,
                               ui::EventTimeForNow());
  followup_event.touches_length = 1;
  followup_event.touches[0].id = 0;
  followup_event.touches[0].state = WebTouchPoint::kStateMoved;
  SetFollowupEvent(followup_event);

  // Receive an ACK for the press. This should cause the followup touch-move to
  // be sent to the renderer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());
  EXPECT_EQ(WebInputEvent::kTouchStart, acked_event().GetType());

  // Queue another event.
  MoveTouchPoint(0, 2, 2);
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the touch-move followup event. This should cause the
  // subsequent touch move event be sent to the renderer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that touch-events can be synchronously ack'ed.
TEST_F(PassthroughTouchEventQueueTest, SynchronousAcks) {
  // TouchStart
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  PressTouchPoint(1, 1);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  MoveTouchPoint(0, 2, 2);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchEnd
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  ReleaseTouchPoint(0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchCancel (first inserting a TouchStart so the TouchCancel will be sent)
  PressTouchPoint(1, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  CancelTouchPoint(0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that touch-events acks are in order even with synchronous acks.
TEST_F(PassthroughTouchEventQueueTest, SynchronousAcksInOrder) {
  // TouchStart
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // TouchMove
  MoveTouchPoint(0, 2, 3);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Ack the TouchMove
  SendTouchEventAckLast(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Create a touch event that will be queued synchronously by a touch ack.
  WebTouchEvent followup_event(WebInputEvent::kTouchMove,
                               WebInputEvent::kNoModifiers,
                               ui::EventTimeForNow());
  followup_event.touches_length = 1;
  followup_event.touches[0].id = 0;
  followup_event.unique_touch_event_id = 100;
  followup_event.touches[0].state = WebTouchPoint::kStateMoved;
  SetFollowupEvent(followup_event);
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Ack the touch start, should release the |follow_up| event (and its ack).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(3U, GetAndResetAckedEventCount());
  EXPECT_EQ(100U, acked_event().unique_touch_event_id);
}

// Tests that followup events triggered by an immediate ack from
// TouchEventQueue::QueueEvent() are properly handled.
TEST_F(PassthroughTouchEventQueueTest, ImmediateAckWithFollowupEvents) {
  // Create a touch event that will be queued synchronously by a touch ack.
  WebTouchEvent followup_event(WebInputEvent::kTouchStart,
                               WebInputEvent::kNoModifiers,
                               ui::EventTimeForNow());
  followup_event.touches_length = 1;
  followup_event.touches[0].id = 1;
  followup_event.touches[0].state = WebTouchPoint::kStatePressed;
  SetFollowupEvent(followup_event);

  // Now, enqueue a stationary touch that will not be forwarded.  This should be
  // immediately ack'ed with "NO_CONSUMER_EXISTS".  The followup event should
  // then be enqueued and immediately sent to the renderer.
  WebTouchEvent stationary_event(WebInputEvent::kTouchMove,
                                 WebInputEvent::kNoModifiers,
                                 ui::EventTimeForNow());
  stationary_event.touches_length = 1;
  stationary_event.touches[0].id = 1;
  stationary_event.touches[0].state = WebTouchPoint::kStateStationary;
  SendTouchEvent(stationary_event);

  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());
  EXPECT_EQ(WebInputEvent::kTouchMove, acked_event().GetType());
}

// Tests that basic TouchEvent forwarding suppression has been disabled.
TEST_F(PassthroughTouchEventQueueTest, NoTouchBasic) {
  // The old behaviour was to suppress events when there were no handlers.
  // Signal the no-handler case and test that events still get forwarded.
  OnHasTouchEventHandlers(false);
  PressTouchPoint(30, 5);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // TouchMove should not be sent to renderer.
  MoveTouchPoint(0, 65, 10);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // TouchEnd should be sent to renderer.
  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Signal handlers-present and make sure events are still getting forwarded.
  OnHasTouchEventHandlers(true);

  PressTouchPoint(80, 10);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 80, 20);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that IsTouchStartPendingAck works correctly.
TEST_F(PassthroughTouchEventQueueTest, PendingStart) {
  EXPECT_FALSE(IsPendingAckTouchStart());

  // Send the touchstart for one point (#1).
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Send a touchmove for that point (#2).
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Ack the touchstart (#1).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_FALSE(IsPendingAckTouchStart());

  // Send a touchstart for another point (#3).
  PressTouchPoint(10, 10);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Ack the touchmove (#2).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Send a touchstart for a third point (#4).
  PressTouchPoint(15, 15);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Ack the touchstart for the second point (#3).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Ack the touchstart for the third point (#4).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(IsPendingAckTouchStart());
}

// Tests that the touch timeout is started when sending certain touch types.
TEST_F(PassthroughTouchEventQueueTest, TouchTimeoutTypes) {
  SetUpForTimeoutTesting();

  // Sending a TouchStart will start the timeout.
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchMove should start the timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_TRUE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchEnd should not start the timeout.
  ReleaseTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchCancel should not start the timeout.
  PressTouchPoint(0, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_FALSE(IsTimeoutRunning());
  CancelTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that a delayed TouchEvent ack will trigger a TouchCancel timeout,
// disabling touch forwarding until the next TouchStart is received after
// the timeout events are ack'ed.
TEST_F(PassthroughTouchEventQueueTest, TouchTimeoutBasic) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  GetAndResetSentEventCount();
  GetAndResetAckedEventCount();
  PressTouchPoint(0, 1);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_TRUE(IsTimeoutRunning());

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, synthetically ack'ing the timed-out event.
  // TouchEvent forwarding is disabled until the ack is received for the
  // timed-out event and the future cancel event.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Ack'ing the original event should trigger a cancel event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(WebInputEvent::kTouchCancel, sent_event().GetType());
  EXPECT_NE(WebInputEvent::kBlocking, sent_event().dispatch_type);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Touch events should not be forwarded until we receive the cancel acks.
  MoveTouchPoint(0, 1, 1);
  ASSERT_EQ(0U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  ReleaseTouchPoint(0);
  ASSERT_EQ(0U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // The synthetic TouchCancel ack should not reach the client, but should
  // resume touch forwarding.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Subsequent events should be handled normally.
  PressTouchPoint(0, 1);
  EXPECT_EQ(WebInputEvent::kTouchStart, sent_event().GetType());
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that the timeout is never started if the renderer consumes
// a TouchEvent from the current touch sequence.
TEST_F(PassthroughTouchEventQueueTest,
       NoTouchTimeoutIfRendererIsConsumingGesture) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Mark the event as consumed. This should prevent the timeout from
  // being activated on subsequent TouchEvents in this gesture.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchMove should not start the timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // A secondary TouchStart should not start the timeout.
  PressTouchPoint(1, 0);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // A TouchEnd should not start the timeout.
  ReleaseTouchPoint(1);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // A TouchCancel should not start the timeout.
  CancelTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that the timeout is never started if the renderer consumes
// a TouchEvent from the current touch sequence.
TEST_F(PassthroughTouchEventQueueTest,
       NoTouchTimeoutIfDisabledAfterTouchStart) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Send the ack immediately. The timeout should not have fired.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now explicitly disable the timeout.
  SetAckTimeoutDisabled();
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchMove should not start or trigger the timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that the timeout is never started if the ack is synchronous.
TEST_F(PassthroughTouchEventQueueTest, NoTouchTimeoutIfAckIsSynchronous) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_FALSE(IsTimeoutRunning());
  PressTouchPoint(0, 1);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that the timeout does not fire if explicitly disabled while an event
// is in-flight.
TEST_F(PassthroughTouchEventQueueTest,
       NoTouchTimeoutIfDisabledWhileTimerIsActive) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Verify that disabling the timeout also turns off the timer.
  SetAckTimeoutDisabled();
  EXPECT_FALSE(IsTimeoutRunning());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that the timeout does not fire if the delay is zero.
TEST_F(PassthroughTouchEventQueueTest, NoTouchTimeoutIfTimeoutDelayIsZero) {
  SetUpForTimeoutTesting(base::TimeDelta(), base::TimeDelta());

  // As the delay is zero, timeout behavior should be disabled.
  PressTouchPoint(0, 1);
  EXPECT_FALSE(IsTimeoutRunning());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that timeout delays for mobile sites take effect when appropriate.
TEST_F(PassthroughTouchEventQueueTest, TouchTimeoutConfiguredForMobile) {
  base::TimeDelta desktop_delay = DefaultTouchTimeoutDelay();
  base::TimeDelta mobile_delay = base::TimeDelta();
  SetUpForTimeoutTesting(desktop_delay, mobile_delay);

  // The desktop delay is non-zero, allowing timeout behavior.
  SetIsMobileOptimizedSite(false);

  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  ReleaseTouchPoint(0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  ASSERT_FALSE(IsTimeoutRunning());

  // The mobile delay is zero, preventing timeout behavior.
  SetIsMobileOptimizedSite(true);

  PressTouchPoint(0, 1);
  EXPECT_FALSE(IsTimeoutRunning());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that a TouchCancel timeout plays nice when the timed out touch stream
// turns into a scroll gesture sequence.
TEST_F(PassthroughTouchEventQueueTest, TouchTimeoutWithFollowupGesture) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The cancelled sequence may turn into a scroll gesture.
  WebGestureEvent followup_scroll(WebInputEvent::kGestureScrollBegin,
                                  WebInputEvent::kNoModifiers,
                                  ui::EventTimeForNow());
  SetFollowupEvent(followup_scroll);

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, disabling touch forwarding until both acks
  // are received, acking the timed out event.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Ack the original event, triggering a TouchCancel.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Ack the cancel event. Normally, this would resume touch forwarding,
  // but we're still within a scroll gesture so it remains disabled.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Forward touch events for the current sequence.
  GetAndResetSentEventCount();
  GetAndResetAckedEventCount();
  MoveTouchPoint(0, 1, 1);
  ReleaseTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now end the scroll sequence.
  SendGestureEvent(blink::WebInputEvent::kGestureScrollEnd);
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that a TouchCancel timeout plays nice when the timed out touch stream
// turns into a scroll gesture sequence, but the original event acks are
// significantly delayed.
TEST_F(PassthroughTouchEventQueueTest,
       TouchTimeoutWithFollowupGestureAndDelayedAck) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The cancelled sequence may turn into a scroll gesture.
  WebGestureEvent followup_scroll(WebInputEvent::kGestureScrollBegin,
                                  WebInputEvent::kNoModifiers,
                                  ui::EventTimeForNow());
  SetFollowupEvent(followup_scroll);

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, disabling touch forwarding until both acks
  // are received and acking the timed out event.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Try to forward a touch event.
  GetAndResetSentEventCount();
  GetAndResetAckedEventCount();
  MoveTouchPoint(0, 1, 1);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now end the scroll sequence.  Events will not be forwarded until the two
  // outstanding touch acks are received.
  SendGestureEvent(blink::WebInputEvent::kGestureScrollEnd);
  MoveTouchPoint(0, 2, 2);
  ReleaseTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, GetAndResetAckedEventCount());

  // Ack the original event, triggering a cancel.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Ack the cancel event, resuming touch forwarding.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

// Tests that a delayed TouchEvent ack will not trigger a TouchCancel timeout if
// the timed-out event had no consumer.
TEST_F(PassthroughTouchEventQueueTest, NoCancelOnTouchTimeoutWithoutConsumer) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_TRUE(IsTimeoutRunning());

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, synthetically ack'ing the timed out event.
  // TouchEvent forwarding is disabled until the original ack is received.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Touch events should not be forwarded until we receive the original ack.
  MoveTouchPoint(0, 1, 1);
  ReleaseTouchPoint(0);
  ASSERT_EQ(0U, GetAndResetSentEventCount());
  ASSERT_EQ(2U, GetAndResetAckedEventCount());

  // Ack'ing the original event should not trigger a cancel event, as the
  // TouchStart had no consumer.  However, it should re-enable touch forwarding.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Subsequent events should be handled normally.
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that TouchMove's movedBeyondSlopRegion is set to false if within the
// boundary-inclusive slop region for an unconsumed TouchStart.
TEST_F(PassthroughTouchEventQueueTest, TouchMovedBeyondSlopRegionCheck) {
  SetUpForTouchMoveSlopTesting(kSlopLengthDips);

  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove's movedBeyondSlopRegion within the slop region is set to false.
  MoveTouchPoint(0, 0, kHalfSlopLengthDips);
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_FALSE(acked_event().moved_beyond_slop_region);

  MoveTouchPoint(0, kHalfSlopLengthDips, 0);
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_FALSE(acked_event().moved_beyond_slop_region);

  MoveTouchPoint(0, -kHalfSlopLengthDips, 0);
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_FALSE(acked_event().moved_beyond_slop_region);

  MoveTouchPoint(0, -kSlopLengthDips, 0);
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_FALSE(acked_event().moved_beyond_slop_region);

  MoveTouchPoint(0, 0, kSlopLengthDips);
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_FALSE(acked_event().moved_beyond_slop_region);

  // When a TouchMove exceeds the (Euclidean) distance, the TouchMove's
  // movedBeyondSlopRegion is set to true.
  const float kFortyFiveDegreeSlopLengthXY =
      kSlopLengthDips * std::sqrt(2.f) / 2;
  MoveTouchPoint(0, kFortyFiveDegreeSlopLengthXY + .2f,
                 kFortyFiveDegreeSlopLengthXY + .2f);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_TRUE(acked_event().moved_beyond_slop_region);
}

// Tests that even very small TouchMove's movedBeyondSlopRegion is set to true
// when the slop region's dimension is 0.
TEST_F(PassthroughTouchEventQueueTest,
       MovedBeyondSlopRegionAlwaysTrueIfDimensionZero) {
  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // Small TouchMove's movedBeyondSlopRegion is set to true.
  MoveTouchPoint(0, 0.001f, 0.001f);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_TRUE(acked_event().moved_beyond_slop_region);
}

// Tests that secondary touch points can be forwarded even if the primary touch
// point had no consumer.
TEST_F(PassthroughTouchEventQueueTest,
       SecondaryTouchForwardedAfterPrimaryHadNoConsumer) {
  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove events should not be forwarded, as the point had no consumer.
  MoveTouchPoint(0, 0, 15);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Simulate a secondary pointer press.
  PressTouchPoint(20, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove with a secondary pointer should not be suppressed.
  MoveTouchPoint(1, 25, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that secondary touch points can be forwarded after scrolling begins
// while first touch point has no consumer.
TEST_F(PassthroughTouchEventQueueTest,
       NoForwardingAfterScrollWithNoTouchConsumers) {
  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  WebGestureEvent followup_scroll(WebInputEvent::kGestureScrollBegin,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
  SetFollowupEvent(followup_scroll);
  MoveTouchPoint(0, 20, 5);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // The secondary pointer press should be forwarded.
  PressTouchPoint(20, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove with a secondary pointer should also be forwarded.
  MoveTouchPoint(1, 25, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

TEST_F(PassthroughTouchEventQueueTest, TouchAbsorptionWithConsumedFirstMove) {
  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 20, 5);
  SendGestureEvent(blink::WebInputEvent::kGestureScrollBegin);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  // Even if the first touchmove event was consumed, subsequent unconsumed
  // touchmove events should trigger scrolling.
  MoveTouchPoint(0, 60, 5);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 20, 5);
  WebGestureEvent followup_scroll(WebInputEvent::kGestureScrollUpdate,
                                  WebInputEvent::kNoModifiers,
                                  WebInputEvent::GetStaticTimeStampForTests());
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendGestureEventAck(WebInputEvent::kGestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Touch moves are sent right away.
  MoveTouchPoint(0, 60, 5);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

TEST_F(PassthroughTouchEventQueueTest, TouchStartCancelableDuringScroll) {
  // Queue a touchstart and touchmove that go unconsumed, transitioning to an
  // active scroll sequence.
  PressTouchPoint(0, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 20, 5);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  SendGestureEvent(blink::WebInputEvent::kGestureScrollBegin);
  SendGestureEvent(blink::WebInputEvent::kGestureScrollUpdate);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // Even though scrolling has begun, touchstart events should be cancelable,
  // allowing, for example, customized pinch processing.
  PressTouchPoint(10, 11);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // As the touch start was consumed, touchmoves should no longer be throttled.
  MoveTouchPoint(1, 11, 11);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // With throttling disabled, touchend and touchmove events should also be
  // cancelable.
  MoveTouchPoint(1, 12, 12);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ReleaseTouchPoint(1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // If subsequent touchmoves aren't consumed, the generated scroll events
  // will restore async touch dispatch.
  MoveTouchPoint(0, 25, 5);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendGestureEvent(blink::WebInputEvent::kGestureScrollUpdate);
  EXPECT_EQ(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 30, 5);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_NE(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // The touchend will be uncancelable during an active scroll sequence.
  ReleaseTouchPoint(0);
  EXPECT_NE(WebInputEvent::kBlocking, sent_event().dispatch_type);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
}

TEST_F(PassthroughTouchEventQueueTest, UnseenTouchPointerIdsNotForwarded) {
  SyntheticWebTouchEvent event;
  event.PressPoint(0, 0);
  SendTouchEvent(event);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Give the touchmove a previously unseen pointer id; it should not be sent.
  int press_id = event.touches[0].id;
  event.MovePoint(0, 1, 1);
  event.touches[0].id = 7;
  SendTouchEvent(event);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Give the touchmove a valid id; it should be sent.
  event.touches[0].id = press_id;
  SendTouchEvent(event);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Do the same for release.
  event.ReleasePoint(0);
  event.touches[0].id = 11;
  SendTouchEvent(event);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Give the touchmove a valid id after release; it should be sent.
  event.touches[0].id = press_id;
  SendTouchEvent(event);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that touch points states are correct in TouchMove events.
TEST_F(PassthroughTouchEventQueueTest, PointerStatesInTouchMove) {
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);
  PressTouchPoint(3, 3);
  EXPECT_EQ(3U, queued_event_count());
  EXPECT_EQ(3U, GetAndResetSentEventCount());
  PressTouchPoint(4, 4);

  // Receive ACK for the first three touch-events.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());

  // Test current touches state before sending TouchMoves.
  const WebTouchEvent& event1 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchStart, event1.GetType());
  EXPECT_EQ(WebTouchPoint::kStateStationary, event1.touches[0].state);
  EXPECT_EQ(WebTouchPoint::kStateStationary, event1.touches[1].state);
  EXPECT_EQ(WebTouchPoint::kStateStationary, event1.touches[2].state);
  EXPECT_EQ(WebTouchPoint::kStatePressed, event1.touches[3].state);

  // Move x-position for 1st touch, y-position for 2nd touch
  // and do not move other touches.
  MoveTouchPoints(0, 1.1f, 1.f, 1, 2.f, 20.001f);
  MoveTouchPoints(2, 3.f, 3.f, 3, 4.f, 4.f);
  EXPECT_EQ(3U, queued_event_count());

  // Receive an ACK for the last TouchPress event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // 1st TouchMove is sent. Test for touches state.
  const WebTouchEvent& event2 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchMove, event2.GetType());
  EXPECT_EQ(WebTouchPoint::kStateMoved, event2.touches[0].state);
  EXPECT_EQ(WebTouchPoint::kStateMoved, event2.touches[1].state);
  EXPECT_EQ(WebTouchPoint::kStateStationary, event2.touches[2].state);
  EXPECT_EQ(WebTouchPoint::kStateStationary, event2.touches[3].state);

  // Move only 4th touch but not others.
  MoveTouchPoints(0, 1.1f, 1.f, 1, 2.f, 20.001f);
  MoveTouchPoints(2, 3.f, 3.f, 3, 4.1f, 4.1f);

  // Receive an ACK for previous (1st) TouchMove.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // 2nd TouchMove is sent. Test for touches state.
  const WebTouchEvent& event3 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchMove, event3.GetType());
  EXPECT_EQ(WebTouchPoint::kStateStationary, event3.touches[0].state);
  EXPECT_EQ(WebTouchPoint::kStateStationary, event3.touches[1].state);
  EXPECT_EQ(WebTouchPoint::kStateStationary, event3.touches[2].state);
  EXPECT_EQ(WebTouchPoint::kStateMoved, event3.touches[3].state);
}

// Tests that touch point state is correct in TouchMove events
// when point properties other than position changed.
TEST_F(PassthroughTouchEventQueueTest,
       PointerStatesWhenOtherThanPositionChanged) {
  PressTouchPoint(1, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Default initial radiusX/Y is (20.f, 20.f).
  // Default initial rotationAngle is 0.f.
  // Default initial force is 1.f.

  // Change touch point radius only.
  ChangeTouchPointRadius(0, 1.5f, 1.f);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // TouchMove is sent. Test for pointer state.
  const WebTouchEvent& event1 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchMove, event1.GetType());
  EXPECT_EQ(WebTouchPoint::kStateMoved, event1.touches[0].state);

  // Change touch point force.
  ChangeTouchPointForce(0, 0.9f);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // TouchMove is sent. Test for pointer state.
  const WebTouchEvent& event2 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchMove, event2.GetType());
  EXPECT_EQ(WebTouchPoint::kStateMoved, event2.touches[0].state);

  // Change touch point rotationAngle.
  ChangeTouchPointRotationAngle(0, 1.1f);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // TouchMove is sent. Test for pointer state.
  const WebTouchEvent& event3 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchMove, event3.GetType());
  EXPECT_EQ(WebTouchPoint::kStateMoved, event3.touches[0].state);

  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(4U, GetAndResetSentEventCount());
  EXPECT_EQ(4U, GetAndResetAckedEventCount());
}

// Tests that TouchMoves are filtered when none of the points are changed.
TEST_F(PassthroughTouchEventQueueTest, FilterTouchMovesWhenNoPointerChanged) {
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, GetAndResetAckedEventCount());

  // Move 1st touch point.
  MoveTouchPoint(0, 10, 10);
  EXPECT_EQ(1U, queued_event_count());

  // TouchMove should be allowed and test for touches state.
  const WebTouchEvent& event1 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchMove, event1.GetType());
  EXPECT_EQ(WebTouchPoint::kStateMoved, event1.touches[0].state);
  EXPECT_EQ(WebTouchPoint::kStateStationary, event1.touches[1].state);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Do not really move any touch points, but use previous values.
  MoveTouchPoint(0, 10, 10);
  ChangeTouchPointRadius(1, radius_x_, radius_y_);
  MoveTouchPoint(1, 2, 2);
  EXPECT_EQ(4U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  // The TouchMove but should be filtered when none of the touch points have
  // changed but don't get acked right away.
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Receive an ACK for 1st TouchMove.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(4U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // Move 2nd touch point.
  MoveTouchPoint(1, 3, 3);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());

  // TouchMove should be allowed and test for touches state.
  const WebTouchEvent& event2 = sent_event();
  EXPECT_EQ(WebInputEvent::kTouchMove, event2.GetType());
  EXPECT_EQ(WebTouchPoint::kStateStationary, event2.touches[0].state);
  EXPECT_EQ(WebTouchPoint::kStateMoved, event2.touches[1].state);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that touch-scroll-notification is queued normally.
TEST_F(PassthroughTouchEventQueueTest,
       TouchScrollNotificationOrder_EmptyQueue) {
  PrependTouchScrollNotification();

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

// Tests touch-scroll-notification firing is appended to the tail of sent
// events since all events are sent right away.
TEST_F(PassthroughTouchEventQueueTest,
       TouchScrollNotificationOrder_EndOfQueue) {
  PressTouchPoint(1, 1);

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Send the touch-scroll-notification when 3 events are in the queue.
  PrependTouchScrollNotification();

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Send ACKs.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_IGNORED);

  // Touch-scroll-start Ack is not reported to client.
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  EXPECT_EQ(WebInputEvent::kTouchStart, all_sent_events()[0].GetType());
  EXPECT_EQ(WebInputEvent::kTouchScrollStarted, all_sent_events()[1].GetType());
  EXPECT_EQ(2U, GetAndResetSentEventCount());
}

// Tests that if touchStartOrFirstTouchMove is correctly set up for touch
// events.
TEST_F(PassthroughTouchEventQueueTest, TouchStartOrFirstTouchMove) {
  PressTouchPoint(1, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kTouchStart, sent_event().GetType());
  EXPECT_TRUE(sent_event().touch_start_or_first_touch_move);

  MoveTouchPoint(0, 5, 5);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kTouchMove, sent_event().GetType());
  EXPECT_TRUE(sent_event().touch_start_or_first_touch_move);

  MoveTouchPoint(0, 15, 15);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kTouchMove, sent_event().GetType());
  EXPECT_FALSE(sent_event().touch_start_or_first_touch_move);

  ReleaseTouchPoint(0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kTouchEnd, sent_event().GetType());
  EXPECT_FALSE(sent_event().touch_start_or_first_touch_move);
}

TEST_F(PassthroughTouchEventQueueTest, TouchScrollStartedUnfiltered) {
  SyntheticWebTouchEvent event;
  event.SetType(WebInputEvent::kTouchScrollStarted);
  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest,
       TouchStartWithoutPageHandlersUnfiltered) {
  OnHasTouchEventHandlers(false);
  SyntheticWebTouchEvent event;
  event.PressPoint(1, 1);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, TouchStartWithPageHandlersUnfiltered) {
  OnHasTouchEventHandlers(true);
  SyntheticWebTouchEvent event;
  event.PressPoint(1, 1);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, TouchMoveFilteredAfterTimeout) {
  SetUpForTimeoutTesting();
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);

  // Allow the initial touch start event to time out.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // Any subsequent touch move events are filtered.
  SyntheticWebTouchEvent event;
  int id = event.PressPoint(1, 1);
  event.MovePoint(id, 2, 2);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kFilteredTimeout,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, TouchMoveWithoutPageHandlersUnfiltered) {
  OnHasTouchEventHandlers(false);
  // Start the touch sequence.
  PressTouchPoint(1, 1);

  SyntheticWebTouchEvent event;
  int id = event.PressPoint(1, 1);
  event.MovePoint(id, 2, 2);

  EXPECT_EQ(
      PassthroughTouchEventQueue::PreFilterResult::kFilteredNoPageHandlers,
      FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, StationaryTouchMoveFiltered) {
  OnHasTouchEventHandlers(true);
  // Start the touch sequence.
  PressTouchPoint(1, 1);

  // Touch move events with stationary pointers are filtered.
  SyntheticWebTouchEvent event;
  int id = event.PressPoint(1, 1);
  event.MovePoint(id, 1, 1);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::
                kFilteredNoNonstationaryPointers,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest,
       StationaryTouchMoveWithActualTouchMoveUnfiltered) {
  OnHasTouchEventHandlers(true);
  // Start the touch sequence.
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);

  // Touch move events with non stationary pointers are unfiltered, even if
  // there's another touch with a stationary pointer.
  SyntheticWebTouchEvent event;
  int id1 = event.PressPoint(1, 1);
  event.MovePoint(id1, 1, 1);
  int id2 = event.PressPoint(2, 2);
  event.MovePoint(id2, 3, 3);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, NonTouchMoveUnfiltered) {
  OnHasTouchEventHandlers(true);
  // Start the touch sequence.
  PressTouchPoint(1, 1);

  // Non-touchmove events are never filtered.
  SyntheticWebTouchEvent event;
  int id = event.PressPoint(1, 1);
  event.ReleasePoint(id);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, TouchMoveWithNonTouchMoveUnfiltered) {
  OnHasTouchEventHandlers(true);
  // Start the touch sequence.
  PressTouchPoint(1, 1);
  PressTouchPoint(1, 1);

  // Non-touchmove events are never filtered, even if they're paired with a
  // touchmove event that would otherwise be filtered.
  SyntheticWebTouchEvent event;
  int id1 = event.PressPoint(1, 1);
  event.MovePoint(id1, 1, 1);
  int id2 = event.PressPoint(2, 2);
  event.ReleasePoint(id2);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest,
       TouchMoveWithoutSequenceHandlerUnfiltered) {
  OnHasTouchEventHandlers(true);
  // Start the touch sequence.
  PressTouchPoint(1, 1);

  // Send an ack indicating that there's no handler for the current sequence.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Any subsequent touches in the sequence should be unfiltered.
  SyntheticWebTouchEvent event;
  int id = event.PressPoint(1, 1);
  event.MovePoint(id, 3, 3);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::
                kFilteredNoHandlerForSequence,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest,
       TouchStartUnfilteredWithForwardDiscrete) {
  SetUpForSkipFilterTesting(
      features::kSkipTouchEventFilterTypeParamValueDiscrete);

  OnHasTouchEventHandlers(false);
  SyntheticWebTouchEvent event;
  event.PressPoint(1, 1);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, TouchMoveFilteredWithForwardDiscrete) {
  SetUpForSkipFilterTesting(
      features::kSkipTouchEventFilterTypeParamValueDiscrete);

  OnHasTouchEventHandlers(false);
  // Start the touch sequence.
  PressTouchPoint(1, 1);

  SyntheticWebTouchEvent event;
  int id = event.PressPoint(1, 1);
  event.MovePoint(id, 2, 2);

  EXPECT_EQ(
      PassthroughTouchEventQueue::PreFilterResult::kFilteredNoPageHandlers,
      FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, TouchStartUnfilteredWithForwardAll) {
  SetUpForSkipFilterTesting(features::kSkipTouchEventFilterTypeParamValueAll);

  OnHasTouchEventHandlers(false);
  SyntheticWebTouchEvent event;
  event.PressPoint(1, 1);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

TEST_F(PassthroughTouchEventQueueTest, TouchMoveUnfilteredWithForwardAll) {
  SetUpForSkipFilterTesting(features::kSkipTouchEventFilterTypeParamValueAll);

  OnHasTouchEventHandlers(false);
  // Start the touch sequence.
  PressTouchPoint(1, 1);

  SyntheticWebTouchEvent event;
  int id = event.PressPoint(1, 1);
  event.MovePoint(id, 2, 2);

  EXPECT_EQ(PassthroughTouchEventQueue::PreFilterResult::kUnfiltered,
            FilterBeforeForwarding(event));
}

}  // namespace content
