// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include "base/format_macros.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::ScopedUkmHierarchicalTimer(
    scoped_refptr<LocalFrameUkmAggregator> aggregator,
    size_t metric_index,
    const base::TickClock* clock)
    : aggregator_(aggregator),
      metric_index_(metric_index),
      clock_(clock),
      start_time_(clock_->NowTicks()) {}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::ScopedUkmHierarchicalTimer(
    ScopedUkmHierarchicalTimer&& other)
    : aggregator_(other.aggregator_),
      metric_index_(other.metric_index_),
      clock_(other.clock_),
      start_time_(other.start_time_) {
  other.aggregator_ = nullptr;
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::
    ~ScopedUkmHierarchicalTimer() {
  if (aggregator_ && base::TimeTicks::IsHighResolution()) {
    aggregator_->RecordSample(metric_index_, start_time_, clock_->NowTicks());
  }
}

LocalFrameUkmAggregator::LocalFrameUkmAggregator(int64_t source_id,
                                                 ukm::UkmRecorder* recorder)
    : source_id_(source_id),
      recorder_(recorder),
      clock_(base::DefaultTickClock::GetInstance()),
      event_name_("Blink.UpdateTime") {
  // Record average and worst case for the primary metric.
  primary_metric_.reset();

  // Define the UMA for the primary metric.
  primary_metric_.uma_counter.reset(
      new CustomCountHistogram("Blink.MainFrame.UpdateTime", 0, 10000000, 50));
  primary_metric_.pre_fcp_uma_counter.reset(new CustomCountHistogram(
      "Blink.MainFrame.UpdateTime.PreFCP", 0, 10000000, 50));
  primary_metric_.post_fcp_uma_counter.reset(new CustomCountHistogram(
      "Blink.MainFrame.UpdateTime.PostFCP", 0, 10000000, 50));
  primary_metric_.uma_aggregate_counter.reset(new CustomCountHistogram(
      "Blink.MainFrame.UpdateTime.AggregatedPreFCP", 0, 10000000, 50));

  // Set up the substrings to create the UMA names
  const String uma_preamble = "Blink.";
  const String uma_postscript = ".UpdateTime";
  const String uma_prefcp_postscript = ".PreFCP";
  const String uma_postfcp_postscript = ".PostFCP";
  const String uma_pre_fcp_aggregated_postscript = ".AggregatedPreFCP";
  const String uma_percentage_preamble = "Blink.MainFrame.";
  const String uma_percentage_postscript = "Ratio";

  // Set up sub-strings for the bucketed UMA metrics
  Vector<String> threshold_substrings;
  if (!bucket_thresholds().size()) {
    threshold_substrings.push_back(".All");
  } else {
    threshold_substrings.push_back(String::Format(
        ".LessThan%" PRId64 "ms", bucket_thresholds()[0].InMilliseconds()));
    for (wtf_size_t i = 1; i < bucket_thresholds().size(); ++i) {
      threshold_substrings.push_back(
          String::Format(".%" PRId64 "msTo%" PRId64 "ms",
                         bucket_thresholds()[i - 1].InMilliseconds(),
                         bucket_thresholds()[i].InMilliseconds()));
    }
    threshold_substrings.push_back(String::Format(
        ".MoreThan%" PRId64 "ms",
        bucket_thresholds()[bucket_thresholds().size() - 1].InMilliseconds()));
  }

  // Populate all the sub-metrics.
  absolute_metric_records_.ReserveInitialCapacity(kCount);
  main_frame_percentage_records_.ReserveInitialCapacity(kCount);
  for (unsigned i = 0; i < (unsigned)kCount; ++i) {
    const MetricInitializationData& metric_data = metrics_data()[i];

    // Absolute records report the absolute time for each metric per frame.
    // They also aggregate the time spent in each stage between navigation
    // (LocalFrameView resets) and First Contentful Paint.
    // They have an associated UMA too that we own and allocate here.
    auto& absolute_record = absolute_metric_records_.emplace_back();
    absolute_record.reset();
    absolute_record.pre_fcp_aggregate = base::TimeDelta();
    if (metric_data.has_uma) {
      StringBuilder uma_name;
      uma_name.Append(uma_preamble);
      uma_name.Append(metric_data.name);
      uma_name.Append(uma_postscript);
      absolute_record.uma_counter.reset(new CustomCountHistogram(
          uma_name.ToString().Utf8().c_str(), 0, 10000000, 50));
      StringBuilder pre_fcp_uma_name;
      pre_fcp_uma_name.Append(uma_name);
      pre_fcp_uma_name.Append(uma_prefcp_postscript);
      absolute_record.pre_fcp_uma_counter.reset(new CustomCountHistogram(
          pre_fcp_uma_name.ToString().Utf8().c_str(), 0, 10000000, 50));
      StringBuilder post_fcp_uma_name;
      post_fcp_uma_name.Append(uma_name);
      post_fcp_uma_name.Append(uma_postfcp_postscript);
      absolute_record.post_fcp_uma_counter.reset(new CustomCountHistogram(
          post_fcp_uma_name.ToString().Utf8().c_str(), 0, 10000000, 50));
      StringBuilder aggregated_uma_name;
      aggregated_uma_name.Append(uma_name);
      aggregated_uma_name.Append(uma_pre_fcp_aggregated_postscript);
      absolute_record.uma_aggregate_counter.reset(new CustomCountHistogram(
          aggregated_uma_name.ToString().Utf8().c_str(), 0, 10000000, 50));
    }

    // Percentage records report the ratio of each metric to the primary metric.
    // UMA counters are also associated with the ratios and we allocate and own
    // them here.
    auto& percentage_record = main_frame_percentage_records_.emplace_back();
    percentage_record.reset();
    for (auto bucket_substring : threshold_substrings) {
      StringBuilder uma_percentage_name;
      uma_percentage_name.Append(uma_percentage_preamble);
      uma_percentage_name.Append(metric_data.name);
      uma_percentage_name.Append(uma_percentage_postscript);
      uma_percentage_name.Append(bucket_substring);
      percentage_record.uma_counters_per_bucket.push_back(
          std::make_unique<CustomCountHistogram>(
              uma_percentage_name.ToString().Utf8().c_str(), 0, 10000000, 50));
    }
  }
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer
LocalFrameUkmAggregator::GetScopedTimer(size_t metric_index) {
  return ScopedUkmHierarchicalTimer(this, metric_index, clock_);
}

void LocalFrameUkmAggregator::BeginMainFrame() {
  DCHECK(!in_main_frame_update_);
  in_main_frame_update_ = true;
}

std::unique_ptr<cc::BeginMainFrameMetrics>
LocalFrameUkmAggregator::GetBeginMainFrameMetrics() {
  DCHECK(InMainFrameUpdate());

  // Use the main_frame_percentage_records_ because they are the ones that
  // only count time between the Begin and End of a main frame update.
  std::unique_ptr<cc::BeginMainFrameMetrics> metrics_data =
      std::make_unique<cc::BeginMainFrameMetrics>();
  metrics_data->handle_input_events =
      main_frame_percentage_records_[static_cast<unsigned>(
                                         MetricId::kHandleInputEvents)]
          .interval_duration;
  metrics_data->animate =
      main_frame_percentage_records_[static_cast<unsigned>(MetricId::kAnimate)]
          .interval_duration;
  metrics_data->style_update =
      main_frame_percentage_records_[static_cast<unsigned>(MetricId::kStyle)]
          .interval_duration;
  metrics_data->layout_update =
      main_frame_percentage_records_[static_cast<unsigned>(MetricId::kLayout)]
          .interval_duration;
  metrics_data->prepaint =
      main_frame_percentage_records_[static_cast<unsigned>(MetricId::kPrePaint)]
          .interval_duration;
  metrics_data->composite =
      main_frame_percentage_records_[static_cast<unsigned>(
                                         MetricId::kCompositing)]
          .interval_duration;
  metrics_data->paint =
      main_frame_percentage_records_[static_cast<unsigned>(MetricId::kPaint)]
          .interval_duration;
  metrics_data->scrolling_coordinator =
      main_frame_percentage_records_[static_cast<unsigned>(
                                         MetricId::kScrollingCoordinator)]
          .interval_duration;
  metrics_data->composite_commit =
      main_frame_percentage_records_[static_cast<unsigned>(
                                         MetricId::kCompositingCommit)]
          .interval_duration;
  return metrics_data;
}

void LocalFrameUkmAggregator::SetTickClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
}

void LocalFrameUkmAggregator::RecordForcedStyleLayoutUMA(
    base::TimeDelta& duration) {
  if (!calls_to_next_forced_style_layout_uma_) {
    auto& record = absolute_metric_records_[kForcedStyleAndLayout];
    record.uma_counter->CountMicroseconds(duration);
    if (is_before_fcp_)
      record.pre_fcp_uma_counter->CountMicroseconds(duration);
    else
      record.post_fcp_uma_counter->CountMicroseconds(duration);
    calls_to_next_forced_style_layout_uma_ =
        base::RandInt(0, mean_calls_between_forced_style_layout_uma_ * 2);
  } else {
    DCHECK_GT(calls_to_next_forced_style_layout_uma_, 0u);
    --calls_to_next_forced_style_layout_uma_;
  }
}

void LocalFrameUkmAggregator::DidReachFirstContentfulPaint(
    bool are_painting_main_frame) {
  DCHECK(is_before_fcp_);

  is_before_fcp_ = false;

  if (!are_painting_main_frame) {
    DCHECK(AllMetricsAreZero());
    return;
  }

#define CASE_FOR_ID(name)                                                  \
  case k##name:                                                            \
    builder.Set##name(absolute_record.pre_fcp_aggregate.InMicroseconds()); \
    break

  ukm::builders::Blink_PageLoad builder(source_id_);
  builder.SetMainFrame(primary_metric_.pre_fcp_aggregate.InMicroseconds());
  primary_metric_.uma_aggregate_counter->CountMicroseconds(
      primary_metric_.pre_fcp_aggregate);
  for (unsigned i = 0; i < (unsigned)kCount; ++i) {
    auto& absolute_record = absolute_metric_records_[i];
    if (absolute_record.uma_aggregate_counter) {
      absolute_record.uma_aggregate_counter->CountMicroseconds(
          absolute_record.pre_fcp_aggregate);
    }

    switch (static_cast<MetricId>(i)) {
      CASE_FOR_ID(Compositing);
      CASE_FOR_ID(CompositingCommit);
      CASE_FOR_ID(IntersectionObservation);
      CASE_FOR_ID(Paint);
      CASE_FOR_ID(PrePaint);
      CASE_FOR_ID(StyleAndLayout);
      CASE_FOR_ID(Style);
      CASE_FOR_ID(Layout);
      CASE_FOR_ID(ForcedStyleAndLayout);
      CASE_FOR_ID(ScrollingCoordinator);
      CASE_FOR_ID(HandleInputEvents);
      CASE_FOR_ID(Animate);
      CASE_FOR_ID(UpdateLayers);
      CASE_FOR_ID(ProxyCommit);
      case kCount:
      case kMainFrame:
        NOTREACHED();
        break;
    }
  }
  builder.Record(recorder_);

#undef CASE_FOR_ID
}

void LocalFrameUkmAggregator::RecordSample(size_t metric_index,
                                           base::TimeTicks start,
                                           base::TimeTicks end) {
  base::TimeDelta duration = end - start;

  // Accumulate for UKM and record the UMA
  DCHECK_LT(metric_index, absolute_metric_records_.size());
  auto& record = absolute_metric_records_[metric_index];
  record.interval_duration += duration;
  if (is_before_fcp_)
    record.pre_fcp_aggregate += duration;
  // Record the UMA
  // ForcedStyleAndLayout happen so frequently on some pages that we overflow
  // the signed 32 counter for number of events in a 30 minute period. So
  // randomly record with probability 1/100.
  if (record.uma_counter) {
    if (metric_index == static_cast<size_t>(kForcedStyleAndLayout)) {
      RecordForcedStyleLayoutUMA(duration);
    } else {
      record.uma_counter->CountMicroseconds(duration);
      if (is_before_fcp_) {
        record.pre_fcp_uma_counter->CountMicroseconds(duration);
      } else {
        record.post_fcp_uma_counter->CountMicroseconds(duration);
      }
    }
  }

  // Only record ratios when inside a main frame.
  if (in_main_frame_update_) {
    // Just record the duration for ratios. We compute the ratio later
    // when we know the frame time.
    main_frame_percentage_records_[metric_index].interval_duration += duration;
  }
}

void LocalFrameUkmAggregator::RecordEndOfFrameMetrics(base::TimeTicks start,
                                                      base::TimeTicks end) {
  // Any of the early out's in LocalFrameView::UpdateLifecyclePhases
  // will mean we are not in a main frame update. Recording is triggered
  // higher in the stack, so we cannot know to avoid calling this method.
  if (!in_main_frame_update_) {
    // Reset for the next frame to start the next recording period with
    // clear counters, even when we did not record anything this frame.
    ResetAllMetrics();
    return;
  }
  in_main_frame_update_ = false;

  base::TimeDelta duration = end - start;

  // Record UMA
  primary_metric_.uma_counter->CountMicroseconds(duration);
  if (is_before_fcp_)
    primary_metric_.pre_fcp_uma_counter->CountMicroseconds(duration);
  else
    primary_metric_.post_fcp_uma_counter->CountMicroseconds(duration);

  // Record primary time information
  primary_metric_.interval_duration = duration;
  if (is_before_fcp_)
    primary_metric_.pre_fcp_aggregate += duration;

  // Compute all the dependent metrics, after finding which bucket we're in
  // for UMA data.
  size_t bucket_index = bucket_thresholds().size();
  for (size_t i = 0; i < bucket_index; ++i) {
    if (duration < bucket_thresholds()[i]) {
      bucket_index = i;
    }
  }

  for (auto& record : main_frame_percentage_records_) {
    unsigned percentage =
        (unsigned)floor(record.interval_duration.InMicrosecondsF() * 100.0 /
                        duration.InMicrosecondsF());
    record.uma_counters_per_bucket[bucket_index]->Count(percentage);
  }

  // Record here to avoid resetting the ratios before this data point is
  // recorded.
  UpdateEventTimeAndRecordEventIfNeeded();

  // Reset for the next frame.
  ResetAllMetrics();
}

void LocalFrameUkmAggregator::UpdateEventTimeAndRecordEventIfNeeded() {
  // TODO(schenney) Adjust the mean sample interval so that we do not
  // get our events throttled by the UKM system. For M-73 only 1 in 212
  // events are being sent.
  if (!frames_to_next_event_) {
    RecordEvent();
    frames_to_next_event_ += SampleFramesToNextEvent();
  }
  DCHECK_GT(frames_to_next_event_, 0u);
  --frames_to_next_event_;
}

void LocalFrameUkmAggregator::RecordEvent() {
#define CASE_FOR_ID(name)                                                 \
  case k##name:                                                           \
    builder.Set##name(absolute_record.interval_duration.InMicroseconds()) \
        .Set##name##Percentage(percentage);                               \
    break

  ukm::builders::Blink_UpdateTime builder(source_id_);
  builder.SetMainFrame(primary_metric_.interval_duration.InMicroseconds());
  builder.SetMainFrameIsBeforeFCP(is_before_fcp_);
  for (unsigned i = 0; i < (unsigned)kCount; ++i) {
    auto& absolute_record = absolute_metric_records_[i];
    auto& percentage_record = main_frame_percentage_records_[i];
    unsigned percentage = (unsigned)floor(
        percentage_record.interval_duration.InMicrosecondsF() * 100.0 /
        primary_metric_.interval_duration.InMicrosecondsF());
    switch (static_cast<MetricId>(i)) {
      CASE_FOR_ID(Compositing);
      CASE_FOR_ID(CompositingCommit);
      CASE_FOR_ID(IntersectionObservation);
      CASE_FOR_ID(Paint);
      CASE_FOR_ID(PrePaint);
      CASE_FOR_ID(StyleAndLayout);
      CASE_FOR_ID(Style);
      CASE_FOR_ID(Layout);
      CASE_FOR_ID(ForcedStyleAndLayout);
      CASE_FOR_ID(ScrollingCoordinator);
      CASE_FOR_ID(HandleInputEvents);
      CASE_FOR_ID(Animate);
      CASE_FOR_ID(UpdateLayers);
      CASE_FOR_ID(ProxyCommit);
      case kCount:
      case kMainFrame:
        NOTREACHED();
        break;
    }
  }
  builder.Record(recorder_);
#undef CASE_FOR_ID
}

void LocalFrameUkmAggregator::ResetAllMetrics() {
  primary_metric_.reset();
  for (auto& record : absolute_metric_records_)
    record.reset();
  for (auto& record : main_frame_percentage_records_)
    record.reset();
}

unsigned LocalFrameUkmAggregator::SampleFramesToNextEvent() {
  // Return the test interval if set
  if (frames_to_next_event_for_test_)
    return frames_to_next_event_for_test_;

  // Sample from an exponential distribution to give a poisson distribution
  // of samples per time unit. In this case, a mean of one sample per
  // mean_milliseconds_between_samples_. The exponential distribution tends
  // to give more samples in the range (0, mean) than in the range
  // [mean, infinity). Intuitively, the (0, mean) is bounded and can only
  // influence the mean so much, while the [mean, infinity) range is infinite
  // and can generate some long interval times (though there is less than 1%
  // chance of an interval mofre than 5 * mean).
  // RandDouble() returns [0,1), but we need (0,1]. If RandDouble() is
  // uniformly random, so is 1-RandDouble(), so use it to adjust the range.
  // When RandDouble returns 0.0, as it could, we will get a float_sample of
  // 0, causing underflow in UpdateEventTimeAndRecordIfNeeded. So rejection
  // sample until we get a positive count.
  double float_sample = 0;
  do {
    float_sample =
        -(mean_frames_between_samples_ * std::log(1.0 - base::RandDouble()));
  } while (float_sample == 0);
  // float_sample is positive, so we don't need to worry about underflow.
  // But with extremely low probability we might end up with a super high
  // sample. That's OK because it just means we'll stop reporting metrics
  // for that session.
  return (unsigned)std::ceil(float_sample);
}

bool LocalFrameUkmAggregator::AllMetricsAreZero() {
  if (primary_metric_.interval_duration.InMicroseconds())
    return false;
  for (auto& record : absolute_metric_records_) {
    if (record.interval_duration.InMicroseconds()) {
      return false;
    }
  }
  return true;
}

}  // namespace blink
