// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_action_filter.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/common/input_event_ack_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/blink/blink_features.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {
namespace {

const blink::WebGestureDevice kSourceDevice =
    blink::WebGestureDevice::kTouchscreen;

}  // namespace

class TouchActionFilterTest : public testing::Test,
                              public testing::WithParamInterface<bool> {
 public:
  TouchActionFilterTest() : compositor_touch_action_enabled_(GetParam()) {
    filter_.OnHasTouchEventHandlers(true);
    if (compositor_touch_action_enabled_) {
      feature_list_.InitAndEnableFeature(features::kCompositorTouchAction);
      filter_.compositor_touch_action_enabled_ = true;
    } else {
      feature_list_.InitAndDisableFeature(features::kCompositorTouchAction);
      filter_.compositor_touch_action_enabled_ = false;
    }
  }
  ~TouchActionFilterTest() override {}

 protected:
  base::Optional<cc::TouchAction> ActiveTouchAction() const {
    return filter_.active_touch_action_;
  }
  void ResetTouchAction() { filter_.ResetTouchAction(); }
  void ResetActiveTouchAction() { filter_.active_touch_action_.reset(); }
  void ResetWhiteListedTouchAction() {
    filter_.white_listed_touch_action_ = cc::kTouchActionAuto;
  }
  void SetNoDeferredEvents() { filter_.has_deferred_events_ = false; }
  void SetGestureSequenceInProgress() {
    filter_.gesture_sequence_in_progress_ = true;
  }
  void ResetGestureSequenceInProgress() {
    filter_.gesture_sequence_in_progress_ = false;
  }
  void PanTest(cc::TouchAction action,
               float scroll_x,
               float scroll_y,
               float dx,
               float dy,
               float expected_dx,
               float expected_dy) {
    WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGestureTapDown, kSourceDevice);
    WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGestureScrollEnd, kSourceDevice);

    {
      // Scrolls with no direction hint are permitted in the |action| direction.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();

      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(0, 0,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFilterGestureEventAllowed);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(expected_dx, scroll_update.data.scroll_update.delta_x);
      EXPECT_EQ(expected_dy, scroll_update.data.scroll_update.delta_y);

      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls biased towards the touch-action axis are permitted.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_x, scroll_y,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFilterGestureEventAllowed);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(expected_dx, scroll_update.data.scroll_update.delta_x);
      EXPECT_EQ(expected_dy, scroll_update.data.scroll_update.delta_y);

      // Ensure that scrolls in the opposite direction are not filtered once
      // scrolling has started. (Once scrolling is started, the direction may
      // be reversed by the user even if scrolls that start in the reversed
      // direction are disallowed.
      WebGestureEvent scroll_update2 =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(-dx, -dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update2),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(-expected_dx, scroll_update2.data.scroll_update.delta_x);
      EXPECT_EQ(-expected_dy, scroll_update2.data.scroll_update.delta_y);

      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls biased towards the perpendicular of the touch-action axis are
      // suppressed entirely.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_y, scroll_x,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFilterGestureEventFiltered);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                             kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFilterGestureEventFiltered);
      EXPECT_EQ(dx, scroll_update.data.scroll_update.delta_x);
      EXPECT_EQ(dy, scroll_update.data.scroll_update.delta_y);

      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFilterGestureEventFiltered);
      filter_.DecreaseActiveTouches();
    }
  }

  void PanTestForUnidirectionalTouchAction(cc::TouchAction action,
                                           float scroll_x,
                                           float scroll_y) {
    WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGestureTapDown, kSourceDevice);
    WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGestureScrollEnd, kSourceDevice);

    {
      // Scrolls towards the touch-action direction are permitted.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(scroll_x, scroll_y,
                                                            kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFilterGestureEventAllowed);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(scroll_x, scroll_y,
                                                             0, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls towards the exact opposite of the touch-action direction are
      // suppressed entirely.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(
              -scroll_x, -scroll_y, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFilterGestureEventFiltered);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(
              -scroll_x, -scroll_y, 0, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFilterGestureEventFiltered);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFilterGestureEventFiltered);
      filter_.DecreaseActiveTouches();
    }

    {
      // Scrolls towards the diagonal opposite of the touch-action direction are
      // suppressed entirely.
      ResetTouchAction();
      filter_.OnSetTouchAction(action);
      filter_.IncreaseActiveTouches();
      WebGestureEvent scroll_begin =
          SyntheticWebGestureEventBuilder::BuildScrollBegin(
              -scroll_x - scroll_y, -scroll_x - scroll_y, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
                FilterGestureEventResult::kFilterGestureEventAllowed);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
                FilterGestureEventResult::kFilterGestureEventFiltered);

      WebGestureEvent scroll_update =
          SyntheticWebGestureEventBuilder::BuildScrollUpdate(
              -scroll_x - scroll_y, -scroll_x - scroll_y, 0, kSourceDevice);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
                FilterGestureEventResult::kFilterGestureEventFiltered);
      EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
                FilterGestureEventResult::kFilterGestureEventFiltered);
      filter_.DecreaseActiveTouches();
    }
  }
  TouchActionFilter filter_;
  const bool compositor_touch_action_enabled_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(, TouchActionFilterTest, ::testing::Bool());

TEST_P(TouchActionFilterTest, SimpleFilter) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  const float kDeltaX = 5;
  const float kDeltaY = 10;
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDeltaX, kDeltaY, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  // cc::kTouchActionAuto doesn't cause any filtering.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();

  // cc::kTouchActionNone filters out all scroll events, but no other events.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.DecreaseActiveTouches();

  // When a new touch sequence begins, the state is reset.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();

  // Setting touch action doesn't impact any in-progress gestures.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();

  // And the state is still cleared for the next gesture.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();

  // Changing the touch action during a gesture has no effect.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.DecreaseActiveTouches();
}

TEST_P(TouchActionFilterTest, PanLeft) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 7;
  const float kScrollY = 6;

  PanTest(cc::kTouchActionPanLeft, kScrollX, kScrollY, kDX, kDY, kDX, 0);
  PanTestForUnidirectionalTouchAction(cc::kTouchActionPanLeft, kScrollX, 0);
}

TEST_P(TouchActionFilterTest, PanRight) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = -7;
  const float kScrollY = 6;

  PanTest(cc::kTouchActionPanRight, kScrollX, kScrollY, kDX, kDY, kDX, 0);
  PanTestForUnidirectionalTouchAction(cc::kTouchActionPanRight, kScrollX, 0);
}

TEST_P(TouchActionFilterTest, PanX) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 7;
  const float kScrollY = 6;

  PanTest(cc::kTouchActionPanX, kScrollX, kScrollY, kDX, kDY, kDX, 0);
}

TEST_P(TouchActionFilterTest, PanUp) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = 7;

  PanTest(cc::kTouchActionPanUp, kScrollX, kScrollY, kDX, kDY, 0, kDY);
  PanTestForUnidirectionalTouchAction(cc::kTouchActionPanUp, 0, kScrollY);
}

TEST_P(TouchActionFilterTest, PanDown) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = -7;

  PanTest(cc::kTouchActionPanDown, kScrollX, kScrollY, kDX, kDY, 0, kDY);
  PanTestForUnidirectionalTouchAction(cc::kTouchActionPanDown, 0, kScrollY);
}

TEST_P(TouchActionFilterTest, PanY) {
  const float kDX = 5;
  const float kDY = 10;
  const float kScrollX = 6;
  const float kScrollY = 7;

  PanTest(cc::kTouchActionPanY, kScrollX, kScrollY, kDX, kDY, 0, kDY);
}

TEST_P(TouchActionFilterTest, PanXY) {
  const float kDX = 5;
  const float kDY = 10;
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  {
    // Scrolls hinted in the X axis are permitted and unmodified.
    ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionPan);
    filter_.IncreaseActiveTouches();
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-7, 6, kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(kDX, scroll_update.data.scroll_update.delta_x);
    EXPECT_EQ(kDY, scroll_update.data.scroll_update.delta_y);

    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();
  }

  {
    // Scrolls hinted in the Y axis are permitted and unmodified.
    ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionPan);
    filter_.IncreaseActiveTouches();
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-6, 7, kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(kDX, scroll_update.data.scroll_update.delta_x);
    EXPECT_EQ(kDY, scroll_update.data.scroll_update.delta_y);

    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();
  }

  {
    // A two-finger gesture is not allowed.
    ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionPan);
    filter_.IncreaseActiveTouches();
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(-6, 7, kSourceDevice,
                                                          2);
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventFiltered);

    WebGestureEvent scroll_update =
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDX, kDY, 0,
                                                           kSourceDevice);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFilterGestureEventFiltered);

    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    filter_.DecreaseActiveTouches();
  }
}

TEST_P(TouchActionFilterTest, BitMath) {
  // Verify that the simple flag mixing properties we depend on are now
  // trivially true.
  EXPECT_EQ(cc::kTouchActionNone, cc::kTouchActionNone & cc::kTouchActionAuto);
  EXPECT_EQ(cc::kTouchActionNone, cc::kTouchActionPanY & cc::kTouchActionPanX);
  EXPECT_EQ(cc::kTouchActionPan, cc::kTouchActionAuto & cc::kTouchActionPan);
  EXPECT_EQ(cc::kTouchActionManipulation,
            cc::kTouchActionAuto & ~cc::kTouchActionDoubleTapZoom);
  EXPECT_EQ(cc::kTouchActionPanX,
            cc::kTouchActionPanLeft | cc::kTouchActionPanRight);
  EXPECT_EQ(cc::kTouchActionAuto,
            cc::kTouchActionManipulation | cc::kTouchActionDoubleTapZoom);
}

TEST_P(TouchActionFilterTest, MultiTouch) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  const float kDeltaX = 5;
  const float kDeltaY = 10;
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(kDeltaX, kDeltaY, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  // For multiple points, the intersection is what matters.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(kDeltaX, scroll_update.data.scroll_update.delta_x);
  EXPECT_EQ(kDeltaY, scroll_update.data.scroll_update.delta_y);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.DecreaseActiveTouches();

  // Intersection of PAN_X and PAN_Y is NONE.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionPanX);
  filter_.OnSetTouchAction(cc::kTouchActionPanY);
  filter_.OnSetTouchAction(cc::kTouchActionPan);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.DecreaseActiveTouches();
}

class TouchActionFilterPinchTest : public testing::Test,
                                   public testing::WithParamInterface<bool> {
 public:
  TouchActionFilterPinchTest() {
    if (GetParam())
      feature_list_.InitAndEnableFeature(features::kCompositorTouchAction);
    else
      feature_list_.InitAndDisableFeature(features::kCompositorTouchAction);
  }

  void RunTest(bool force_enable_zoom) {
    filter_.OnHasTouchEventHandlers(true);
    filter_.SetForceEnableZoom(force_enable_zoom);

    WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGestureTapDown, kSourceDevice);
    WebGestureEvent scroll_begin =
        SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice,
                                                          2);
    WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGesturePinchBegin, kSourceDevice);
    WebGestureEvent pinch_update =
        SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                          kSourceDevice);
    WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGesturePinchEnd, kSourceDevice);
    WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
        WebInputEvent::kGestureScrollEnd, kSourceDevice);

    // Pinch is allowed with touch-action: auto.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionAuto);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();

    // Pinch is not allowed with touch-action: none.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionNone);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    filter_.DecreaseActiveTouches();

    // Pinch is not allowed with touch-action: pan-x pan-y except for force
    // enable zoom.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionPan);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&scroll_begin),
              force_enable_zoom
                  ? FilterGestureEventResult::kFilterGestureEventFiltered
                  : FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&pinch_begin),
              force_enable_zoom
                  ? FilterGestureEventResult::kFilterGestureEventFiltered
                  : FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&pinch_update),
              force_enable_zoom
                  ? FilterGestureEventResult::kFilterGestureEventFiltered
                  : FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&pinch_end),
              force_enable_zoom
                  ? FilterGestureEventResult::kFilterGestureEventFiltered
                  : FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_NE(filter_.FilterGestureEvent(&scroll_end),
              force_enable_zoom
                  ? FilterGestureEventResult::kFilterGestureEventFiltered
                  : FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();

    // Pinch is allowed with touch-action: manipulation.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionManipulation);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_FALSE(filter_.drop_pinch_events_);
    // The pinch gesture is always re-evaluated on pinch begin.
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();

    // Pinch state is automatically reset at the end of a scroll.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionAuto);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();

    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionAuto);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();

    // Scrolling is allowed when two fingers are down.
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionPinchZoom);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    filter_.DecreaseActiveTouches();

    // At double-tap-drag-zoom case, the pointer_count is 1 at GesturePinchBegin
    // and we need to evaluate whether the gesture is allowed or not at that
    // time.
    scroll_begin.data.scroll_begin.pointer_count = 1;
    filter_.ResetTouchAction();
    filter_.OnSetTouchAction(cc::kTouchActionPinchZoom);
    filter_.IncreaseActiveTouches();
    EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    filter_.DecreaseActiveTouches();
  }

 private:
  TouchActionFilter filter_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(, TouchActionFilterPinchTest, ::testing::Bool());

TEST_P(TouchActionFilterPinchTest, Pinch) {
  RunTest(false);
}

// Enables force enable zoom will override touch-action except for
// touch-action: none.
TEST_P(TouchActionFilterPinchTest, ForceEnableZoom) {
  RunTest(true);
}

TEST_P(TouchActionFilterTest, DoubleTapWithTouchActionAuto) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureDoubleTap, kSourceDevice);

  // Double tap is allowed with touch action auto.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(unconfirmed_tap.GetType(), WebInputEvent::kGestureTapUnconfirmed);
  // The tap cancel will come as part of the next touch sequence.
  ResetTouchAction();
  // Changing the touch action for the second tap doesn't effect the behaviour
  // of the event.
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&double_tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

TEST_P(TouchActionFilterTest, DoubleTap) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureDoubleTap, kSourceDevice);

  // Double tap is disabled with any touch action other than auto.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionManipulation);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(WebInputEvent::kGestureTap, unconfirmed_tap.GetType());
  // Changing the touch action for the second tap doesn't effect the behaviour
  // of the event. The tap cancel will come as part of the next touch sequence.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&double_tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(WebInputEvent::kGestureTap, double_tap.GetType());
  EXPECT_EQ(2, double_tap.data.tap.tap_count);
  filter_.DecreaseActiveTouches();
}

TEST_P(TouchActionFilterTest, SingleTapWithTouchActionAuto) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap1 = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTap, kSourceDevice);

  // Single tap is allowed with touch action auto.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap1),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(WebInputEvent::kGestureTapUnconfirmed, unconfirmed_tap1.GetType());
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

TEST_P(TouchActionFilterTest, SingleTap) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent unconfirmed_tap1 = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapUnconfirmed, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTap, kSourceDevice);

  // With touch action other than auto, tap unconfirmed is turned into tap.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&unconfirmed_tap1),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(WebInputEvent::kGestureTap, unconfirmed_tap1.GetType());
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.DecreaseActiveTouches();
}

TEST_P(TouchActionFilterTest, TouchActionResetsOnResetTouchAction) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTap, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.DecreaseActiveTouches();

  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();

  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();
}

TEST_P(TouchActionFilterTest, TouchActionResetMidSequence) {
  filter_.OnHasTouchEventHandlers(true);
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(2, 3, kSourceDevice);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchEnd, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  filter_.OnSetTouchAction(cc::kTouchActionNone);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);

  // Even though the allowed action is auto after the reset, the remaining
  // scroll and pinch events should be suppressed.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  filter_.DecreaseActiveTouches();

  // A new scroll and pinch sequence should be allowed.
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);

  // Resetting from auto to auto mid-stream should have no effect.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionAuto);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();
}

// This test makes sure that we do not reset scrolling touch action in the
// middle of a gesture sequence.
TEST_P(TouchActionFilterTest, TouchActionNotResetWithinGestureSequence) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
  filter_.OnSetTouchAction(cc::kTouchActionPanY);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(1, 3, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(1, 5, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(cc::kTouchActionPanY, ActiveTouchAction().value());

  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  // Simulate a touch sequence end by calling ReportAndResetTouchAction.
  filter_.DecreaseActiveTouches();
  filter_.ReportAndResetTouchAction();
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
  EXPECT_EQ(cc::kTouchActionPanY, ActiveTouchAction().value());
  // In fling or fling boosting case, we will see ScrollUpdate after the touch
  // end.
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  // The |allowed_touch_action_| should have been reset, but not the
  // |scrolling_touch_action_|.
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
  EXPECT_EQ(cc::kTouchActionPanY, ActiveTouchAction().value());
}

// The following 3 tests ensures that when the IPC message
// OnHasTouchEventHandlers is received in the middle of a gesture sequence, the
// touch action is not reset.
TEST_P(TouchActionFilterTest, OnHasTouchEventHandlersReceivedDuringTap) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_TRUE(ActiveTouchAction().has_value());

  filter_.OnSetTouchAction(cc::kTouchActionPan);
  // Simulate a simple tap gesture.
  WebGestureEvent tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTap, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  // Gesture tap indicates that there is no scroll in progress, so this should
  // reset the |allowed_touch_action_|.
  ResetTouchAction();
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
}

TEST_P(TouchActionFilterTest, OnHasTouchEventHandlersReceivedDuringDoubleTap) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapCancel, kSourceDevice);
  WebGestureEvent double_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureDoubleTap, kSourceDevice);

  // Simulate a double tap gesture: GTD-->GTC-->GTD-->GTC-->GDT.
  filter_.IncreaseActiveTouches();
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionAuto);
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_TRUE(ActiveTouchAction().has_value());
  filter_.OnSetTouchAction(cc::kTouchActionPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&double_tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.DecreaseActiveTouches();
}

TEST_P(TouchActionFilterTest, OnHasTouchEventHandlersReceivedDuringScroll) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapCancel, kSourceDevice);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(5, 0, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(5, 0, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  // Simulate a gesture scroll: GTD-->GTC-->GSB-->GSU-->GSE.
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  filter_.OnHasTouchEventHandlers(true);
  filter_.OnSetTouchAction(cc::kTouchActionPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

// If OnHasTouchEventHandlers IPC is received after LongTap or TwoFingerTap,
// the touch action should be reset.
TEST_P(TouchActionFilterTest,
       OnHasTouchEventHandlersReceivedAfterLongTapOrTwoFingerTap) {
  filter_.OnHasTouchEventHandlers(false);

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  WebGestureEvent tap_cancel = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapCancel, kSourceDevice);
  WebGestureEvent long_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureLongTap, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&long_tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionAuto);

  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());

  filter_.OnHasTouchEventHandlers(false);
  WebGestureEvent two_finger_tap = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTwoFingerTap, kSourceDevice);

  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_cancel),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&two_finger_tap),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionAuto);

  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
}

TEST_P(TouchActionFilterTest, OnHasTouchEventHandlersReceivedAfterTouchStart) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  // Receive a touch start ack, set the touch action.
  filter_.OnSetTouchAction(cc::kTouchActionPanY);
  filter_.IncreaseActiveTouches();
  filter_.OnHasTouchEventHandlers(false);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionPanY);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::kTouchActionPanY);
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionPanY);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::kTouchActionPanY);
}

TEST_P(TouchActionFilterTest, ResetTouchActionWithActiveTouch) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  // Receive a touch start ack, set the touch action.
  filter_.OnSetTouchAction(cc::kTouchActionPanY);
  filter_.IncreaseActiveTouches();

  // Somehow we get the ACK for the second touch start before the ACK for the
  // first touch end.
  filter_.OnSetTouchAction(cc::kTouchActionPan);
  filter_.IncreaseActiveTouches();

  // The first touch end comes, we report and reset touch action. The touch
  // actions should still have value.
  filter_.DecreaseActiveTouches();
  filter_.ReportAndResetTouchAction();
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionPanY);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::kTouchActionPanY);

  // The ack for the second touch end comes, the touch actions will be reset.
  filter_.DecreaseActiveTouches();
  filter_.ReportAndResetTouchAction();
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());
}

// If the renderer is busy, the gesture event might have come before the
// OnHasTouchEventHanlders IPC is received. In this case, we should allow all
// the gestures.
TEST_P(TouchActionFilterTest, GestureArrivesBeforeHasHandlerSet) {
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

TEST_P(TouchActionFilterTest, PinchGesturesAllowedByWhiteListedTouchAction) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  // white listed touch action has a default value of Auto, and pinch related
  // gestures should be allowed.
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchEnd, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

// Test gesture event filtering with white listed touch action. It should test
// all 3 kinds of results: Allowed / Dropped / Delayed.
TEST_P(TouchActionFilterTest, FilterWithWhiteListedTouchAction) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  int dx = 2, dy = 5;
  // Test gestures that are allowed.
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPan);
  if (!compositor_touch_action_enabled_)
    filter_.OnSetTouchAction(cc::kTouchActionPan);
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPan);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);

  // Pinch related gestures are always delayed.
  ResetTouchAction();
  ResetActiveTouchAction();
  ResetWhiteListedTouchAction();
  filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPan);
  if (!compositor_touch_action_enabled_)
    filter_.OnSetTouchAction(cc::kTouchActionPan);
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPan);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchEnd, kSourceDevice);
  if (compositor_touch_action_enabled_) {
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventDelayed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventDelayed);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventDelayed);
  } else {
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
              FilterGestureEventResult::kFilterGestureEventFiltered);
  }

  // Scroll updates should be delayed if white listed touch action is PanY,
  // because there are delta along the direction that is not allowed.
  ResetTouchAction();
  ResetActiveTouchAction();
  ResetWhiteListedTouchAction();
  filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPanY);
  if (!compositor_touch_action_enabled_)
    filter_.OnSetTouchAction(cc::kTouchActionPanY);
  SetNoDeferredEvents();
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPanY);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  if (compositor_touch_action_enabled_) {
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFilterGestureEventDelayed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventDelayed);
  } else {
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFilterGestureEventAllowed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventAllowed);
  }

  ResetTouchAction();
  ResetActiveTouchAction();
  ResetWhiteListedTouchAction();
  filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPanX);
  if (!compositor_touch_action_enabled_)
    filter_.OnSetTouchAction(cc::kTouchActionPanX);
  SetNoDeferredEvents();
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPanX);

  dy = 0;
  scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      dx, dy, 0, kSourceDevice);
  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);

  dx = 0;
  dy = 5;
  scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  scroll_update = SyntheticWebGestureEventBuilder::BuildScrollUpdate(
      dx, dy, 0, kSourceDevice);
  filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPanX);
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPanX);
  SetGestureSequenceInProgress();
  if (compositor_touch_action_enabled_) {
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventDelayed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFilterGestureEventDelayed);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventDelayed);
  } else {
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
              FilterGestureEventResult::kFilterGestureEventFiltered);
    EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
              FilterGestureEventResult::kFilterGestureEventFiltered);
  }
}

TEST_P(TouchActionFilterTest, WhiteListedTouchActionResetToAuto) {
  filter_.OnHasTouchEventHandlers(true);

  filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPan);
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPan);
  ResetTouchAction();
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionAuto);
}

TEST_P(TouchActionFilterTest, WhiteListedTouchActionAutoNoHasHandlers) {
  filter_.OnHasTouchEventHandlers(false);
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionAuto);

  ResetTouchAction();
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionAuto);
}

TEST_P(TouchActionFilterTest, ResetBeforeHasHandlerSet) {
  // This should not crash, and should set touch action to auto.
  ResetTouchAction();
  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

TEST_P(TouchActionFilterTest,
       WhiteListedTouchActionNotResetAtGestureScrollEnd) {
  if (!compositor_touch_action_enabled_)
    return;
  filter_.OnHasTouchEventHandlers(true);

  filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPan);
  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPan);

  int dx = 2, dy = 5;
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(dx, dy, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(dx, dy, 0,
                                                         kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);

  SetGestureSequenceInProgress();
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);

  EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPan);
}

// Having a gesture scroll begin without tap down should set touch action to
// Auto.
TEST_P(TouchActionFilterTest, ScrollBeginWithoutTapDown) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  if (compositor_touch_action_enabled_)
    filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPan);
  else
    filter_.OnSetTouchAction(cc::kTouchActionPan);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(5, 0, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  if (compositor_touch_action_enabled_) {
    EXPECT_EQ(filter_.white_listed_touch_action(), cc::kTouchActionPan);
  } else {
    EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionPan);
    EXPECT_EQ(filter_.allowed_touch_action().value(), cc::kTouchActionPan);
  }

  ResetTouchAction();
  ResetActiveTouchAction();
  ResetGestureSequenceInProgress();
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  // Ensure that there is no crash at GSB if both |allowed_| and |active_|
  // touch action have no value.
  if (compositor_touch_action_enabled_)
    filter_.OnSetWhiteListedTouchAction(cc::kTouchActionPan);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  if (!compositor_touch_action_enabled_) {
    EXPECT_EQ(filter_.allowed_touch_action().value(), cc::kTouchActionAuto);
    EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionAuto);
  }
}

// This tests a gesture tap down with |num_of_active_touches_| == 0
TEST_P(TouchActionFilterTest, TapDownWithZeroNumOfActiveTouches) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  WebGestureEvent tap_down = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureTapDown, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&tap_down),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_TRUE(ActiveTouchAction().has_value());
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionAuto);
}

// Regression test for crbug.com/771330. One can start one finger panning y, and
// add another finger to pinch zooming. The pinch zooming should not be allowed
// if the allowed touch action doesn't allow it.
TEST_P(TouchActionFilterTest, PinchZoomStartsWithOneFingerPanDisallowed) {
  filter_.OnHasTouchEventHandlers(true);
  filter_.OnSetTouchAction(cc::kTouchActionPanY);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(0, 3, kSourceDevice);
  WebGestureEvent scroll_update =
      SyntheticWebGestureEventBuilder::BuildScrollUpdate(5, 10, 0,
                                                         kSourceDevice);
  WebGestureEvent pinch_begin = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchBegin, kSourceDevice);
  WebGestureEvent pinch_update =
      SyntheticWebGestureEventBuilder::BuildPinchUpdate(1.2f, 5, 5, 0,
                                                        kSourceDevice);
  WebGestureEvent pinch_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGesturePinchEnd, kSourceDevice);
  WebGestureEvent scroll_end = SyntheticWebGestureEventBuilder::Build(
      WebInputEvent::kGestureScrollEnd, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_update),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_begin),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_update),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&pinch_end),
            FilterGestureEventResult::kFilterGestureEventFiltered);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_end),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

TEST_P(TouchActionFilterTest, ScrollBeginWithoutTapDownWithKnownTouchAction) {
  filter_.OnHasTouchEventHandlers(true);
  EXPECT_FALSE(ActiveTouchAction().has_value());
  EXPECT_FALSE(filter_.allowed_touch_action().has_value());

  filter_.OnSetTouchAction(cc::kTouchActionPan);
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(5, 0, kSourceDevice);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
  EXPECT_EQ(ActiveTouchAction().value(), cc::kTouchActionPan);
  EXPECT_EQ(filter_.allowed_touch_action().value(), cc::kTouchActionPan);
}

TEST_P(TouchActionFilterTest, TouchpadScroll) {
  WebGestureEvent scroll_begin =
      SyntheticWebGestureEventBuilder::BuildScrollBegin(
          2, 3, blink::WebGestureDevice::kTouchpad);

  // cc::kTouchActionNone filters out only touchscreen scroll events.
  ResetTouchAction();
  filter_.OnSetTouchAction(cc::kTouchActionNone);
  EXPECT_EQ(filter_.FilterGestureEvent(&scroll_begin),
            FilterGestureEventResult::kFilterGestureEventAllowed);
}

}  // namespace content
