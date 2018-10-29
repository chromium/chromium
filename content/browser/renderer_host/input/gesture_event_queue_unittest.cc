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
#include "base/test/scoped_task_environment.h"
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
  GestureEventQueueTest() : GestureEventQueueTest(false) {}

  GestureEventQueueTest(bool enable_compositor_event_queue)
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI),
        acked_gesture_event_count_(0),
        sent_gesture_event_count_(0) {
    if (enable_compositor_event_queue)
      feature_list_.InitAndEnableFeature(features::kVsyncAlignedInputEvents);
    else
      feature_list_.InitAndDisableFeature(features::kVsyncAlignedInputEvents);
  }

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
    if (!queue()->FlingControllerFilterEvent(gesture_event)) {
      queue()->DebounceOrQueueEvent(gesture_event);
    }
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollUpdate(
        dX, dY, modifiers, blink::kWebGestureDeviceTouchscreen));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale, anchorX, anchorY, modifiers,
        blink::kWebGestureDeviceTouchscreen));
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
    return queue()->coalesced_gesture_events_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() {
    return queue()->coalesced_gesture_events_.at(
        GestureEventQueueSize() - 2).event;
  }

  WebGestureEvent GestureEventLastQueueEvent() {
    return queue()->coalesced_gesture_events_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() {
    return queue()->debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) {
    return queue()->coalesced_gesture_events_.at(i).event;
  }

  bool ScrollingInProgress() {
    return queue()->scrolling_in_progress_;
  }

  bool FlingInProgress() { return queue()->FlingInProgressForTest(); }
  bool FlingCancellationIsDeferred() {
    return queue()->FlingCancellationIsDeferred();
  }

  bool WillIgnoreNextACK() {
    return queue()->ignore_next_ack_;
  }

  GestureEventQueue* queue() const {
    return queue_.get();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
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
    : public GestureEventQueueTest {
 public:
  GestureEventQueueWithCompositorEventQueueTest()
      : GestureEventQueueTest(true) {}
};

TEST_F(GestureEventQueueTest, CoalescesScrollGestureEvents) {
  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -5, 0);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Coalesced.
  SimulateGestureScrollUpdateEvent(8, -6, 0);

  // Check that coalescing updated the correct values.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(0, merged_event.GetModifiers());
  EXPECT_EQ(16, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-11, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -7, 1);

  // Check that we didn't wrongly coalesce.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Different.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);

  // Check that only the first event was sent.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Ack for queued coalesced event.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Ack for queued uncoalesced event.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::kGestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

TEST_F(GestureEventQueueTest,
       DoesNotCoalesceScrollGestureEventsFromDifferentDevices) {
  // Test that GestureScrollUpdate events from Touchscreen and Touchpad do not
  // coalesce.

  // Sent.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -5, 0);

  // Make sure that the queue contains what we think it should.
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen,
            GestureEventLastQueueEvent().SourceDevice());

  // Coalesced.
  SimulateGestureScrollUpdateEvent(8, -6, 0);
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen,
            GestureEventLastQueueEvent().SourceDevice());

  // Enqueued.
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(blink::kWebGestureDeviceTouchpad,
            GestureEventLastQueueEvent().SourceDevice());

  // Coalesced.
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(blink::kWebGestureDeviceTouchpad,
            GestureEventLastQueueEvent().SourceDevice());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -7, 0);
  EXPECT_EQ(4U, GestureEventQueueSize());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen,
            GestureEventLastQueueEvent().SourceDevice());
}

TEST_F(GestureEventQueueTest, CoalescesScrollAndPinchEvents) {
  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);

  // Sent.
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -4, 1);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(1.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(8, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-4, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(1.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(12, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-6, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(3, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(12, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-6, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(6, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(12, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-6, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Check that only the first event was sent.
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -6, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(6, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(13, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-7, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // At this point ACKs shouldn't be getting ignored.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Check that the ACK sends both scroll and pinch updates.
  SendInputEventACK(WebInputEvent::kGesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // The next ACK should be getting ignored.
  EXPECT_TRUE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(1, -1, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(1, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-1, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(6, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 1);

  // Coalescing scrolls should still work.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(3, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-3, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(6, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Enqueued.
  SimulateGesturePinchUpdateEvent(0.5, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(0.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(3, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-3, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Check that the ACK gets ignored.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  // The flag should have been flipped back to false.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 2);

  // Shouldn't coalesce with different modifiers.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(2, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-2, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(2, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(0.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  EXPECT_EQ(blink::kWebGestureDeviceTouchscreen, merged_event.SourceDevice());

  // Check that the ACK sends the next scroll pinch pair.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second event.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Check that the queue is empty after ACK and no events get sent.
  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueTest, CoalescesMultiplePinchEventSequences) {
  // Simulate a pinch sequence.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  size_t expected_events_in_queue = 3;
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(++expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(1.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(8, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-4, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(1.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(12, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-6, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());

  // Now start another sequence before the previous sequence has been ack'ed.
  SimulateGestureEvent(WebInputEvent::kGesturePinchEnd,
                       blink::kWebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  expected_events_in_queue += 5;
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 30, 30, 1);
  EXPECT_EQ(++expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(1.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(8, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-4, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, merged_event.GetType());
  EXPECT_EQ(1.5, merged_event.data.pinch_update.scale);
  EXPECT_EQ(1, merged_event.GetModifiers());
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());
  EXPECT_EQ(12, merged_event.data.scroll_update.delta_x);
  EXPECT_EQ(-6, merged_event.data.scroll_update.delta_y);
  EXPECT_EQ(1, merged_event.GetModifiers());
}

TEST_F(GestureEventQueueTest, CoalescesPinchSequencesWithEarlyAck) {
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SendInputEventACK(WebInputEvent::kGesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // ScrollBegin and PinchBegin have been sent
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            GestureEventLastQueueEvent().GetType());
  EXPECT_EQ(1U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            GestureEventLastQueueEvent().GetType());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(3, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            GestureEventLastQueueEvent().GetType());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  // The coalesced pinch/scroll pair will have been re-arranged, with the
  // pinch following the scroll.
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            GestureEventLastQueueEvent().GetType());
  EXPECT_EQ(4U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(4, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(2.f, last_acked_event().data.pinch_update.scale);

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());

  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(3.f * 4.f, last_acked_event().data.pinch_update.scale);

  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueTest,
       DoesNotCoalescePinchGestureEventsWithDifferentModifiers) {
  // Insert an event to force queueing of gestures.
  SimulateGestureEvent(WebInputEvent::kGestureTapCancel,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(3, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(10, 15, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(4, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Using different modifiers should prevent coalescing.
  SimulateGesturePinchUpdateEvent(5, 60, 60, 2);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(4U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(6, 60, 60, 3);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(5U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureTapCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(3.f * 4.f, last_acked_event().data.pinch_update.scale);
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(5.f, last_acked_event().data.pinch_update.scale);
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(6.f, last_acked_event().data.pinch_update.scale);
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueTest, CoalescesScrollAndPinchEventsIdentity) {
  // Insert an event to force queueing of gestures.
  SimulateGestureEvent(WebInputEvent::kGestureTapCancel,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());

  // Ensure that coalescing yields an identity transform for any pinch/scroll
  // pair combined with its inverse.
  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(5, 10, 10, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(.2f, 10, 10, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(-5, -5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureTapCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  EXPECT_EQ(0.f, last_acked_event().data.scroll_update.delta_x);
  EXPECT_EQ(0.f, last_acked_event().data.scroll_update.delta_y);

  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(1.f, last_acked_event().data.pinch_update.scale);
  EXPECT_EQ(0U, GestureEventQueueSize());

  // Insert an event to force queueing of gestures.
  SimulateGestureEvent(WebInputEvent::kGestureTapCancel,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());

  // Ensure that coalescing yields an identity transform for any pinch/scroll
  // pair combined with its inverse.
  SimulateGesturePinchUpdateEvent(2, 10, 10, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(20, 20, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(0.5f, 20, 20, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(-5, -5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureTapCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  EXPECT_EQ(0.f, last_acked_event().data.scroll_update.delta_x);
  EXPECT_EQ(0.f, last_acked_event().data.scroll_update.delta_y);

  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(1.f, last_acked_event().data.pinch_update.scale);
}

// Tests a single event with an synchronous ack.
TEST_F(GestureEventQueueTest, SimpleSyncAck) {
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an synchronous ack which enqueues an additional event.
TEST_F(GestureEventQueueTest, SyncAckQueuesEvent) {
  std::unique_ptr<WebGestureEvent> queued_event;
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  set_sync_followup_event(WebInputEvent::kGestureShowPress,
                          blink::kWebGestureDeviceTouchscreen);
  // This event enqueues the show press event.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::kGestureShowPress,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an async ack followed by an event with a sync ack.
TEST_F(GestureEventQueueTest, AsyncThenSyncAck) {
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);

  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());

  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::kGestureTapDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetAckedGestureEventCount());
}

TEST_F(GestureEventQueueTest, CoalescesScrollAndPinchEventWithSyncAck) {
  // Simulate a pinch sequence.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, merged_event.GetType());

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::kGestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Ack the PinchBegin, and schedule a synchronous ack for GestureScrollUpdate.
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::kGesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Both GestureScrollUpdate and GesturePinchUpdate should have been sent.
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate, last_acked_event().GetType());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // Ack the final GesturePinchUpdate.
  SendInputEventACK(WebInputEvent::kGesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate, last_acked_event().GetType());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

INSTANTIATE_TEST_CASE_P(AllSources,
                        GestureEventQueueWithSourceTest,
                        testing::Values(blink::kWebGestureDeviceTouchscreen,
                                        blink::kWebGestureDeviceTouchpad));

// Test that a GestureScrollEnd is deferred during the debounce interval,
// that Scrolls are not and that the deferred events are sent after that
// timer fires.
TEST_F(GestureEventQueueTest, DebounceDefersFollowingGestureEvents) {
  SetUpForDebounce(3);

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());

  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TimeDelta::FromMilliseconds(5));
  run_loop.Run();

  // The deferred events are correctly queued in coalescing queue.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
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

// Test that non-scroll events are deferred while scrolling during the debounce
// interval and are discarded if a GestureScrollUpdate event arrives before the
// interval end.
TEST_F(GestureEventQueueTest, DebounceDropsDeferredEvents) {
  SetUpForDebounce(3);

  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // This event should get discarded.
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
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
TEST_F(GestureEventQueueTest, TapGetsSuppressedAfterTapDownCancellsFling) {
  SetUpForTapSuppression(400);
  // The velocity of the event must be large enough to make sure that the fling
  // is still active when the tap down happens.
  SimulateGestureFlingStartEvent(0, -1000, blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(FlingInProgress());
  // The fling start event is not sent to the renderer.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());
  RunUntilIdle();

  // Simulate a fling cancel event before sending a gesture tap down event. The
  // fling cancel event is not sent to the renderer.
  SimulateGestureEvent(WebInputEvent::kGestureFlingCancel,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_TRUE(FlingCancellationIsDeferred());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  RunUntilIdle();

  // Simulate a fling cancelling tap down. The tap down must get suppressed
  // since the fling cancel event is processed by the fling controller.
  SimulateGestureEvent(WebInputEvent::kGestureTapDown,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GestureEventQueueSize());

  // The tap event must get suppressed since its corresponding tap down event
  // is suppressed.
  SimulateGestureEvent(WebInputEvent::kGestureTap,
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueTest, CoalescesSyntheticScrollBeginEndEvents) {
  // Test coalescing of only GestureScrollBegin/End events.
  SimulateGestureEvent(WebInputEvent::kGestureScrollUpdate,
                       blink::kWebGestureDeviceTouchpad);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());

  WebGestureEvent synthetic_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, blink::kWebGestureDeviceTouchpad);
  synthetic_end.data.scroll_end.synthetic = true;

  SimulateGestureEvent(synthetic_end);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // Synthetic begin will remove the unsent synthetic end.
  WebGestureEvent synthetic_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollBegin, blink::kWebGestureDeviceTouchpad);
  synthetic_begin.data.scroll_begin.synthetic = true;

  SimulateGestureEvent(synthetic_begin);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueWithCompositorEventQueueTest,
       PreserveOrderWithOutOfOrderAck) {
  // Simulate a scroll sequence, events should be ACKed in original order.
  SimulateGestureEvent(WebInputEvent::kGestureScrollBegin,
                       blink::kWebGestureDeviceTouchscreen);
  SimulateGestureScrollUpdateEvent(8, -4, 1);
  SimulateGestureEvent(WebInputEvent::kGestureScrollEnd,
                       blink::kWebGestureDeviceTouchscreen);

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
                       blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SimulateGestureEvent(WebInputEvent::kGesturePinchBegin,
                       blink::kWebGestureDeviceTouchscreen);
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
