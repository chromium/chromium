// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/scroll_predictor.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/predictor_factory.h"
#include "ui/base/ui_base_features.h"

namespace blink {

ScrollPredictor::ScrollPredictor()
    : metrics_handler_("Event.InputEventPrediction.Scroll") {
  // Get the predictor from feature flags
  std::string predictor_name = GetFieldTrialParamValueByFeature(
      blink::features::kResamplingScrollEvents, "predictor");

  if (predictor_name.empty())
    predictor_name = ::features::kPredictorNameLinearResampling;

  input_prediction::PredictorType predictor_type =
      PredictorFactory::GetPredictorTypeFromName(predictor_name);
  predictor_ = PredictorFactory::GetPredictor(predictor_type);

  filtering_enabled_ =
      base::FeatureList::IsEnabled(blink::features::kFilteringScrollPrediction);

  if (filtering_enabled_) {
    // Get the filter from feature flags
    std::string filter_name = GetFieldTrialParamValueByFeature(
        blink::features::kFilteringScrollPrediction, "filter");

    input_prediction::FilterType filter_type =
        filter_factory_->GetFilterTypeFromName(filter_name);

    filter_factory_ = std::make_unique<FilterFactory>(
        blink::features::kFilteringScrollPrediction, predictor_type,
        filter_type);

    filter_ = filter_factory_->CreateFilter(filter_type, predictor_type);
  }
}

ScrollPredictor::~ScrollPredictor() = default;

void ScrollPredictor::ResetOnGestureScrollBegin(const WebGestureEvent& event) {
  DCHECK(event.GetType() == WebInputEvent::Type::kGestureScrollBegin);
  // Only do resampling for scroll on touchscreen.
  if (event.SourceDevice() == WebGestureDevice::kTouchscreen) {
    should_resample_scroll_events_ = true;
    Reset();
  }
}

std::unique_ptr<EventWithCallback> ScrollPredictor::ResampleScrollEvents(
    std::unique_ptr<EventWithCallback> event_with_callback,
    base::TimeTicks frame_time,
    base::TimeDelta frame_interval) {
  if (!should_resample_scroll_events_)
    return event_with_callback;

  const EventWithCallback::OriginalEventList& original_events =
      event_with_callback->original_events();

  if (event_with_callback->event().GetType() ==
      WebInputEvent::Type::kGestureScrollUpdate) {
    // TODO(eirage): When scroll events are coalesced with pinch, we can have
    // empty original event list. In that case, we can't use the original events
    // to update the prediction. We don't want to use the aggregated event to
    // update because of the event time stamp, so skip the prediction for now.
    if (original_events.empty() ||
        event_with_callback->coalesced_scroll_and_pinch())
      return event_with_callback;

    for (auto& coalesced_event : original_events)
      UpdatePrediction(coalesced_event.event_->Event(), frame_time);

    if (should_resample_scroll_events_) {
      auto* metrics = event_with_callback->metrics()
                          ? event_with_callback->metrics()->AsScrollUpdate()
                          : nullptr;
      ResampleEvent(frame_time, frame_interval,
                    event_with_callback->event_pointer(),
                    &event_with_callback->latency_info(), metrics);
    }

    metrics_handler_.EvaluatePrediction();

  } else if (event_with_callback->event().GetType() ==
             WebInputEvent::Type::kGestureScrollEnd) {
    should_resample_scroll_events_ = false;
  }

  return event_with_callback;
}

void ScrollPredictor::Reset() {
  predictor_->Reset();
  if (filtering_enabled_)
    filter_->Reset();
  current_event_accumulated_delta_ = gfx::PointF();
  last_predicted_accumulated_delta_ = gfx::PointF();
  metrics_handler_.Reset();
}

void ScrollPredictor::UpdatePrediction(const WebInputEvent& event,
                                       base::TimeTicks frame_time) {
  DCHECK(event.GetType() == WebInputEvent::Type::kGestureScrollUpdate);
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);
  // When fling, GSU is sending per frame, resampling is not needed.
  if (gesture_event.data.scroll_update.inertial_phase ==
      WebGestureEvent::InertialPhaseState::kMomentum) {
    should_resample_scroll_events_ = false;
    return;
  }

  current_event_accumulated_delta_.Offset(
      gesture_event.data.scroll_update.delta_x,
      gesture_event.data.scroll_update.delta_y);
  ui::InputPredictor::InputData data = {current_event_accumulated_delta_,
                                        gesture_event.TimeStamp()};

  predictor_->Update(data);

  metrics_handler_.AddRealEvent(current_event_accumulated_delta_,
                                gesture_event.TimeStamp(), frame_time,
                                true /* Scrolling */);
}

void ScrollPredictor::ResampleEvent(base::TimeTicks frame_time,
                                    base::TimeDelta frame_interval,
                                    WebInputEvent* event,
                                    ui::LatencyInfo* latency_info,
                                    cc::ScrollUpdateEventMetrics* metrics) {
  DCHECK(event->GetType() == WebInputEvent::Type::kGestureScrollUpdate);
  WebGestureEvent* gesture_event = static_cast<WebGestureEvent*>(event);

  TRACE_EVENT_BEGIN1("input", "ScrollPredictor::ResampleScrollEvents",
                     "OriginalDelta",
                     gfx::PointF(gesture_event->data.scroll_update.delta_x,
                                 gesture_event->data.scroll_update.delta_y)
                         .ToString());
  gfx::PointF predicted_accumulated_delta = current_event_accumulated_delta_;

  base::TimeDelta prediction_delta = frame_time - gesture_event->TimeStamp();
  bool predicted = false;

  // For resampling, we don't want to predict too far away because the result
  // will likely be inaccurate in that case. We cut off the prediction to the
  // maximum available for the current predictor
  prediction_delta = std::min(prediction_delta, predictor_->MaxResampleTime());

  base::TimeTicks prediction_time =
      gesture_event->TimeStamp() + prediction_delta;

  auto result = predictor_->GeneratePrediction(prediction_time, frame_interval);
  if (result) {
    predicted_accumulated_delta = result->pos;
    gesture_event->SetTimeStamp(result->time_stamp);
    predicted = true;
  }

  // Feed the filter with the first non-predicted events but only apply
  // filtering on predicted events
  gfx::PointF filtered_pos = predicted_accumulated_delta;
  if (filtering_enabled_ && filter_->Filter(prediction_time, &filtered_pos) &&
      predicted)
    predicted_accumulated_delta = filtered_pos;

  // If the last resampled GSU over predict the delta, new GSU might try to
  // scroll back to make up the difference, which cause the scroll to jump
  // back. So we set the new delta to 0 when predicted delta is in different
  // direction to the original event.
  gfx::Vector2dF new_delta =
      predicted_accumulated_delta - last_predicted_accumulated_delta_;
  gesture_event->data.scroll_update.delta_x =
      (new_delta.x() * gesture_event->data.scroll_update.delta_x < 0)
          ? 0
          : new_delta.x();
  gesture_event->data.scroll_update.delta_y =
      (new_delta.y() * gesture_event->data.scroll_update.delta_y < 0)
          ? 0
          : new_delta.y();

  // Sync the predicted `delta_y` to `metrics` for AverageLag metric.
  if (metrics)
    metrics->set_predicted_delta(new_delta.y());

  TRACE_EVENT_END1("input", "ScrollPredictor::ResampleScrollEvents",
                   "PredictedDelta",
                   gfx::PointF(gesture_event->data.scroll_update.delta_x,
                               gesture_event->data.scroll_update.delta_y)
                       .ToString());
  last_predicted_accumulated_delta_.Offset(
      gesture_event->data.scroll_update.delta_x,
      gesture_event->data.scroll_update.delta_y);

  if (predicted) {
    metrics_handler_.AddPredictedEvent(predicted_accumulated_delta,
                                       result->time_stamp, frame_time,
                                       true /* Scrolling */);
  }
}

}  // namespace blink
