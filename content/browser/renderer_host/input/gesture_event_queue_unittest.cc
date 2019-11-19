// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_event_queue.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/input_router_config_helper.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/common/input_event_ack_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/blink/blink_features.h"

using base::TimeDelta;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

class GestureEventQueueTest : public testing::Test,
                              public GestureEventQueueClient,
                              public FlingControllerEventSenderClient,
                              public FlingControllerSchedulerClient {
 public:
  GestureEventQueueTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        acked_gesture_event_count_(0),
        sent_gesture_event_count_(0) {}

  ~GestureEventQueueTest() override {}

  // testing::Test
  void SetUp() override {
    queue_.reset(new GestureEventQueue(this, this, this, DefaultConfig()));
  }

  void TearDown() override {
    // Process all pending tasks to avoid leaks.
    RunUntilIdle();
    queue_.reset();
  }

  void SetUpForTapSuppression(int max_cancel_to_down_time_ms) {
    GestureEventQueue::Config gesture_config;
    gesture_config.fling_config.touchscreen_tap_suppression_config.enabled =
        true;
    gesture_config.fling_config.touchscreen_tap_suppression_config
        .max_cancel_to_down_time =
        base::TimeDelta::FromMilliseconds(max_cancel_to_down_time_ms);
    queue_.reset(new GestureEventQueue(this, this, this, gesture_config));
  }

  // GestureEventQueueClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) override {
    ++sent_gesture_event_count_;
    if (sync_ack_result_) {
      std::unique_ptr<InputEventAckState> ack_result =
          std::move(sync_ack_result_);
      SendInputEventACK(event.event.GetType(), *ack_result);
    }
  }

  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckSource ack_source,
                         InputEventAckState ack_result) override {
    ++acked_gesture_event_count_;
    last_acked_event_ = event.event;
    if (sync_followup_event_) {
      auto sync_followup_event = std::move(sync_followup_event_);
      SimulateGestureEvent(*sync_followup_event);
    }
  }

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {}
  void SendGeneratedGestureScrollEvents(
      const GestureEventWithLatencyInfo& gesture_event) override {}
  gfx::Size GetRootWidgetViewportSize() override {
    return gfx::Size(1920, 1080);
  }

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override {}
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override {}
  bool NeedsBeginFrameForFlingProgress() override { return false; }

 protected:
  static GestureEventQueue::Config DefaultConfig() {
    return GestureEventQueue::Config();
  }

  void SetUpForDebounce(int interval_ms) {
    queue()->set_debounce_interval_time_ms_for_testing(interval_ms);
  }

  void SimulateGestureEvent(const WebGestureEvent& gesture) {
    GestureEventWithLatencyInfo gesture_event(gesture);
    if (!queue()->PassToFlingController(gesture_event)) {
      queue()->DebounceOrForwardEvent(gesture_event);
    }
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGSEGeneratedByFlingController(WebGestureDevice sourceDevice) {
    WebGestureEvent gesture_scroll_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGestureScrollEnd, sourceDevice);
    gesture_scroll_end.data.scroll_end.generated_by_fling_controller = true;
    SimulateGestureEvent(gesture_scroll_end);
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollUpdate(
        dX, dY, modifiers, blink::WebGestureDevice::kTouchscreen));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale, anchorX, anchorY, modifiers,
        blink::WebGestureDevice::kTouchscreen));
  }

  void SimulateGestureFlingStartEvent(float velocityX,
                                      float velocityY,
                                      WebGestureDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::BuildFling(velocityX,
                                                    velocityY,
                                                    sourceDevice));
  }

  void SendInputEventACK(WebInputEvent::Type type,
                         InputEventAckState ack) {
    queue()->ProcessGestureAck(InputEventAckSource::COMPOSITOR_THREAD, ack,
                               type, ui::LatencyInfo());
  }

  void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  size_t GetAndResetSentGestureEventCount() {
    size_t count = sent_gesture_event_count_;
    sent_gesture_event_count_ = 0;
    return count;
  }

  size_t GetAndResetAckedGestureEventCount() {
    size_t count = acked_gesture_event_count_;
    acked_gesture_event_count_ = 0;
    return count;
  }

  const WebGestureEvent& last_acked_event() const {
    return last_acked_event_;
  }

  void set_synchronous_ack(InputEventAckState ack_result) {
    sync_ack_result_.reset(new InputEventAckState(ack_result));
  }

  void set_sync_followup_event(WebInputEvent::Type type,
                               WebGestureDevice sourceDevice) {
    sync_followup_event_.reset(new WebGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice)));
  }

  unsigned GestureEventQueueSize() {
    return queue()->sent_events_awaiting_ack_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() {
    return queue()
        ->sent_events_awaiting_ack_.at(GestureEventQueueSize() - 2)
        .event;
  }

  WebGestureEvent GestureEventLastQueueEvent() {
    return queue()->sent_events_awaiting_ack_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() {
    return queue()->debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) {
    return queue()->sent_events_awaiting_ack_.at(i).event;
  }

  bool ScrollingInProgress() {
    return queue()->scrolling_in_progress_;
  }

  bool FlingInProgress() { return queue()->FlingInProgressForTest(); }

  GestureEventQueue* queue() const {
    return queue_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<GestureEventQueue> queue_;
  size_t acked_gesture_event_count_;
  size_t sent_gesture_event_count_;
  WebGestureEvent last_acked_event_;
  std::unique_ptr<InputEventAckState> sync_ack_result_;
  std::unique_ptr<WebGestureEvent> sync_followup_event_;
  base::test::ScopedFeatureList feature_list_;
};

// This is for tests that are to be run for all source devices.
class GestureEventQueueWithSourceTest
    : public GestureEventQueueTest,
      public testing::WithParamInterface<WebGestureDevice> {};

class GestureEventQueueWithCompositorEventQueueTest
    : public GestureEventQueueTest {};

// Tests a single event with an synchronous ack.
TEST_F(GestureEventQueueTest, SimpleSyncAck) {
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an synchronous ack which enqueues an additional event.
TEST_F(GestureEventQueueTest, SyncAckQueuesEvent) {
  std::unique_ptr<WebGestureEvent> queued_event;
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  set_sync_followup_event(WebInputEvent::kGestureShowPress,
                          blink::WebGestureDevice::kTouchscreen);
  // This event enqueues the show press event.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::kGestureShowPress,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

INSTANTIATE_TEST_SUITE_P(AllSources,
                         GestureEventQueueWithSourceTest,
                         testing::Values(blink::WebGestureDevice::kTouchscreen,
                                         blink::WebGestureDevice::kTouchpad));

// Test that a GestureScrollEnd is deferred during the debounce interval,
// that Scrolls are not and that the deferred events are sent after that
// timer fires.
TEST_F(GestureEventQueueTest, DebounceDefersFollowingGestureEvents) {
  SetUpForDebounce(3);

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TimeDelta::FromMilliseconds(5));
  run_loop.Run();

  // The deferred events are correctly queued in coalescing queue.
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(4U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::kGestureScrollUpdate,
                                    WebInputEvent::kGestureScrollUpdate,
                                    WebInputEvent::kGestureScrollEnd};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
       i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

// Tests that GSE events generated by the fling controller are forwarded to the
// renderer instead of getting pushed back to the deboucing_deferral_queue_. In
// this case the following GSB won't get deferred either.
TEST_F(GestureEventQueueTest,
       DebounceDoesNotDeferGSEsGeneratedByFlingController) {
  SetUpForDebounce(3);

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGSEGeneratedByFlingController(blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::kGestureScrollUpdate,
                                    WebInputEvent::kGestureScrollEnd,
                                    WebInputEvent::kGestureScrollBegin};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
       i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

TEST_F(GestureEventQueueTest, DebounceDefersGSBIfPreviousGSEDeferred) {
  SetUpForDebounce(3);
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());
}

TEST_F(GestureEventQueueTest, DebounceDefersGSBIfPreviousGSEDropped) {
  SetUpForDebounce(3);
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::kGestureScrollUpdate,
                                    WebInputEvent::kGestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
       i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

// Test that non-scroll events are deferred while scrolling during the debounce
// interval and are discarded if a GestureScrollUpdate event arrives before the
// interval end.
TEST_F(GestureEventQueueTest, DebounceDropsDeferredEvents) {
  SetUpForDebounce(3);

  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // This event should get discarded.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::kGestureScrollUpdate,
                                    WebInputEvent::kGestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

// Test that the fling cancelling tap down event and its following tap get
// suppressed when tap suppression is enabled.
TEST_F(GestureEventQueueTest, TapGetsSuppressedAfterTapDownCancelsFling) {
  SetUpForTapSuppression(400);
  // The velocity of the event must be large enough to make sure that the fling
  // is still active when the tap down happens.
  SimulateGestureFlingStartEvent(0, -1000,
                                 blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(FlingInProgress());
  // The fling start event is not sent to the renderer.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());
  RunUntilIdle();

  // Simulate a fling cancel event before sending a gesture tap down event. The
  // fling cancel event is not sent to the renderer.
  SimulateGestureEvent(WebInputEvent::kGestureFlingCancel,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  RunUntilIdle();

  // Simulate a fling cancelling tap down. The tap down must get suppressed
  // since the fling cancel event is processed by the fling controller.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GestureEventQueueSize());

  // The tap event must get suppressed since its corresponding tap down event
  // is suppressed.
  SimulateGestureEvent(WebInputEvent::kGestureTap,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueWithCompositorEventQueueTest,
       PreserveOrderWithOutOfOrderAck) {
  // Simulate a scroll sequence, events should be ACKed in original order.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SimulateGestureScrollUpdateEvent(8, -4, 1);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);

  // All events should have been sent.
  EXPECT_EQ(3U, GetAndResetSentGestureEventCount());

  // Simulate GSB ACK.
  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin, last_acked_event().GetType());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // Simulate GSE ACK first since it's usually dispatched non-blocking.
  SendInputEventACK(WebInputEvent::kGestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // GSE ACK will be cached in GestureEventQueue since we haven't ACKed GSU yet.
  EXPECT_EQ(WebInputEvent::kGestureScrollBegin, last_acked_event().GetType());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // Simulate GSU ACK.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // Both ACKs should be released in order.
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd, last_acked_event().GetType());
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueWithCompositorEventQueueTest,
       MultipleGesturesInFlight) {
  // Simulate a pinch sequence, events should be forwarded immediately.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            GestureEventLastQueueEvent().GetType());

  // Simulate 2 pinch update events.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());
  SimulateGesturePinchUpdateEvent(1.3, 60, 60, 1);
  // Events should be forwarded immediately instead of being coalesced.
  EXPECT_EQ(5U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            GestureEventLastQueueEvent().GetType());

  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Both GestureScrollUpdate and GesturePinchUpdate should have been sent.
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Ack the last 2 GesturePinchUpdate events.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

}  // namespace content
