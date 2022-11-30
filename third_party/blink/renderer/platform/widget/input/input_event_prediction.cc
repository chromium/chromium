// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/input_event_prediction.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

const WebPointerProperties* ToWebPointerProperties(const WebInputEvent* event) {
  if (WebInputEvent::IsMouseEventType(event->GetType()))
    return static_cast<const WebMouseEvent*>(event);
  if (WebInputEvent::IsPointerEventType(event->GetType()))
    return static_cast<const WebPointerEvent*>(event);
  return nullptr;
}
WebPointerProperties* ToWebPointerProperties(WebInputEvent* event) {
  if (WebInputEvent::IsMouseEventType(event->GetType()))
    return static_cast<WebMouseEvent*>(event);
  if (WebInputEvent::IsPointerEventType(event->GetType()))
    return static_cast<WebPointerEvent*>(event);
  return nullptr;
}

}  // namespace

InputEventPrediction::InputEventPrediction(bool enable_resampling)
    : enable_resampling_(enable_resampling) {
  // When resampling is enabled, set predictor type by resampling flag params;
  // otherwise, get predictor type parameters from kInputPredictorTypeChoice
  // flag.
  std::string predictor_name =
      enable_resampling_
          ? GetFieldTrialParamValueByFeature(
                blink::features::kResamplingInputEvents, "predictor")
          : GetFieldTrialParamValueByFeature(
                blink::features::kInputPredictorTypeChoice, "predictor");

  if (predictor_name.empty()) {
    selected_predictor_type_ =
        input_prediction::PredictorType::kScrollPredictorTypeKalman;
  } else {
    selected_predictor_type_ =
        PredictorFactory::GetPredictorTypeFromName(predictor_name);
  }

  mouse_predictor_ = CreatePredictor();
}

InputEventPrediction::~InputEventPrediction() {}

void InputEventPrediction::HandleEvents(
    blink::WebCoalescedInputEvent& coalesced_event,
    base::TimeTicks frame_time) {
  switch (coalesced_event.Event().GetType()) {
    case WebInputEvent::Type::kMouseMove:
    case WebInputEvent::Type::kTouchMove:
    case WebInputEvent::Type::kPointerMove: {
      size_t coalesced_size = coalesced_event.CoalescedEventSize();
      for (size_t i = 0; i < coalesced_size; i++)
        UpdatePrediction(coalesced_event.CoalescedEvent(i));

      if (enable_resampling_)
        ApplyResampling(frame_time, coalesced_event.EventPointer());

      AddPredictedEvents(coalesced_event);
      break;
    }
    case WebInputEvent::Type::kTouchScrollStarted:
    case WebInputEvent::Type::kPointerCausedUaAction:
      pointer_id_predictor_map_.clear();
      break;
    default:
      ResetPredictor(coalesced_event.Event());
  }
}

std::unique_ptr<ui::InputPredictor> InputEventPrediction::CreatePredictor()
    const {
  return PredictorFactory::GetPredictor(selected_predictor_type_);
}

void InputEventPrediction::UpdatePrediction(const WebInputEvent& event) {
  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    DCHECK(event.GetType() == WebInputEvent::Type::kTouchMove);
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].state ==
          blink::WebTouchPoint::State::kStateMoved) {
        UpdateSinglePointer(touch_event.touches[i], touch_event.TimeStamp());
      }
    }
  } else {
    DCHECK(event.GetType() == WebInputEvent::Type::kMouseMove ||
           event.GetType() == WebInputEvent::Type::kPointerMove);
    UpdateSinglePointer(*ToWebPointerProperties(&event), event.TimeStamp());
  }
  last_event_timestamp_ = event.TimeStamp();
}

void InputEventPrediction::ApplyResampling(base::TimeTicks frame_time,
                                           WebInputEvent* event) {
  base::TimeDelta prediction_delta = frame_time - event->TimeStamp();
  base::TimeTicks predict_time;

  if (event->GetType() == WebInputEvent::Type::kTouchMove) {
    WebTouchEvent* touch_event = static_cast<WebTouchEvent*>(event);
    for (unsigned i = 0; i < touch_event->touches_length; ++i) {
      if (touch_event->touches[i].state ==
          blink::WebTouchPoint::State::kStateMoved) {
        if (auto* predictor = GetPredictor(touch_event->touches[i])) {
          // When resampling, we don't want to predict too far away because the
          // result will likely be inaccurate in that case. We then cut off the
          // prediction to the MaxResampleTime for the predictor.
          prediction_delta =
              std::min(prediction_delta, predictor->MaxResampleTime());
          predict_time = event->TimeStamp() + prediction_delta;

          if (GetPointerPrediction(predict_time, &touch_event->touches[i]))
            event->SetTimeStamp(predict_time);
        }
      }
    }
  } else {
    WebPointerProperties* pointer_event = ToWebPointerProperties(event);
    if (auto* predictor = GetPredictor(*pointer_event)) {
      // Cutoff prediction if delta > MaxResampleTime
      prediction_delta =
          std::min(prediction_delta, predictor->MaxResampleTime());
      predict_time = event->TimeStamp() + prediction_delta;

      if (GetPointerPrediction(predict_time, pointer_event))
        event->SetTimeStamp(predict_time);
    }
  }
}

void InputEventPrediction::ResetPredictor(const WebInputEvent& event) {
  if (WebInputEvent::IsTouchEventType(event.GetType())) {
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    for (unsigned i = 0; i < touch_event.touches_length; ++i) {
      if (touch_event.touches[i].state !=
              blink::WebTouchPoint::State::kStateMoved &&
          touch_event.touches[i].state !=
              blink::WebTouchPoint::State::kStateStationary)
        pointer_id_predictor_map_.erase(touch_event.touches[i].id);
    }
  } else if (WebInputEvent::IsMouseEventType(event.GetType())) {
    ResetSinglePredictor(static_cast<const WebMouseEvent&>(event));
  } else if (WebInputEvent::IsPointerEventType(event.GetType())) {
    ResetSinglePredictor(static_cast<const WebPointerEvent&>(event));
  }
}

void InputEventPrediction::AddPredictedEvents(
    blink::WebCoalescedInputEvent& coalesced_event) {
  base::TimeTicks predict_time = last_event_timestamp_;
  base::TimeTicks max_prediction_timestamp =
      last_event_timestamp_ + mouse_predictor_->MaxPredictionTime();
  bool success = true;
  while (success) {
    std::unique_ptr<WebInputEvent> predicted_event =
        coalesced_event.Event().Clone();
    success = false;
    if (predicted_event->GetType() == WebInputEvent::Type::kTouchMove) {
      WebTouchEvent& touch_event =
          static_cast<WebTouchEvent&>(*predicted_event);
      // Average all touch intervals
      base::TimeDelta touch_time_interval;
      for (unsigned i = 0; i < touch_event.touches_length; ++i) {
        touch_time_interval +=
            GetPredictionTimeInterval(touch_event.touches[i]);
      }
      predict_time += touch_time_interval / touch_event.touches_length;
      if (predict_time <= max_prediction_timestamp) {
        for (unsigned i = 0; i < touch_event.touches_length; ++i) {
          if (touch_event.touches[i].state ==
              blink::WebTouchPoint::State::kStateMoved) {
            success =
                GetPointerPrediction(predict_time, &touch_event.touches[i]);
          }
        }
      }
    } else {
      WebPointerProperties* pointer_event =
          ToWebPointerProperties(predicted_event.get());
      predict_time += GetPredictionTimeInterval(*pointer_event);
      success = predict_time <= max_prediction_timestamp &&
                GetPointerPrediction(predict_time, pointer_event);
    }
    if (success) {
      predicted_event->SetTimeStamp(predict_time);
      coalesced_event.AddPredictedEvent(*predicted_event);
    }
  }
}

ui::InputPredictor* InputEventPrediction::GetPredictor(
    const WebPointerProperties& event) const {
  if (event.pointer_type == WebPointerProperties::PointerType::kMouse)
    return mouse_predictor_.get();

  auto predictor = pointer_id_predictor_map_.find(event.id);
  if (predictor != pointer_id_predictor_map_.end())
    return predictor->second.get();

  return nullptr;
}

base::TimeDelta InputEventPrediction::GetPredictionTimeInterval(
    const WebPointerProperties& event) const {
  if (auto* predictor = GetPredictor(event))
    return predictor->TimeInterval();
  return mouse_predictor_->TimeInterval();
}

void InputEventPrediction::UpdateSinglePointer(
    const WebPointerProperties& event,
    base::TimeTicks event_time) {
  ui::InputPredictor::InputData data = {event.PositionInWidget(), event_time};
  if (auto* predictor = GetPredictor(event)) {
    predictor->Update(data);
  } else {
    // Create new predictor.
    auto pair = std::make_pair(event.id, CreatePredictor());
    pointer_id_predictor_map_.insert(std::move(pair));
    pointer_id_predictor_map_[event.id]->Update(data);
  }
}

bool InputEventPrediction::GetPointerPrediction(base::TimeTicks predict_time,
                                                WebPointerProperties* event) {
  // Reset mouse predictor if pointer type is touch or stylus
  if (event->pointer_type != WebPointerProperties::PointerType::kMouse)
    mouse_predictor_->Reset();

  if (auto* predictor = GetPredictor(*event)) {
    if (auto predict_result = predictor->GeneratePrediction(predict_time)) {
      event->SetPositionInWidget(predict_result->pos);
      return true;
    }
  }
  return false;
}

void InputEventPrediction::ResetSinglePredictor(
    const WebPointerProperties& event) {
  if (event.pointer_type == WebPointerProperties::PointerType::kMouse)
    mouse_predictor_->Reset();
  else
    pointer_id_predictor_map_.erase(event.id);
}

}  // namespace blink
