// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/input/input_event_prediction.h"

#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/prediction/empty_predictor.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebPointerProperties;
using blink::WebTouchEvent;

namespace content {

class InputEventPredictionTest : public testing::Test {
 public:
  InputEventPredictionTest() {
    event_predictor_ =
        std::make_unique<InputEventPrediction>(true /* enable_resampling */);
  }

  int GetPredictorMapSize() const {
    return event_predictor_->pointer_id_predictor_map_.size();
  }

  bool GetPrediction(const WebPointerProperties& event,
                     ui::InputPredictor::InputData* result) const {
    if (!event_predictor_)
      return false;

    if (event.pointer_type == WebPointerProperties::PointerType::kMouse) {
      return event_predictor_->mouse_predictor_->GeneratePrediction(
          WebInputEvent::GetStaticTimeStampForTests(), result);
    } else {
      auto predictor =
          event_predictor_->pointer_id_predictor_map_.find(event.id);

      if (predictor != event_predictor_->pointer_id_predictor_map_.end())
        return predictor->second->GeneratePrediction(
            WebInputEvent::GetStaticTimeStampForTests(), result);
      else
        return false;
    }
  }

  void HandleEvents(const WebInputEvent& event) {
    blink::WebCoalescedInputEvent coalesced_event(event);
    event_predictor_->HandleEvents(coalesced_event,
                                   WebInputEvent::GetStaticTimeStampForTests());
  }

  void ConfigureFieldTrial(const base::Feature& feature,
                           const std::string& predictor_type) {
    const std::string kTrialName = "TestTrial";
    const std::string kGroupName = "TestGroup";

    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(nullptr));
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();

    std::map<std::string, std::string> params;
    params["predictor"] = predictor_type;
    base::FieldTrialParamAssociator::GetInstance()->AssociateFieldTrialParams(
        kTrialName, kGroupName, params);

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(
        feature.name, base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial.get());
    base::FeatureList::ClearInstanceForTesting();
    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));

    EXPECT_EQ(params["predictor"],
              GetFieldTrialParamValueByFeature(feature, "predictor"));
  }

 protected:
  std::unique_ptr<InputEventPrediction> event_predictor_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(InputEventPredictionTest);
};

TEST_F(InputEventPredictionTest, PredictorType) {
  // resampling is default to true for InputEventPredictionTest.
  EXPECT_TRUE(event_predictor_->enable_resampling_);
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            InputEventPrediction::PredictorType::kEmpty);

  ConfigureFieldTrial(features::kResamplingInputEvents, "empty");
  event_predictor_->SetUpPredictorType();
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            InputEventPrediction::PredictorType::kEmpty);

  ConfigureFieldTrial(features::kResamplingInputEvents, "kalman");
  event_predictor_->SetUpPredictorType();
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            InputEventPrediction::PredictorType::kKalman);

  ConfigureFieldTrial(features::kResamplingInputEvents, "lsq");
  event_predictor_->SetUpPredictorType();
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            InputEventPrediction::PredictorType::kLsq);

  // When enable_resampling_ is true, kInputPredictorTypeChoice flag have no
  // effect.
  ConfigureFieldTrial(features::kInputPredictorTypeChoice, "lsq");
  event_predictor_->SetUpPredictorType();
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            InputEventPrediction::PredictorType::kEmpty);
}

TEST_F(InputEventPredictionTest, MouseEvent) {
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);

  ui::InputPredictor::InputData last_point;
  EXPECT_FALSE(GetPrediction(mouse_move, &last_point));

  HandleEvents(mouse_move);
  EXPECT_EQ(GetPredictorMapSize(), 0);
  EXPECT_TRUE(GetPrediction(mouse_move, &last_point));
  EXPECT_EQ(last_point.pos.x(), 10);
  EXPECT_EQ(last_point.pos.y(), 10);

  WebMouseEvent mouse_down = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseDown, 10, 10, 0);
  HandleEvents(mouse_down);
  EXPECT_FALSE(GetPrediction(mouse_down, &last_point));
}

TEST_F(InputEventPredictionTest, SingleTouchPoint) {
  SyntheticWebTouchEvent touch_event;

  ui::InputPredictor::InputData last_point;

  touch_event.PressPoint(10, 10);
  touch_event.touches[0].pointer_type =
      WebPointerProperties::PointerType::kTouch;
  HandleEvents(touch_event);
  EXPECT_FALSE(GetPrediction(touch_event.touches[0], &last_point));

  touch_event.MovePoint(0, 11, 12);
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 1);
  EXPECT_TRUE(GetPrediction(touch_event.touches[0], &last_point));
  EXPECT_EQ(last_point.pos.x(), 11);
  EXPECT_EQ(last_point.pos.y(), 12);

  touch_event.ReleasePoint(0);
  HandleEvents(touch_event);
  EXPECT_FALSE(GetPrediction(touch_event.touches[0], &last_point));
}

TEST_F(InputEventPredictionTest, MouseEventTypePen) {
  WebMouseEvent pen_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0,
      WebPointerProperties::PointerType::kPen);

  ui::InputPredictor::InputData last_point;
  EXPECT_FALSE(GetPrediction(pen_move, &last_point));
  HandleEvents(pen_move);
  EXPECT_EQ(GetPredictorMapSize(), 1);
  EXPECT_TRUE(GetPrediction(pen_move, &last_point));
  EXPECT_EQ(last_point.pos.x(), 10);
  EXPECT_EQ(last_point.pos.y(), 10);

  WebMouseEvent pen_leave = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseLeave, 10, 10, 0,
      WebPointerProperties::PointerType::kPen);
  HandleEvents(pen_leave);
  EXPECT_EQ(GetPredictorMapSize(), 0);
  EXPECT_FALSE(GetPrediction(pen_leave, &last_point));
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

  ui::InputPredictor::InputData last_point;
  EXPECT_TRUE(GetPrediction(touch_event.touches[0], &last_point));
  EXPECT_EQ(last_point.pos.x(), 11);
  EXPECT_EQ(last_point.pos.y(), 12);

  EXPECT_TRUE(GetPrediction(touch_event.touches[1], &last_point));
  EXPECT_EQ(last_point.pos.x(), 25);
  EXPECT_EQ(last_point.pos.y(), 25);

  touch_event.ReleasePoint(0);
  HandleEvents(touch_event);
  EXPECT_EQ(GetPredictorMapSize(), 1);
}

TEST_F(InputEventPredictionTest, TouchAndStylusResetMousePredictor) {
  WebMouseEvent mouse_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 10, 10, 0);
  HandleEvents(mouse_move);
  ui::InputPredictor::InputData last_point;
  EXPECT_TRUE(GetPrediction(mouse_move, &last_point));

  WebMouseEvent pen_move = SyntheticWebMouseEventBuilder::Build(
      WebInputEvent::kMouseMove, 20, 20, 0,
      WebPointerProperties::PointerType::kPen);
  pen_move.id = 1;
  HandleEvents(pen_move);
  EXPECT_TRUE(GetPrediction(pen_move, &last_point));
  EXPECT_FALSE(GetPrediction(mouse_move, &last_point));

  HandleEvents(mouse_move);
  EXPECT_TRUE(GetPrediction(mouse_move, &last_point));

  SyntheticWebTouchEvent touch_event;
  touch_event.PressPoint(10, 10);
  touch_event.touches[0].pointer_type =
      WebPointerProperties::PointerType::kTouch;
  HandleEvents(touch_event);
  touch_event.MovePoint(0, 10, 10);
  HandleEvents(touch_event);
  EXPECT_TRUE(GetPrediction(touch_event.touches[0], &last_point));
  EXPECT_FALSE(GetPrediction(mouse_move, &last_point));
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
  // When resampling is disabled, set predictor type by
  // kInputPredictorTypeChoice.
  event_predictor_->enable_resampling_ = false;
  ConfigureFieldTrial(features::kInputPredictorTypeChoice, "kalman");
  event_predictor_->SetUpPredictorType();
  EXPECT_EQ(event_predictor_->selected_predictor_type_,
            InputEventPrediction::PredictorType::kKalman);

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

  // Verify when resampling event is disabled, event coordinate doesn't change.
  const WebMouseEvent& event =
      static_cast<const blink::WebMouseEvent&>(coalesced_event.Event());
  EXPECT_EQ(event.PositionInWidget().x, 13);
  EXPECT_EQ(event.PositionInWidget().y, 7);
}

}  // namespace content
