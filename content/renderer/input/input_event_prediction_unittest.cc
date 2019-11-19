// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_event_prediction.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace content {

namespace {
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;
using ui::input_prediction::PredictorType;
}  // namespace

class InputEventPredictionTest : public testing::Test {
 public:
  InputEventPredictionTest() {
    // Default to enable resampling with empty predictor for testing.
    ConfigureFieldTrialAndInitialize(
        features::kResamplingInputEvents,
        ui::input_prediction::kScrollPredictorNameEmpty);
  }

  int GetPredictorMapSize() const {
    return event_predictor_->pointer_id_predictor_map_.size();
  }

  std::unique_ptr<ui::InputPredictor::InputData> GetPrediction(
      const WebPointerProperties& event) const {
    if (event.pointer_type == WebPointerProperties::PointerType::kMouse) {
      return event_predictor_->mouse_predictor_->GeneratePrediction(
          ui::EventTimeForNow());
    } else {
      auto predictor =
          event_predictor_->pointer_id_predictor_map_.find(event.id);
      if (predictor != event_predictor_->pointer_id_predictor_map_.end())
        return predictor->second->GeneratePrediction(ui::EventTimeForNow());
    }
    return nullptr;
  }

  void HandleEvents(const WebInputEvent& event) {
    blink::WebCoalescedInputEvent coalesced_event(event);
    event_predictor_->HandleEvents(coalesced_event, ui::EventTimeForNow());
  }

  void ConfigureFieldTrial(const base::Feature& feature,
                           const std::string& predictor_type) {
    base::FieldTrialParams params;
    params["predictor"] = predictor_type;
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(feature, params);

    EXPECT_EQ(params["predictor"],
              GetFieldTrialParamValueByFeature(feature, "predictor"));
  }

  void ConfigureFieldTrialAndInitialize(const base::Feature& feature,
                                        const std::string& predictor_type) {
    ConfigureFieldTrial(feature, predictor_type);
    event_predictor_ = std::make_unique<InputEventPrediction>(
        base::FeatureList::IsEnabled(features::kResamplingInputEvents));
  }

 protected:
  std::unique_ptr<InputEventPrediction> event_predictor_;

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(InputEventPredictionTest);
};

TEST_F(InputEventPredictionTest, PredictorType) {
  // Resampling is default to true for InputEventPredictionTest.
  EXPECT_TRUE(event_predictor_->enable_resampling_);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeEmpty);

  ConfigureFieldTrialAndInitialize(
      features::kResamplingInputEvents,
      ui::input_prediction::kScrollPredictorNameEmpty);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeEmpty);

  ConfigureFieldTrialAndInitialize(
      features::kResamplingInputEvents,
      ui::input_prediction::kScrollPredictorNameKalman);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeKalman);

  ConfigureFieldTrialAndInitialize(
      features::kResamplingInputEvents,
      ui::input_prediction::kScrollPredictorNameKalman);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeKalman);

  ConfigureFieldTrialAndInitialize(
      features::kResamplingInputEvents,
      ui::input_prediction::kScrollPredictorNameLsq);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeLsq);

  // Default to Kalman predictor.
  ConfigureFieldTrialAndInitialize(features::kResamplingInputEvents, "");
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeKalman);

  ConfigureFieldTrialAndInitialize(
      features::kInputPredictorTypeChoice,
      ui::input_prediction::kScrollPredictorNameLsq);
  EXPECT_FALSE(event_predictor_->enable_resampling_);
  // When enable_resampling_ is true, kInputPredictorTypeChoice flag has no
  // effect.
  event_predictor_ = std::make_unique<InputEventPrediction>(true);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeKalman);
}

TEST_F(InputEventPredictionTest, MouseEvent) {
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);

  EXPECT_FALSE(GetPrediction(mouse_move));

  HandleEvents(mouse_move);
  EXPECT_EQ(GetPredictorMapSize(), 0);
  auto predicted_point = GetPrediction(mouse_move);
  EXPECT_TRUE(predicted_point);
  EXPECT_EQ(predicted_point->pos.x(), 10);
  EXPECT_EQ(predicted_point->pos.y(), 10);

  WebMouseEvent mouse_down = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseDown, 10, 10, 0);

  HandleEvents(mouse_down);
  EXPECT_FALSE(GetPrediction(mouse_down));
}

TEST_F(InputEventPredictionTest, SingleTouchPoint) {
  SyntheticWebTouchEvent touch_event;

  touch_event.PressPoint(10, 10);
  touch_event.touches[0].pointer_type =
      WebPointerProperties::PointerType::kTouch;

  HandleEvents(touch_event);
  EXPECT_FALSE(GetPrediction(touch_event.touches[0]));

  touch_event.MovePoint(0, 11, 12);
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 1);
  auto predicted_point = GetPrediction(touch_event.touches[0]);
  EXPECT_TRUE(predicted_point);
  EXPECT_EQ(predicted_point->pos.x(), 11);
  EXPECT_EQ(predicted_point->pos.y(), 12);

  touch_event.ReleasePoint(0);
  HandleEvents(touch_event);
  EXPECT_FALSE(GetPrediction(touch_event.touches[0]));
}

TEST_F(InputEventPredictionTest, MouseEventTypePen) {
  WebMouseEvent pen_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0,
      WebPointerProperties::PointerType::kPen);

  EXPECT_FALSE(GetPrediction(pen_move));
  HandleEvents(pen_move);
  EXPECT_EQ(GetPredictorMapSize(), 1);
  auto predicted_point = GetPrediction(pen_move);
  EXPECT_TRUE(predicted_point);
  EXPECT_EQ(predicted_point->pos.x(), 10);
  EXPECT_EQ(predicted_point->pos.y(), 10);

  WebMouseEvent pen_leave = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseLeave, 10, 10, 0,
      WebPointerProperties::PointerType::kPen);

  HandleEvents(pen_leave);
  EXPECT_EQ(GetPredictorMapSize(), 0);
  EXPECT_FALSE(GetPrediction(pen_leave));
}

TEST_F(InputEventPredictionTest, MultipleTouchPoint) {
  SyntheticWebTouchEvent touch_event;

  // Press and move 1st touch point
  touch_event.PressPoint(10, 10);
  touch_event.MovePoint(0, 11, 12);
  touch_event.touches[0].pointer_type =
      WebPointerProperties::PointerType::kTouch;

  HandleEvents(touch_event);

  // Press 2nd touch point
  touch_event.PressPoint(20, 30);
  touch_event.touches[1].pointer_type = WebPointerProperties::PointerType::kPen;
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 1);

  // Move 2nd touch point
  touch_event.MovePoint(1, 25, 25);
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 2);

  auto predicted_point = GetPrediction(touch_event.touches[0]);
  EXPECT_TRUE(predicted_point);
  EXPECT_EQ(predicted_point->pos.x(), 11);
  EXPECT_EQ(predicted_point->pos.y(), 12);

  predicted_point = GetPrediction(touch_event.touches[1]);
  EXPECT_TRUE(predicted_point);
  EXPECT_EQ(predicted_point->pos.x(), 25);
  EXPECT_EQ(predicted_point->pos.y(), 25);

  touch_event.ReleasePoint(0);
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 1);
}

TEST_F(InputEventPredictionTest, TouchAndStylusResetMousePredictor) {
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);

  HandleEvents(mouse_move);
  auto predicted_point = GetPrediction(mouse_move);
  EXPECT_TRUE(predicted_point);

  WebMouseEvent pen_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 20, 20, 0,
      WebPointerProperties::PointerType::kPen);
  pen_move.id = 1;

  HandleEvents(pen_move);
  predicted_point = GetPrediction(pen_move);
  EXPECT_TRUE(predicted_point);
  EXPECT_FALSE(GetPrediction(mouse_move));

  HandleEvents(mouse_move);
  predicted_point = GetPrediction(mouse_move);
  EXPECT_TRUE(predicted_point);

  SyntheticWebTouchEvent touch_event;
  touch_event.PressPoint(10, 10);
  touch_event.touches[0].pointer_type =
      WebPointerProperties::PointerType::kTouch;

  HandleEvents(touch_event);
  touch_event.MovePoint(0, 10, 10);
  HandleEvents(touch_event);
  predicted_point = GetPrediction(touch_event.touches[0]);
  EXPECT_TRUE(predicted_point);
  EXPECT_FALSE(GetPrediction(mouse_move));
}

// TouchScrollStarted event removes all touch points.
TEST_F(InputEventPredictionTest, TouchScrollStartedRemoveAllTouchPoints) {
  SyntheticWebTouchEvent touch_event;

  // Press 1st & 2nd touch point
  touch_event.PressPoint(10, 10);
  touch_event.touches[0].pointer_type =
      WebPointerProperties::PointerType::kTouch;
  touch_event.PressPoint(20, 20);
  touch_event.touches[1].pointer_type =
      WebPointerProperties::PointerType::kTouch;
  HandleEvents(touch_event);

  // Move 1st & 2nd touch point
  touch_event.MovePoint(0, 15, 18);
  touch_event.MovePoint(1, 25, 27);
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 2);

  touch_event.SetType(WebInputEvent::kTouchScrollStarted);
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 0);
}

TEST_F(InputEventPredictionTest, ResamplingDisabled) {
  // When resampling is disabled, default to use kalman filter.
  ConfigureFieldTrialAndInitialize(features::kInputPredictorTypeChoice, "");
  EXPECT_FALSE(event_predictor_->enable_resampling_);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            PredictorType::kScrollPredictorTypeKalman);

  // Send 3 mouse move to get kalman predictor ready.
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);

  HandleEvents(mouse_move);
  mouse_move =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove, 11, 9, 0);
  HandleEvents(mouse_move);

  mouse_move =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove, 12, 8, 0);
  HandleEvents(mouse_move);

  // The 4th move event should generate predicted events.
  mouse_move =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove, 13, 7, 0);
  blink::WebCoalescedInputEvent coalesced_event(mouse_move);
  event_predictor_->HandleEvents(coalesced_event, ui::EventTimeForNow());

  EXPECT_GT(coalesced_event.PredictedEventSize(), 0u);

  // Verify when resampling event is disabled, original event coordinates don't
  // change.
  const WebMouseEvent& event =
      static_cast<const blink::WebMouseEvent&>(coalesced_event.Event());
  EXPECT_EQ(event.PositionInWidget().x, 13);
  EXPECT_EQ(event.PositionInWidget().y, 7);
}

// Test that when dt > maxResampling, resampling is cut off .
TEST_F(InputEventPredictionTest, NoResampleWhenExceedMaxResampleTime) {
  ConfigureFieldTrialAndInitialize(
      features::kResamplingInputEvents,
      ui::input_prediction::kScrollPredictorNameKalman);

  base::TimeDelta predictor_max_resample_time =
      event_predictor_->mouse_predictor_->MaxResampleTime();

  base::TimeTicks event_time = ui::EventTimeForNow();
  // Send 3 mouse move each has 8ms interval to get kalman predictor ready.
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);
  mouse_move.SetTimeStamp(event_time);
  HandleEvents(mouse_move);
  mouse_move =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove, 11, 9, 0);
  mouse_move.SetTimeStamp(event_time += base::TimeDelta::FromMilliseconds(8));
  HandleEvents(mouse_move);
  mouse_move =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove, 12, 8, 0);
  mouse_move.SetTimeStamp(event_time += base::TimeDelta::FromMilliseconds(8));
  HandleEvents(mouse_move);

  {
    // When frame_time is 8ms away from the last event, we have both resampling
    // and 3 predicted events.
    mouse_move = SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove,
                                                      13, 7, 0);
    mouse_move.SetTimeStamp(event_time += base::TimeDelta::FromMilliseconds(8));
    blink::WebCoalescedInputEvent coalesced_event(mouse_move);
    base::TimeTicks frame_time =
        event_time + predictor_max_resample_time;  // No cut off
    event_predictor_->HandleEvents(coalesced_event, frame_time);

    const WebMouseEvent& event =
        static_cast<const blink::WebMouseEvent&>(coalesced_event.Event());
    EXPECT_GT(event.PositionInWidget().x, 13);
    EXPECT_LT(event.PositionInWidget().y, 7);
    EXPECT_EQ(event.TimeStamp(), frame_time);

    EXPECT_EQ(coalesced_event.PredictedEventSize(), 3u);
    // First predicted event time stamp is 8ms from original event timestamp.
    EXPECT_EQ(coalesced_event.PredictedEvent(0).TimeStamp(),
              event_time + base::TimeDelta::FromMilliseconds(8));
  }

  {
    // Test When the delta time between the frame time and the event is greater
    // than the maximum resampling time for a predictor, the resampling is cut
    // off to the maximum allowed by the predictor
    mouse_move = SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove,
                                                      14, 6, 0);
    mouse_move.SetTimeStamp(event_time += base::TimeDelta::FromMilliseconds(8));
    blink::WebCoalescedInputEvent coalesced_event(mouse_move);
    base::TimeTicks frame_time =
        event_time + predictor_max_resample_time +
        base::TimeDelta::FromMilliseconds(10);  // overpredict on purpose
    event_predictor_->HandleEvents(coalesced_event, frame_time);

    // We expect the prediction to be cut off to the max resampling time of
    // the predictor
    const WebMouseEvent& event =
        static_cast<const blink::WebMouseEvent&>(coalesced_event.Event());
    EXPECT_GT(event.PositionInWidget().x, 14);
    EXPECT_LT(event.PositionInWidget().y, 6);
    EXPECT_EQ(event.TimeStamp(), event_time + predictor_max_resample_time);

    EXPECT_EQ(coalesced_event.PredictedEventSize(), 3u);
    // First predicted event time stamp is 8ms from original event timestamp.
    EXPECT_EQ(coalesced_event.PredictedEvent(0).TimeStamp(),
              event_time + base::TimeDelta::FromMilliseconds(8));
  }
}

// Test that when dt between events is 6ms, first predicted point is 6ms ahead.
TEST_F(InputEventPredictionTest, PredictedEventsTimeIntervalEqualRealEvents) {
  ConfigureFieldTrialAndInitialize(
      features::kResamplingInputEvents,
      ui::input_prediction::kScrollPredictorNameKalman);

  base::TimeTicks event_time = ui::EventTimeForNow();
  // Send 3 mouse move each has 6ms interval to get kalman predictor ready.
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);
  mouse_move.SetTimeStamp(event_time);
  HandleEvents(mouse_move);
  mouse_move =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove, 11, 9, 0);
  mouse_move.SetTimeStamp(event_time += base::TimeDelta::FromMilliseconds(6));
  HandleEvents(mouse_move);
  mouse_move =
      SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove, 12, 8, 0);
  mouse_move.SetTimeStamp(event_time += base::TimeDelta::FromMilliseconds(6));
  HandleEvents(mouse_move);

  {
    mouse_move = SyntheticWebMouseEventBuilder::Build(WebInputEvent::kMouseMove,
                                                      13, 7, 0);
    mouse_move.SetTimeStamp(event_time += base::TimeDelta::FromMilliseconds(6));
    blink::WebCoalescedInputEvent coalesced_event(mouse_move);
    event_predictor_->HandleEvents(coalesced_event, event_time);

    EXPECT_EQ(coalesced_event.PredictedEventSize(), 4u);
    // First predicted event time stamp is 6ms from original event timestamp.
    EXPECT_EQ(coalesced_event.PredictedEvent(0).TimeStamp(),
              event_time + base::TimeDelta::FromMilliseconds(6));
  }
}

// Test that touch points other than kStateMove will not have predicted events.
TEST_F(InputEventPredictionTest, TouchPointStates) {
  SyntheticWebTouchEvent touch_event;
  touch_event.PressPoint(10, 10);
  HandleEvents(touch_event);
  // Send 3 moves to initialize predictor.
  for (int i = 0; i < 3; i++) {
    touch_event.MovePoint(0, 10, 10);
    HandleEvents(touch_event);
  }

  for (int state = blink::WebTouchPoint::kStateUndefined;
       state <= blink::WebTouchPoint::kStateMax; state++) {
    touch_event.touches[0].state =
        static_cast<blink::WebTouchPoint::State>(state);
    blink::WebCoalescedInputEvent coalesced_event(touch_event);
    event_predictor_->HandleEvents(coalesced_event, ui::EventTimeForNow());
    if (state == blink::WebTouchPoint::kStateMoved)
      EXPECT_GT(coalesced_event.PredictedEventSize(), 0u);
    else
      EXPECT_EQ(coalesced_event.PredictedEventSize(), 0u);
  }
}

}  // namespace content
