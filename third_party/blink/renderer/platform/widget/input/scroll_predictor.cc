// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/scroll_predictor.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/common/task_annotator.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/widget/input/prediction/predictor_factory.h"
#include "ui/base/ui_base_features.h"
#include "ui/latency/latency_info.h"

namespace blink {

using ::perfetto::protos::pbzero::ChromeLatencyInfo2;

ScrollPredictor::ScrollPredictor()
    : metrics_handler_("Event.InputEventPrediction.Scroll"),
      fling_metrics_handler_("Event.InputEventPrediction.Fling",
                             /*report_score_metrics=*/false) {
  // Get the predictor from feature flags
  std::string predictor_name = GetFieldTrialParamValueByFeature(
      blink::features::kResamplingScrollEvents, "predictor");

  if (predictor_name.empty())
    predictor_name = ::features::kPredictorNameLinearResampling;

  input_prediction::PredictorType predictor_type =
      PredictorFactory::GetPredictorTypeFromName(predictor_name);
  predictor_ = PredictorFactory::GetPredictor(predictor_type);

  // Initialize the synthetic predictor. Defaults to Kalman for better
  // stability if enabled, otherwise use the same predictor as real events.
  if (base::FeatureList::IsEnabled(
          blink::features::kScrollPredictorSyntheticKalman)) {
    synthetic_predictor_ = PredictorFactory::GetPredictor(
        input_prediction::PredictorType::kScrollPredictorTypeKalman);
  } else {
    synthetic_predictor_ = PredictorFactory::GetPredictor(predictor_type);
  }

  filtering_enabled_ =
      base::FeatureList::IsEnabled(blink::features::kFilteringScrollPrediction);

  if (filtering_enabled_) {
    // Get the filter from feature flags
    std::string filter_name =
        blink::features::kFilteringScrollPredictionFilterParam.Get();

    input_prediction::FilterType filter_type =
        FilterFactory::GetFilterTypeFromName(filter_name);

    filter_factory_ = std::make_unique<FilterFactory>(
        blink::features::kFilteringScrollPrediction, predictor_type,
        filter_type);

    filter_ = filter_factory_->CreateFilter();
  }
}

ScrollPredictor::~ScrollPredictor() = default;

ui::PredictionMetricsHandler& ScrollPredictor::GetMetricsHandler(
    WebGestureEvent::InertialPhaseState phase) {
  return phase == WebGestureEvent::InertialPhaseState::kMomentum
             ? fling_metrics_handler_
             : metrics_handler_;
}

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
    base::TimeDelta frame_interval,
    const WebInputEvent* next_event,
    const cc::EventMetrics* next_event_metrics) {
  // If `next_event` is null, `next_event_metrics` must also be null.
  DCHECK(next_event != nullptr || next_event_metrics == nullptr);

  if (!should_resample_scroll_events_)
    return event_with_callback;

  int64_t trace_id = event_with_callback->latency_info().trace_id();
  const EventWithCallback::OriginalEventList& original_events =
      event_with_callback->original_events();
  TRACE_EVENT(
      "input,benchmark,latencyInfo", "LatencyInfo.Flow",
      [&](perfetto::EventContext ctx) {
        base::TaskAnnotator::EmitTaskTimingDetails(ctx);
        ChromeLatencyInfo2* latency_info = ui::LatencyInfo::FillTraceEvent(
            ctx, trace_id,
            ChromeLatencyInfo2::Step::STEP_RESAMPLE_SCROLL_EVENTS);
        for (const EventWithCallback::OriginalEventWithCallback&
                 coalesced_event : original_events) {
          int64_t coalesced_event_trace_id =
              coalesced_event.event_->latency_info().trace_id();
          latency_info->add_coalesced_trace_ids(coalesced_event_trace_id);
        }
      });

  if (event_with_callback->event().GetType() ==
      WebInputEvent::Type::kGestureScrollUpdate) {
    // TODO(eirage): When scroll events are coalesced with pinch, we can have
    // empty original event list. In that case, we can't use the original events
    // to update the prediction. We don't want to use the aggregated event to
    // update because of the event time stamp, so skip the prediction for now.
    if (original_events.empty() ||
        event_with_callback->coalesced_scroll_and_pinch())
      return event_with_callback;

    for (auto& coalesced_event : original_events) {
      UpdatePrediction(coalesced_event.event_->Event(),
                       coalesced_event.metrics_.get(), frame_time);
    }

    // Update the predictor with the next event in the queue (the first to
    // arrive after the `sample_time` cutoff). This improves prediction accuracy
    // by incorporating a more current data point than the historical events in
    // `original_events`.
    if (next_event) {
      DCHECK(next_event->GetType() ==
             WebInputEvent::Type::kGestureScrollUpdate);
      UpdatePredictionForEventAfterSampleTime(*next_event, next_event_metrics);
    }

    if (should_resample_scroll_events_) {
      ResampleEvent(frame_time, frame_interval,
                    event_with_callback->event_pointer(), trace_id,
                    /*use_synthetic_predictor=*/false);
      // Sync the predicted `delta_y` to `metrics` for AverageLag metric.
      auto* metrics = event_with_callback->metrics()
                          ? event_with_callback->metrics()->AsScrollUpdate()
                          : nullptr;
      if (metrics) {
        WebGestureEvent* gesture_event =
            static_cast<WebGestureEvent*>(event_with_callback->event_pointer());
        // Do not sync predicted delta for AverageLag if this is a fling.
        // AverageLag measures finger-to-pixel distance, which is undefined
        // during fling.
        if (gesture_event->data.scroll_update.inertial_phase !=
            WebGestureEvent::InertialPhaseState::kMomentum) {
          metrics->set_predicted_delta(
              gesture_event->data.scroll_update.delta_y);
        }
      }
    }

    metrics_handler_.EvaluatePrediction();
    fling_metrics_handler_.EvaluatePrediction();

  } else if (event_with_callback->event().GetType() ==
             WebInputEvent::Type::kGestureScrollEnd) {
    should_resample_scroll_events_ = false;
  }

  return event_with_callback;
}

std::unique_ptr<EventWithCallback>
ScrollPredictor::GenerateSyntheticScrollUpdate(
    base::TimeTicks frame_time,
    base::TimeDelta frame_interval,
    mojom::blink::GestureDevice gesture_device,
    int modifiers) {
  if (!should_resample_scroll_events_ ||
      !HasPrediction(frame_time, frame_interval)) {
    return nullptr;
  }
  WebGestureEvent gesture_event(WebInputEvent::Type::kGestureScrollUpdate,
                                modifiers, frame_time, gesture_device);

  // Inherit the phase from the curve we are predicting.
  gesture_event.data.scroll_update.inertial_phase = last_inertial_phase_;
  bool is_scroll_inertial =
      (last_inertial_phase_ == WebGestureEvent::InertialPhaseState::kMomentum);

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(base::trace_event::GetNextGlobalTraceId());
  ResampleEvent(frame_time, frame_interval, &gesture_event,
                latency_info.trace_id(), /*use_synthetic_predictor=*/true);

  // TODO(b/329346768): We should also add a new `BEGIN` stage, instead of
  // re-using the one that is explicitly about the `content::RenderWidgetHost`.
  latency_info.AddLatencyNumberWithTraceName(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      "InputLatency::GestureScrollUpdate", frame_time);

  std::unique_ptr<cc::ScrollUpdateEventMetrics> metrics =
      cc::ScrollUpdateEventMetrics::Create(
          ui::EventType::kGestureScrollUpdate,
          gesture_event.GetScrollInputType(),
          /*is_inertial=*/is_scroll_inertial,
          cc::ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
          /*delta=*/gesture_event.data.scroll_update.delta_y,
          /*timestamp=*/gesture_event.TimeStamp(),
          /*arrived_in_browser_main_timestamp=*/gesture_event.TimeStamp(),
          /*blocking_touch_dispatched_to_renderer=*/gesture_event.TimeStamp(),
          /*trace_id=*/
          base::IdType64<class ui::LatencyInfo>(latency_info.trace_id()),
          last_scroll_begin_arrival_timestamp_);
  if (!is_scroll_inertial) {
    metrics->set_predicted_delta(gesture_event.data.scroll_update.delta_y);
  }
  metrics->set_is_synthetic(true);
  return std::make_unique<EventWithCallback>(
      std::make_unique<WebCoalescedInputEvent>(std::move(gesture_event),
                                               latency_info),
      base::BindOnce([](InputHandlerProxy::EventDisposition event_disposition,
                        std::unique_ptr<WebCoalescedInputEvent> event,
                        std::unique_ptr<InputHandlerProxy::DidOverscrollParams>
                            overscroll_params,
                        const WebInputEventAttribution& attribution,
                        std::unique_ptr<cc::EventMetrics> metrics) {
        int64_t trace_id = event->latency_info().trace_id();
        TRACE_EVENT(
            "input,benchmark,latencyInfo", "LatencyInfo.Flow",
            [&](perfetto::EventContext ctx) {
              base::TaskAnnotator::EmitTaskTimingDetails(ctx);
              ui::LatencyInfo::FillTraceEvent(
                  ctx, trace_id,
                  ChromeLatencyInfo2::Step::
                      STEP_DID_HANDLE_INPUT_AND_OVERSCROLL,
                  ChromeLatencyInfo2::InputType::GESTURE_SCROLL_UPDATE_EVENT);
            });
      }),
      std::move(metrics));
}

bool ScrollPredictor::HasPrediction(base::TimeTicks frame_time,
                                    base::TimeDelta frame_interval) const {
  // If the last real user event is too old, stop generating synthetic events
  // to avoid creating "phantom" scroll motion.
  base::TimeTicks prediction_time = frame_time;
  if (base::FeatureList::IsEnabled(
          blink::features::kScrollPredictorRefinedHasPrediction)) {
    prediction_time += ResampleLatency(frame_interval);
  }

  base::TimeDelta max_resample_time =
      blink::features::kScrollPredictorMaxResampleTime.Get();

  if (!last_prediction_update_timestamp_.is_null() &&
      prediction_time - last_prediction_update_timestamp_ > max_resample_time) {
    return false;
  }

  return synthetic_predictor_->HasPrediction();
}

void ScrollPredictor::Reset() {
  predictor_->Reset();
  synthetic_predictor_->Reset();
  if (filtering_enabled_) {
    filter_ = filter_factory_->CreateFilter();
  }
  current_event_accumulated_delta_ = gfx::PointF();
  last_predicted_accumulated_delta_ = gfx::PointF();
  last_real_delta_ = gfx::Vector2dF();
  last_resample_time_ = base::TimeTicks();
  last_raw_synthetic_pos_ = gfx::PointF();
  last_raw_linear_pos_ = gfx::PointF();
  last_prediction_update_timestamp_ = base::TimeTicks();  // Reset the timestamp
  last_inertial_phase_ = WebGestureEvent::InertialPhaseState::kUnknownMomentum;
  metrics_handler_.Reset();
  fling_metrics_handler_.Reset();
  last_scroll_begin_arrival_timestamp_ = base::TimeTicks();
}

base::TimeDelta ScrollPredictor::ResampleLatency(
    base::TimeDelta frame_interval) const {
  return predictor_->ResampleLatency(frame_interval);
}

void ScrollPredictor::UpdatePredictionForEventAfterSampleTime(
    const WebInputEvent& event,
    const cc::EventMetrics* metrics) {
  CHECK(event.GetType() == WebInputEvent::Type::kGestureScrollUpdate);
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);

  if (gesture_event.data.scroll_update.inertial_phase ==
      WebGestureEvent::InertialPhaseState::kMomentum) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kResampleScrollEventsForFling)) {
      return;
    }
  }

  if (last_prediction_update_timestamp_ < gesture_event.TimeStamp()) {
    ui::InputPredictor::InputData data = {
        current_event_accumulated_delta_ +
            gfx::Vector2dF(gesture_event.data.scroll_update.delta_x,
                           gesture_event.data.scroll_update.delta_y),
        gesture_event.TimeStamp()};
    predictor_->Update(data);
    synthetic_predictor_->Update(data);
    last_prediction_update_timestamp_ = gesture_event.TimeStamp();
    if (metrics) {
      if (const cc::ScrollEventMetrics* scroll_metrics = metrics->AsScroll()) {
        last_scroll_begin_arrival_timestamp_ =
            scroll_metrics->scroll_begin_arrival_timestamp();
      }
    }
  }
}

void ScrollPredictor::UpdatePrediction(const WebInputEvent& event,
                                       const cc::EventMetrics* metrics,
                                       base::TimeTicks frame_time) {
  DCHECK(event.GetType() == WebInputEvent::Type::kGestureScrollUpdate);
  const WebGestureEvent& gesture_event =
      static_cast<const WebGestureEvent&>(event);

  last_inertial_phase_ = gesture_event.data.scroll_update.inertial_phase;

  // When fling, GSU is sending per frame, resampling is not needed unless
  // kResampleScrollEventsForFling is enabled.
  if (gesture_event.data.scroll_update.inertial_phase ==
      WebGestureEvent::InertialPhaseState::kMomentum) {
    if (!base::FeatureList::IsEnabled(
            blink::features::kResampleScrollEventsForFling)) {
      should_resample_scroll_events_ = false;
      return;
    }
  }

  current_event_accumulated_delta_.Offset(
      gesture_event.data.scroll_update.delta_x,
      gesture_event.data.scroll_update.delta_y);
  ui::InputPredictor::InputData data = {current_event_accumulated_delta_,
                                        gesture_event.TimeStamp()};

  // This check prevents the predictor from processing the same event data
  // twice. An event might be sent to the predictor in
  // `UpdatePredictionForEventAfterSampleTime` and then later appear in the
  // `original_events` list within `ResampleScrollEvents`, which calls this
  // function. The timestamp comparison ensures the predictor only updates with
  // each event once.
  if (last_prediction_update_timestamp_ < gesture_event.TimeStamp()) {
    predictor_->Update(data);
    synthetic_predictor_->Update(data);
    last_prediction_update_timestamp_ = gesture_event.TimeStamp();
    if (metrics) {
      if (const cc::ScrollEventMetrics* scroll_metrics = metrics->AsScroll()) {
        last_scroll_begin_arrival_timestamp_ =
            scroll_metrics->scroll_begin_arrival_timestamp();
      }
    }
  }

  GetMetricsHandler(gesture_event.data.scroll_update.inertial_phase)
      .AddRealEvent(current_event_accumulated_delta_, gesture_event.TimeStamp(),
                    frame_time, true /* Scrolling */);
}

void ScrollPredictor::ResampleEvent(base::TimeTicks frame_time,
                                    base::TimeDelta frame_interval,
                                    WebInputEvent* event,
                                    int64_t trace_id,
                                    bool use_synthetic_predictor) {
  ui::InputPredictor* predictor =
      use_synthetic_predictor ? synthetic_predictor_.get() : predictor_.get();
  DCHECK(predictor);
  DCHECK(event->GetType() == WebInputEvent::Type::kGestureScrollUpdate);
  WebGestureEvent* gesture_event = static_cast<WebGestureEvent*>(event);

  float original_delta_x = gesture_event->data.scroll_update.delta_x;
  float original_delta_y = gesture_event->data.scroll_update.delta_y;

  TRACE_EVENT_BEGIN(
      "input", "ScrollPredictor::ResampleScrollEvents",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* scroll_data = event->set_scroll_deltas();
        scroll_data->set_trace_id(trace_id);
        scroll_data->set_original_delta_x(original_delta_x);
        scroll_data->set_original_delta_y(original_delta_y);
      });
  gfx::PointF predicted_accumulated_delta =
      last_predicted_accumulated_delta_ +
      gfx::Vector2dF(original_delta_x, original_delta_y);

  base::TimeDelta prediction_delta = frame_time - gesture_event->TimeStamp();
  bool predicted = false;
  // Tracks if the synthetic predictor's delta was successfully applied.
  bool used_synthetic_delta = false;

  // For resampling, we don't want to predict too far away because the result
  // will likely be inaccurate in that case.
  prediction_delta = std::min(
      prediction_delta, blink::features::kScrollPredictorMaxResampleTime.Get());

  base::TimeTicks prediction_time =
      gesture_event->TimeStamp() + prediction_delta;

  base::TimeTicks active_prediction_time = prediction_time;
  if (!predictor->AppliesResampleLatencyInternally()) {
    // Add resample latency to the prediction time to be consistent with the
    // linear resampler producing the input at VSync + kResampleLatency.
    active_prediction_time += ResampleLatency(frame_interval);
  }

  auto result =
      predictor->GeneratePrediction(active_prediction_time, frame_interval);

  if (!use_synthetic_predictor) {
    // For real input frames, cache the absolute positions of both predictors.
    // This establishes the anchors required to calculate relative step deltas
    // during subsequent synthetic frames.
    last_real_delta_ = gfx::Vector2dF(original_delta_x, original_delta_y);
    last_raw_linear_pos_ = result ? result->pos : predicted_accumulated_delta;
    // Cache the synthetic predictor's output.
    auto syn_res = synthetic_predictor_->GeneratePrediction(
        active_prediction_time, frame_interval);
    last_raw_synthetic_pos_ = syn_res ? syn_res->pos : gfx::PointF();
  }

  if (result) {
    if (use_synthetic_predictor) {
      // For synthetic gap-filling, apply the predictor's relative movement
      // to |last_predicted_accumulated_delta_|. This provides positional
      // continuity and prevents visual snapping when transitioning between
      // predictors.
      gfx::Vector2dF step_delta = gfx::Vector2dF();

      if (!last_raw_synthetic_pos_.IsOrigin()) {
        step_delta = result->pos - last_raw_synthetic_pos_;
        used_synthetic_delta = true;
      } else if (!last_raw_linear_pos_.IsOrigin()) {
        // The synthetic predictor may not be stabilized. If it lacks a valid
        // anchor, fall back to the primary predictor's delta to preserve scroll
        // momentum.
        if (auto fallback_res = predictor_->GeneratePrediction(
                active_prediction_time, frame_interval)) {
          step_delta = fallback_res->pos - last_raw_linear_pos_;
          last_raw_linear_pos_ = fallback_res->pos;
        }
      }
      last_raw_synthetic_pos_ = result->pos;

      predicted_accumulated_delta =
          last_predicted_accumulated_delta_ + step_delta;
      result->pos = predicted_accumulated_delta;
    } else {
      // For frame containing real input events, use the result from primary
      // predictor.
      predicted_accumulated_delta = result->pos;
    }
    gesture_event->SetTimeStamp(result->time_stamp);
    predicted = true;
  }

  // Feed the filter with the first non-predicted events but only apply
  // filtering on predicted events.
  gfx::PointF filtered_pos = predicted_accumulated_delta;

  // Allow bypassing the stateful filter for synthetic frames to evaluate the
  // raw performance of the synthetic predictor without compounding filter lag.
  const bool bypass_filter =
      used_synthetic_delta &&
      base::FeatureList::IsEnabled(
          blink::features::kScrollPredictorFilteringBypassOnSynthetic);

  if (filtering_enabled_ && !bypass_filter &&
      filter_->Filter(active_prediction_time, &filtered_pos) && predicted) {
    predicted_accumulated_delta = filtered_pos;
  }

  // If the last resampled GSU over predict the delta, new GSU might try to
  // scroll back to make up the difference, which cause the scroll to jump
  // back. So we set the new delta to 0 when predicted delta is in different
  // direction to the original event.
  gfx::Vector2dF new_delta =
      predicted_accumulated_delta - last_predicted_accumulated_delta_;

  const gfx::Vector2dF reference_delta =
      !use_synthetic_predictor
          ? gfx::Vector2dF(original_delta_x, original_delta_y)
          : last_real_delta_;

  gesture_event->data.scroll_update.delta_x =
      (new_delta.x() * reference_delta.x() < 0) ? 0 : new_delta.x();
  gesture_event->data.scroll_update.delta_y =
      (new_delta.y() * reference_delta.y() < 0) ? 0 : new_delta.y();
  TRACE_EVENT_END("input", [&](perfetto::EventContext ctx) {
    auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
    auto* scroll_data = event->set_scroll_deltas();
    scroll_data->set_provided_to_compositor_delta_x(
        gesture_event->data.scroll_update.delta_x);
    scroll_data->set_provided_to_compositor_delta_y(
        gesture_event->data.scroll_update.delta_y);
  });
  last_predicted_accumulated_delta_.Offset(
      gesture_event->data.scroll_update.delta_x,
      gesture_event->data.scroll_update.delta_y);

  if (predicted) {
    GetMetricsHandler(gesture_event->data.scroll_update.inertial_phase)
        .AddPredictedEvent(predicted_accumulated_delta, result->time_stamp,
                           frame_time, true /* Scrolling */);
  }
  last_resample_time_ = gesture_event->TimeStamp();
}

}  // namespace blink
