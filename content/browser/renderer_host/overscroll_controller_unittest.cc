// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/overscroll_controller.h"

#include <memory>

#include "base/containers/queue.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/content_features.h"
#include "content/public/test/scoped_overscroll_modes.h"
#include "content/test/test_overscroll_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"

namespace content {

class OverscrollControllerTest : public ::testing::Test {
 protected:
  OverscrollControllerTest() {}
  ~OverscrollControllerTest() override {}

  void SetUp() override {
    OverscrollConfig::ResetTouchpadOverscrollHistoryNavigationEnabled();
    scoped_feature_list_.InitAndEnableFeature(
        features::kTouchpadOverscrollHistoryNavigation);
    delegate_ = std::make_unique<TestOverscrollDelegate>(gfx::Size(400, 300));
    controller_ = std::make_unique<OverscrollController>();
    controller_->set_delegate(delegate_.get());
  }

  void TearDown() override {
    controller_ = nullptr;
    delegate_ = nullptr;
  }

  // Creates and sends a mouse-wheel event to the overscroll controller. Returns
  // |true| if the event is consumed by the overscroll controller.
  bool SimulateMouseWheel(float dx, float dy) {
    DCHECK(!current_event_);
    current_event_ = std::make_unique<blink::WebMouseWheelEvent>(
        SyntheticWebMouseWheelEventBuilder::Build(
            0, 0, dx, dy, 0,
            ui::input_types::ScrollGranularity::kScrollByPrecisePixel));
    return controller_->WillHandleEvent(*current_event_);
  }

  // Creates and sends a gesture event to the overscroll controller. Returns
  // |true| if the event is consumed by the overscroll controller.
  bool SimulateGestureEvent(blink::WebInputEvent::Type type,
                            blink::WebGestureDevice source_device,
                            base::TimeTicks timestamp) {
    DCHECK(!current_event_);
    current_event_ = std::make_unique<blink::WebGestureEvent>(
        SyntheticWebGestureEventBuilder::Build(type, source_device));
    current_event_->SetTimeStamp(timestamp);
    return controller_->WillHandleEvent(*current_event_);
  }

  // Creates and sends a gesture-scroll-update event to the overscroll
  // controller. Returns |true| if the event is consumed by the overscroll
  // controller.
  bool SimulateGestureScrollUpdate(float dx,
                                   float dy,
                                   blink::WebGestureDevice device,
                                   base::TimeTicks timestamp,
                                   bool inertial_update) {
    DCHECK(!current_event_);
    auto event = std::make_unique<blink::WebGestureEvent>(
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0, device));
    event->SetTimeStamp(timestamp);
    if (inertial_update) {
      event->data.scroll_update.inertial_phase =
          blink::WebGestureEvent::InertialPhaseState::kMomentum;
    }
    current_event_ = std::move(event);
    return controller_->WillHandleEvent(*current_event_);
  }

  // Creates and sends a gesture-fling-start event to the overscroll controller.
  // Returns |true| if the event is consumed by the overscroll controller.
  bool SimulateGestureFlingStart(float velocity_x,
                                 float velocity_y,
                                 blink::WebGestureDevice device,
                                 base::TimeTicks timestamp) {
    DCHECK(!current_event_);
    current_event_ = std::make_unique<blink::WebGestureEvent>(
        SyntheticWebGestureEventBuilder::BuildFling(velocity_x, velocity_y,
                                                    device));
    current_event_->SetTimeStamp(timestamp);
    return controller_->WillHandleEvent(*current_event_);
  }

  // Notifies the overscroll controller that the current event is ACKed.
  void SimulateAck(bool processed) {
    DCHECK(current_event_);
    controller_->ReceivedEventACK(*current_event_, processed);
    current_event_ = nullptr;
  }

  TestOverscrollDelegate* delegate() const { return delegate_.get(); }

  OverscrollMode controller_mode() const {
    return controller_->overscroll_mode_;
  }

  OverscrollSource controller_source() const {
    return controller_->overscroll_source_;
  }

 private:
  std::unique_ptr<TestOverscrollDelegate> delegate_;
  std::unique_ptr<OverscrollController> controller_;

  // Keeps track of the last event that has been processed by the overscroll
  // controller which is not yet ACKed. Will be null if no event is processed or
  // the last event is ACKed.
  std::unique_ptr<blink::WebInputEvent> current_event_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollControllerTest);
};

// Tests that if a mouse-wheel is consumed by content before overscroll is
// initiated, overscroll will not initiate anymore.
TEST_F(OverscrollControllerTest, MouseWheelConsumedPreventsOverscroll) {
  const base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  // Simulate a mouse-wheel, ACK it as not processed, simulate the corresponding
  // gesture scroll-update event, and ACK it as not processed. Since it is not
  // passing the start threshold, no overscroll should happen.
  EXPECT_FALSE(SimulateMouseWheel(10, 0));
  SimulateAck(false);
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      10, 0, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Simulate a mouse-wheel and ACK it as processed. No gesture scroll-update
  // needs to be simulated. Still no overscroll.
  EXPECT_FALSE(SimulateMouseWheel(10, 0));
  SimulateAck(true);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Simulate a mouse-wheel and the corresponding gesture scroll-update both
  // ACKed as not processed. Although the scroll passes overscroll start
  // threshold, no overscroll should happen since the previous mouse-wheel was
  // marked as processed.
  EXPECT_FALSE(SimulateMouseWheel(100, 0));
  SimulateAck(false);
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      100, 0, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

// Verifying the inertial scroll event completes overscroll. After that we will
// ignore the following inertial scroll events until new sequence start.
TEST_F(OverscrollControllerTest,
       InertialGestureScrollUpdateCompletesOverscroll) {
  const base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchpad,
                                    timestamp));
  SimulateAck(false);

  EXPECT_FALSE(SimulateGestureScrollUpdate(
      200, 0, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_EAST, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, controller_source());
  EXPECT_EQ(OVERSCROLL_EAST, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Inertial update event complete the overscroll action.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      100, 0, blink::WebGestureDevice::kTouchpad, timestamp, true));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_EAST, delegate()->completed_mode());

  // Next Inertial update event would be consumed by overscroll controller.
  EXPECT_TRUE(SimulateGestureScrollUpdate(
      100, 0, blink::WebGestureDevice::kTouchpad, timestamp, true));
}

// Ensure inertial gesture scroll update can not start overscroll.
TEST_F(OverscrollControllerTest, InertialGSUsDoNotStartOverscroll) {
  base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();
  // Inertial update event complete the overscroll action.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      100, 0, blink::WebGestureDevice::kTouchpad, timestamp, true));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

// After 300ms inertial gesture scroll updates, overscroll must get cancelled
// if not completed.
TEST_F(OverscrollControllerTest, OnlyProcessLimitedInertialGSUEvents) {
  base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchpad,
                                    timestamp));
  SimulateAck(false);

  EXPECT_FALSE(SimulateGestureScrollUpdate(
      61, 0, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_EAST, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, controller_source());
  EXPECT_EQ(OVERSCROLL_EAST, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // First inertial.
  timestamp += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(SimulateGestureScrollUpdate(
      1, 0, blink::WebGestureDevice::kTouchpad, timestamp, true));
  SimulateAck(true);
  EXPECT_EQ(OVERSCROLL_EAST, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, controller_source());
  EXPECT_EQ(OVERSCROLL_EAST, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Not cancel in 10ms.
  timestamp += base::TimeDelta::FromMilliseconds(10);
  EXPECT_TRUE(SimulateGestureScrollUpdate(
      1, 0, blink::WebGestureDevice::kTouchpad, timestamp, true));
  SimulateAck(true);
  EXPECT_EQ(OVERSCROLL_EAST, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, controller_source());
  EXPECT_EQ(OVERSCROLL_EAST, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Cancel after 300ms.
  timestamp += base::TimeDelta::FromMilliseconds(291);
  EXPECT_TRUE(SimulateGestureScrollUpdate(
      1, 0, blink::WebGestureDevice::kTouchpad, timestamp, true));
  SimulateAck(true);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // Next event should be ignored.
  timestamp += base::TimeDelta::FromMilliseconds(100);
  EXPECT_TRUE(SimulateGestureScrollUpdate(
      1, 0, blink::WebGestureDevice::kTouchpad, timestamp, true));
}

// Verifies that when pull-to-refresh is disabled, it is not triggered for
// neither touchpad nor touchscreen.
TEST_F(OverscrollControllerTest, PullToRefreshDisabled) {
  ScopedPullToRefreshMode scoped_mode(
      OverscrollConfig::PullToRefreshMode::kDisabled);

  base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchpad,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchpad gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should not be
  // triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchpad zero-velocity fling-start which would normally end
  // pull-to-refresh, and ACK it as not processed. Nothing should happen.
  EXPECT_FALSE(SimulateGestureFlingStart(
      0, 0, blink::WebGestureDevice::kTouchpad, timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should not be
  // triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which would normally end
  // pull-to-refresh, and ACK it as not processed. Nothing should happen.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

// Verifies that when pull-to-refresh is enabled, it is triggered for both
// touchpad and touchscreen.
TEST_F(OverscrollControllerTest, PullToRefreshEnabled) {
  ScopedPullToRefreshMode scoped_mode(
      OverscrollConfig::PullToRefreshMode::kEnabled);

  base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchpad,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchpad gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_SOUTH, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHPAD, controller_source());
  EXPECT_EQ(OVERSCROLL_SOUTH, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchpad zero-velocity fling-start and ACK it as not processed..
  // It should abort pull-to-refresh.
  EXPECT_FALSE(SimulateGestureFlingStart(
      0, 0, blink::WebGestureDevice::kTouchpad, timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_SOUTH, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, controller_source());
  EXPECT_EQ(OVERSCROLL_SOUTH, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end and ACK it as not processed. It
  // should abort pull-to-refresh.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

// Verifies that when pull-to-refresh is enabled only for touchscreen, it is
// triggered for touchscreen but not for touchpad.
TEST_F(OverscrollControllerTest, PullToRefreshEnabledTouchscreen) {
  ScopedPullToRefreshMode scoped_mode(
      OverscrollConfig::PullToRefreshMode::kEnabledTouchschreen);

  base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchpad,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchpad gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should not be
  // triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchpad zero-velocity fling-start which would normally end
  // pull-to-refresh, and ACK it as not processed. Nothing should happen.
  EXPECT_FALSE(SimulateGestureFlingStart(
      0, 0, blink::WebGestureDevice::kTouchpad, timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_SOUTH, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, controller_source());
  EXPECT_EQ(OVERSCROLL_SOUTH, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end and ACK it as not processed. It
  // should abort pull-to-refresh.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

// Ensure disabling kTouchpadOverscrollHistoryNavigation will prevent overscroll
// from touchpad.
TEST_F(OverscrollControllerTest, DisableTouchpadOverscrollHistoryNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kTouchpadOverscrollHistoryNavigation);
  ASSERT_FALSE(OverscrollConfig::TouchpadOverscrollHistoryNavigationEnabled());

  const base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureScrollUpdate(
      200, 0, blink::WebGestureDevice::kTouchpad, timestamp, false));
  SimulateAck(false);

  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

// Verifies that if an overscroll happens before cool off period after a page
// scroll, it does not trigger pull-to-refresh. Verifies following sequence of
// scrolls:
//  1) Page scroll;
//  2) Scroll before cool off -> PTR not triggered;
//  3) Scroll before cool off -> PTR not triggered;
//  4) Scroll after cool off  -> PTR triggered;
//  5) Scroll before cool off -> PTR triggered.
TEST_F(OverscrollControllerTest, PullToRefreshBeforeCoolOff) {
  ScopedPullToRefreshMode scoped_mode(
      OverscrollConfig::PullToRefreshMode::kEnabled);

  // 1) Page scroll.
  base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as processed. Pull-to-refresh should not be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(true);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which would normally end
  // pull-to-refresh, and ACK it as not processed. Nothing should happen.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // 2) Scroll before cool off -> PTR not triggered.
  timestamp += base::TimeDelta::FromMilliseconds(500);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should not be
  // triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which would normally end
  // pull-to-refresh, and ACK it as not processed. Nothing should happen.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // 3) Scroll before cool off -> PTR not triggered.
  timestamp += base::TimeDelta::FromMilliseconds(500);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should not be
  // triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which would normally end
  // pull-to-refresh, and ACK it as not processed. Nothing should happen.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // 4) Scroll after cool off -> PTR triggered.
  timestamp += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_SOUTH, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, controller_source());
  EXPECT_EQ(OVERSCROLL_SOUTH, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which will end pull-to-refresh,
  // and ACK it as not processed. Pull-to-refresh should be aborted.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // 5) Scroll before cool off -> PTR triggered.
  timestamp += base::TimeDelta::FromMilliseconds(500);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_SOUTH, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, controller_source());
  EXPECT_EQ(OVERSCROLL_SOUTH, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which will end pull-to-refresh,
  // and ACK it as not processed. Pull-to-refresh should be aborted.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

// Verifies that if an overscroll happens after cool off period after a page
// scroll, it triggers pull-to-refresh. Verifies the following sequence of
// scrolls:
//  1) Page scroll;
//  2) Scroll after cool off  -> PTR triggered;
//  3) Scroll before cool off -> PTR triggered;
TEST_F(OverscrollControllerTest, PullToRefreshAfterCoolOff) {
  ScopedPullToRefreshMode scoped_mode(
      OverscrollConfig::PullToRefreshMode::kEnabled);

  // 1) Page scroll.
  base::TimeTicks timestamp =
      blink::WebInputEvent::GetStaticTimeStampForTests();

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as processed. Pull-to-refresh should not be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(true);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which would normally end
  // pull-to-refresh, and ACK it as not processed. Nothing should happen.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // 2) Scroll after cool off -> PTR triggered.
  timestamp += base::TimeDelta::FromSeconds(1);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_SOUTH, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, controller_source());
  EXPECT_EQ(OVERSCROLL_SOUTH, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which will end pull-to-refresh,
  // and ACK it as not processed. Pull-to-refresh should be aborted.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  // 3) Scroll before cool off -> PTR triggered.
  timestamp += base::TimeDelta::FromMilliseconds(500);

  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollBegin,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);

  // Simulate a touchscreen gesture scroll-update event that passes the start
  // threshold and ACK it as not processed. Pull-to-refresh should be triggered.
  EXPECT_FALSE(SimulateGestureScrollUpdate(
      0, 80, blink::WebGestureDevice::kTouchscreen, timestamp, false));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_SOUTH, controller_mode());
  EXPECT_EQ(OverscrollSource::TOUCHSCREEN, controller_source());
  EXPECT_EQ(OVERSCROLL_SOUTH, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());

  timestamp += base::TimeDelta::FromSeconds(1);

  // Simulate a touchscreen gesture scroll-end which will end pull-to-refresh,
  // and ACK it as not processed. Pull-to-refresh should be aborted.
  EXPECT_FALSE(SimulateGestureEvent(blink::WebInputEvent::kGestureScrollEnd,
                                    blink::WebGestureDevice::kTouchscreen,
                                    timestamp));
  SimulateAck(false);
  EXPECT_EQ(OVERSCROLL_NONE, controller_mode());
  EXPECT_EQ(OverscrollSource::NONE, controller_source());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->current_mode());
  EXPECT_EQ(OVERSCROLL_NONE, delegate()->completed_mode());
}

}  // namespace content
