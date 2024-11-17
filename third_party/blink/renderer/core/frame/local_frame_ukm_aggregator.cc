// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include <memory>

#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace {

inline base::HistogramBase::Sample ToSample(int64_t value) {
  return base::saturated_cast<base::HistogramBase::Sample>(value);
}

inline int64_t ApplyBucket(int64_t value) {
  return ukm::GetExponentialBucketMinForCounts1000(value);
}

BASE_FEATURE(kAvoidUnnecessaryForcedLayoutMeasurements,
             "AvoidUnnecessaryForcedLayoutMeasurements",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace

namespace blink {

int64_t LocalFrameUkmAggregator::ApplyBucketIfNecessary(int64_t value,
                                                        unsigned metric_id) {
  if (metric_id >= kIntersectionObservationInternalCount &&
      metric_id <= kIntersectionObservationJavascriptCount) {
    return ApplyBucket(value);
  }
  return value;
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::ScopedUkmHierarchicalTimer(
    scoped_refptr<LocalFrameUkmAggregator> aggregator,
    size_t metric_index,
    const base::TickClock* clock)
    : aggregator_(aggregator),
      metric_index_(metric_index),
      clock_(clock),
      start_time_(aggregator && aggregator->ShouldMeasureMetric(metric_index)
                      ? clock_->NowTicks()
                      : base::TimeTicks()) {
  if (aggregator_ && !start_time_.is_null())
    TRACE_EVENT_BEGIN0("blink", aggregator_->metrics_data()[metric_index].name);
}

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
  if (aggregator_ && !start_time_.is_null()) {
    if (base::TimeTicks::IsHighResolution()) {
      aggregator_->RecordTimerSample(metric_index_, start_time_,
                                     clock_->NowTicks());
    }
    TRACE_EVENT_END1("blink", aggregator_->metrics_data()[metric_index_].name,
                     "preFCP", aggregator_->fcp_state_ == kBeforeFCPSignal);
  }
}

LocalFrameUkmAggregator::IterativeTimer::IterativeTimer(
    LocalFrameUkmAggregator& aggregator)
    : aggregator_(base::TimeTicks::IsHighResolution() ? &aggregator : nullptr) {
}

LocalFrameUkmAggregator::IterativeTimer::~IterativeTimer() {
  if (aggregator_.get())
    Record(aggregator_->ShouldMeasureMetric(metric_index_), false);
}

void LocalFrameUkmAggregator::IterativeTimer::StartInterval(
    int64_t metric_index) {
  if (aggregator_.get() && metric_index != metric_index_) {
    bool should_record_prev_metric =
        aggregator_->ShouldMeasureMetric(metric_index_);
    bool should_record_next_metric =
        aggregator_->ShouldMeasureMetric(metric_index);
    Record(should_record_prev_metric, should_record_next_metric);
    if (should_record_next_metric)
      metric_index_ = metric_index;
  }
}

void LocalFrameUkmAggregator::IterativeTimer::Record(
    bool should_record_prev_metric,
    bool should_record_next_metric) {
  DCHECK(aggregator_.get());
  if (should_record_prev_metric || should_record_next_metric) {
    base::TimeTicks now = aggregator_->GetClock()->NowTicks();
    if (should_record_prev_metric) {
      aggregator_->RecordTimerSample(
          base::saturated_cast<size_t>(metric_index_), start_time_, now);
    }
    start_time_ = now;
  }
  metric_index_ = -1;
}

LocalFrameUkmAggregator::ScopedForcedLayoutTimer::ScopedForcedLayoutTimer(
    LocalFrameUkmAggregator& aggregator,
    DocumentUpdateReason update_reason,
    bool avoid_unnecessary_forced_layout_measurements,
    bool should_report_uma_this_frame,
    bool is_pre_fcp,
    bool record_ukm_for_current_frame)
    : aggregator_(&aggregator),
      update_reason_(update_reason),
      start_time_(!avoid_unnecessary_forced_layout_measurements ||
                          should_report_uma_this_frame || is_pre_fcp ||
                          record_ukm_for_current_frame
                      ? aggregator_->clock_->NowTicks()
                      : base::TimeTicks()),
      avoid_unnecessary_forced_layout_measurements_(
          avoid_unnecessary_forced_layout_measurements),
      should_report_uma_this_frame_(should_report_uma_this_frame),
      is_pre_fcp_(is_pre_fcp),
      record_ukm_for_current_frame_(record_ukm_for_current_frame) {
  aggregator_->BeginForcedLayout();
}

LocalFrameUkmAggregator::ScopedForcedLayoutTimer::~ScopedForcedLayoutTimer() {
  // aggregator_ will be null in a moved-from object.
  if (!aggregator_) {
    return;
  }

  aggregator_->EndForcedLayout(
      update_reason_,
      // start_time_ will be null if we don't need to measure this forced
      // layout, because it won't be reported.
      !start_time_.is_null() ? aggregator_->clock_->NowTicks() - start_time_
                             : base::TimeDelta(),
      avoid_unnecessary_forced_layout_measurements_,
      should_report_uma_this_frame_, is_pre_fcp_);
}

LocalFrameUkmAggregator::ScopedForcedLayoutTimer::ScopedForcedLayoutTimer(
    ScopedForcedLayoutTimer&&) = default;

LocalFrameUkmAggregator::ScopedForcedLayoutTimer&
LocalFrameUkmAggregator::ScopedForcedLayoutTimer::operator=(
    ScopedForcedLayoutTimer&&) = default;

void LocalFrameUkmAggregator::AbsoluteMetricRecord::reset() {
  interval_count = 0;
  main_frame_count = 0;
}

LocalFrameUkmAggregator::LocalFrameUkmAggregator()
    : clock_(base::DefaultTickClock::GetInstance()) {
  // All of these are assumed to have one entry per sub-metric.
  DCHECK_EQ(std::size(absolute_metric_records_), metrics_data().size());
  DCHECK_EQ(std::size(current_sample_.sub_metrics_counts),
            metrics_data().size());
  DCHECK_EQ(std::size(current_sample_.sub_main_frame_counts),
            metrics_data().size());

  // Record average and worst case for the primary metric.
  primary_metric_.reset();

  // Define the UMA for the primary metric.
  primary_metric_.pre_fcp_uma_counter = std::make_unique<CustomCountHistogram>(
      "Blink.MainFrame.UpdateTime.PreFCP", kTimeBasedHistogramMinSample,
      kTimeBasedHistogramMaxSample, kTimeBasedHistogramBucketCount);
  primary_metric_.post_fcp_uma_counter = std::make_unique<CustomCountHistogram>(
      "Blink.MainFrame.UpdateTime.PostFCP", kTimeBasedHistogramMinSample,
      kTimeBasedHistogramMaxSample, kTimeBasedHistogramBucketCount);
  primary_metric_.uma_aggregate_counter =
      std::make_unique<CustomCountHistogram>(
          "Blink.MainFrame.UpdateTime.AggregatedPreFCP",
          kTimeBasedHistogramMinSample, kTimeBasedHistogramMaxSample,
          kTimeBasedHistogramBucketCount);

  // Set up the substrings to create the UMA names
  const char* const uma_prefcp_postscript = ".PreFCP";
  const char* const uma_postfcp_postscript = ".PostFCP";
  const char* const uma_pre_fcp_aggregated_postscript = ".AggregatedPreFCP";

  // Populate all the sub-metrics.
  size_t metric_index = 0;
  for (const MetricInitializationData& metric_data : metrics_data()) {
    // Absolute records report the absolute time for each metric per frame.
    // They also aggregate the time spent in each stage between navigation
    // (LocalFrameView resets) and First Contentful Paint.
    // They have an associated UMA too that we own and allocate here.
    auto& absolute_record = absolute_metric_records_[metric_index];
    absolute_record.reset();
    absolute_record.pre_fcp_aggregate = 0;
    if (metric_data.has_uma) {
      StringBuilder pre_fcp_uma_name;
      pre_fcp_uma_name.Append(metric_data.name);
      pre_fcp_uma_name.Append(uma_prefcp_postscript);
      absolute_record.pre_fcp_uma_counter =
          std::make_unique<CustomCountHistogram>(
              pre_fcp_uma_name.ToString().Utf8().c_str(), 1, 10000000, 50);
      StringBuilder post_fcp_uma_name;
      post_fcp_uma_name.Append(metric_data.name);
      post_fcp_uma_name.Append(uma_postfcp_postscript);
      absolute_record.post_fcp_uma_counter =
          std::make_unique<CustomCountHistogram>(
              post_fcp_uma_name.ToString().Utf8().c_str(), 1, 10000000, 50);
      StringBuilder aggregated_uma_name;
      aggregated_uma_name.Append(metric_data.name);
      aggregated_uma_name.Append(uma_pre_fcp_aggregated_postscript);
      absolute_record.uma_aggregate_counter =
          std::make_unique<CustomCountHistogram>(
              aggregated_uma_name.ToString().Utf8().c_str(), 1, 10000000, 50);
    }

    metric_index++;
  }
}

LocalFrameUkmAggregator::~LocalFrameUkmAggregator() = default;

void LocalFrameUkmAggregator::TransmitFinalSample(int64_t source_id,
                                                  ukm::UkmRecorder* recorder,
                                                  bool is_for_main_frame) {
  ReportUpdateTimeEvent(source_id, recorder);
}

bool LocalFrameUkmAggregator::ShouldMeasureMetric(int64_t metric_id) const {
  if (metric_id < 0 || metric_id > kMainFrame)
    return false;

  // Downsample IntersectionObserver sub-categories. Note that
  // kIntersectionObservation, which measures a single aggregated time for all
  // IntersectionObserver-related work, is unaffected.
  if (metric_id >= kDisplayLockIntersectionObserver &&
      metric_id <= kUpdateViewportIntersection) {
    return frames_since_last_report_ % intersection_observer_sample_period_ ==
           0;
  }
  return true;
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer
LocalFrameUkmAggregator::GetScopedTimer(size_t metric_index) {
  return ScopedUkmHierarchicalTimer(this, metric_index, clock_);
}

LocalFrameUkmAggregator::ScopedForcedLayoutTimer
LocalFrameUkmAggregator::GetScopedForcedLayoutTimer(
    DocumentUpdateReason update_reason) {
  static const bool avoid_unnecessary_forced_layout_measurements =
      base::FeatureList::IsEnabled(kAvoidUnnecessaryForcedLayoutMeasurements);

  // Accumulate for UKM always, but only record the UMA for a subset of cases to
  // avoid overflowing the counters.
  bool should_report_uma_this_frame = !calls_to_next_forced_style_layout_uma_;
  if (should_report_uma_this_frame) {
    calls_to_next_forced_style_layout_uma_ =
        base::RandInt(0, mean_calls_between_forced_style_layout_uma_ * 2);
  } else {
    DCHECK_GT(calls_to_next_forced_style_layout_uma_, 0u);
    --calls_to_next_forced_style_layout_uma_;
  }

  bool is_pre_fcp = (fcp_state_ != kHavePassedFCP);

  return ScopedForcedLayoutTimer(
      *this, update_reason, avoid_unnecessary_forced_layout_measurements,
      should_report_uma_this_frame, is_pre_fcp, record_ukm_for_current_frame_);
}

void LocalFrameUkmAggregator::BeginMainFrame() {
  DCHECK(!in_main_frame_update_);
  in_main_frame_update_ = true;
  request_timestamp_for_current_frame_ = animation_request_timestamp_;
  animation_request_timestamp_.reset();
}

std::unique_ptr<cc::BeginMainFrameMetrics>
LocalFrameUkmAggregator::GetBeginMainFrameMetrics() {
  DCHECK(InMainFrameUpdate());

  // Use the main_frame_percentage_records_ because they are the ones that
  // only count time between the Begin and End of a main frame update.
  // Do not report hit testing because it is a sub-portion of the other
  // metrics and would result in double counting.
  std::unique_ptr<cc::BeginMainFrameMetrics> metrics_data =
      std::make_unique<cc::BeginMainFrameMetrics>();
  metrics_data->handle_input_events = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kHandleInputEvents)]
          .main_frame_count);
  metrics_data->animate = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kAnimate)]
          .main_frame_count);
  metrics_data->style_update = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kStyle)]
          .main_frame_count);
  metrics_data->layout_update = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kLayout)]
          .main_frame_count);
  metrics_data->accessibility = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kAccessibility)]
          .main_frame_count);
  metrics_data->prepaint = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kPrePaint)]
          .main_frame_count);
  metrics_data->compositing_inputs = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kCompositingInputs)]
          .main_frame_count);
  metrics_data->paint = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kPaint)]
          .main_frame_count);
  metrics_data->composite_commit = base::Microseconds(
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kCompositingCommit)]
          .main_frame_count);
  metrics_data->should_measure_smoothness =
      (fcp_state_ >= kThisFrameReachedFCP);
  return metrics_data;
}

void LocalFrameUkmAggregator::SetTickClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
}

void LocalFrameUkmAggregator::DidReachFirstContentfulPaint() {
  if (fcp_state_ == kBeforeFCPSignal)
    fcp_state_ = kThisFrameReachedFCP;
}

void LocalFrameUkmAggregator::RecordTimerSample(size_t metric_index,
                                                base::TimeTicks start,
                                                base::TimeTicks end) {
  RecordCountSample(metric_index, (end - start).InMicroseconds());
}

void LocalFrameUkmAggregator::RecordCountSample(size_t metric_index,
                                                int64_t count) {
  // Always use EndForcedLayout for the kForcedStyleAndLayout metric id.
  DCHECK_NE(metric_index, static_cast<size_t>(kForcedStyleAndLayout));

  bool is_pre_fcp = (fcp_state_ != kHavePassedFCP);

  // Accumulate for UKM and record the UMA
  DCHECK_LT(metric_index, std::size(absolute_metric_records_));
  auto& record = absolute_metric_records_[metric_index];
  record.interval_count += count;
  if (in_main_frame_update_)
    record.main_frame_count += count;
  if (is_pre_fcp)
    record.pre_fcp_aggregate += count;

  // Subsampling these metrics reduced CPU utilization (crbug.com/1295441).
  if (!metrics_subsampler_.ShouldSample(0.001)) {
    return;
  }

  // Record the UMA
  // ForcedStyleAndLayout happen so frequently on some pages that we overflow
  // the signed 32 counter for number of events in a 30 minute period. So
  // randomly record with probability 1/1000.
  if (record.pre_fcp_uma_counter) {
    if (is_pre_fcp)
      record.pre_fcp_uma_counter->Count(ToSample(count));
    else
      record.post_fcp_uma_counter->Count(ToSample(count));
  }
}

void LocalFrameUkmAggregator::RecordEndOfFrameMetrics(
    base::TimeTicks start,
    base::TimeTicks end,
    cc::ActiveFrameSequenceTrackers trackers,
    int64_t source_id,
    ukm::UkmRecorder* recorder) {
  last_frame_request_timestamp_for_test_ =
      request_timestamp_for_current_frame_.value_or(base::TimeTicks());

  const int64_t count = (end - start).InMicroseconds();
  const bool have_valid_metrics =
      // Any of the early outs in LocalFrameView::UpdateLifecyclePhases() will
      // mean we are not in a main frame update. Recording is triggered higher
      // in the stack, so we cannot know to avoid calling this method.
      in_main_frame_update_ &&
      // In tests it's possible to reach here with zero duration.
      (count > 0);

  in_main_frame_update_ = false;
  if (!have_valid_metrics) {
    // Reset for the next frame to start the next recording period with
    // clear counters, even when we did not record anything this frame.
    ResetAllMetrics();
    return;
  }

  if (request_timestamp_for_current_frame_.has_value()) {
    RecordTimerSample(kVisualUpdateDelay,
                      request_timestamp_for_current_frame_.value(), start);
  }

  bool report_as_pre_fcp = (fcp_state_ != kHavePassedFCP);
  bool report_fcp_metrics = (fcp_state_ == kThisFrameReachedFCP);

  // Record UMA
  if (report_as_pre_fcp)
    primary_metric_.pre_fcp_uma_counter->Count(ToSample(count));
  else
    primary_metric_.post_fcp_uma_counter->Count(ToSample(count));

  // Record primary time information
  primary_metric_.interval_count = count;
  if (report_as_pre_fcp)
    primary_metric_.pre_fcp_aggregate += count;

  bool record_ukm_for_next_frame = false;
  UpdateEventTimeAndUpdateSampleIfNeeded(trackers, record_ukm_for_next_frame);

  // Report the FCP metrics, if necessary, after updating the sample so that
  // the sample has been recorded for the frame that produced FCP.
  if (report_fcp_metrics) {
    ReportPreFCPEvent(source_id, recorder);
    ReportUpdateTimeEvent(source_id, recorder);
    // Update the state to prevent future reporting.
    fcp_state_ = kHavePassedFCP;
  }

  // Reset for the next frame.
  ResetAllMetrics();

  record_ukm_for_current_frame_ = record_ukm_for_next_frame;
}

void LocalFrameUkmAggregator::UpdateEventTimeAndUpdateSampleIfNeeded(
    cc::ActiveFrameSequenceTrackers trackers,
    bool& record_ukm_for_next_frame) {
  // Regardless of test requests always capture the first frame, since
  // record_current_ukm_frame_ is initialized to true.
  if (record_ukm_for_current_frame_) {
    UpdateSample(trackers);
  }

  // Update the frame count first, because it must include this frame
  frames_since_last_report_++;

  // Exit if in testing and we do not want to update this frame
  if (next_frame_sample_control_for_test_ == kMustNotChooseNextFrame)
    return;

  // Update the sample with probability 1/frames_since_last_report_, or if
  // testing demand is.
  record_ukm_for_next_frame =
      (next_frame_sample_control_for_test_ == kMustChooseNextFrame) ||
      base::RandDouble() < 1 / static_cast<double>(frames_since_last_report_);
}

void LocalFrameUkmAggregator::UpdateSample(
    cc::ActiveFrameSequenceTrackers trackers) {
  current_sample_.primary_metric_count = primary_metric_.interval_count;
  for (size_t i = 0; i < metrics_data().size(); ++i) {
    current_sample_.sub_metrics_counts[i] =
        absolute_metric_records_[i].interval_count;
    current_sample_.sub_main_frame_counts[i] =
        absolute_metric_records_[i].main_frame_count;
  }
  current_sample_.trackers = trackers;
}

void LocalFrameUkmAggregator::ReportPreFCPEvent(int64_t source_id,
                                                ukm::UkmRecorder* recorder) {
#define RECORD_METRIC(name)                                         \
  {                                                                 \
    auto& absolute_record = absolute_metric_records_[k##name];      \
    if (absolute_record.uma_aggregate_counter) {                    \
      absolute_record.uma_aggregate_counter->Count(                 \
          ToSample(absolute_record.pre_fcp_aggregate));             \
    }                                                               \
    builder.Set##name(ToSample(absolute_record.pre_fcp_aggregate)); \
  }

#define RECORD_BUCKETED_METRIC(name)                               \
  {                                                                \
    auto& absolute_record = absolute_metric_records_[k##name];     \
    if (absolute_record.uma_aggregate_counter) {                   \
      absolute_record.uma_aggregate_counter->Count(                \
          ToSample(absolute_record.pre_fcp_aggregate));            \
    }                                                              \
    builder.Set##name(                                             \
        ToSample(ApplyBucket(absolute_record.pre_fcp_aggregate))); \
  }

  if (!recorder) {
    return;
  }
  ukm::builders::Blink_PageLoad builder(source_id);
  primary_metric_.uma_aggregate_counter->Count(
      ToSample(primary_metric_.pre_fcp_aggregate));
  builder.SetMainFrame(ToSample(primary_metric_.pre_fcp_aggregate));

  RECORD_METRIC(CompositingCommit);
  RECORD_METRIC(CompositingInputs);
  RECORD_METRIC(ImplCompositorCommit);
  RECORD_METRIC(IntersectionObservation);
  RECORD_BUCKETED_METRIC(IntersectionObservationInternalCount);
  RECORD_BUCKETED_METRIC(IntersectionObservationJavascriptCount);
  RECORD_METRIC(Paint);
  RECORD_METRIC(PrePaint);
  RECORD_METRIC(Style);
  RECORD_METRIC(Layout);
  RECORD_METRIC(ForcedStyleAndLayout);
  RECORD_METRIC(HandleInputEvents);
  RECORD_METRIC(Animate);
  RECORD_METRIC(UpdateLayers);
  RECORD_METRIC(WaitForCommit);
  RECORD_METRIC(DisplayLockIntersectionObserver);
  RECORD_METRIC(JavascriptIntersectionObserver);
  RECORD_METRIC(LazyLoadIntersectionObserver);
  RECORD_METRIC(MediaIntersectionObserver);
  RECORD_METRIC(PermissionElementIntersectionObserver);
  RECORD_METRIC(AnchorElementMetricsIntersectionObserver);
  RECORD_METRIC(UpdateViewportIntersection);
  RECORD_METRIC(VisualUpdateDelay);
  RECORD_METRIC(UserDrivenDocumentUpdate);
  RECORD_METRIC(ServiceDocumentUpdate);
  RECORD_METRIC(ContentDocumentUpdate);
  RECORD_METRIC(HitTestDocumentUpdate);
  RECORD_METRIC(JavascriptDocumentUpdate);
  RECORD_METRIC(ParseStyleSheet);
  RECORD_METRIC(Accessibility);
  RECORD_METRIC(PossibleSynchronizedScrollCount2);

  builder.Record(recorder);
#undef RECORD_METRIC
#undef RECORD_BUCKETED_METRIC
}

void LocalFrameUkmAggregator::ReportUpdateTimeEvent(
    int64_t source_id,
    ukm::UkmRecorder* recorder) {
  // Don't report if we haven't generated any samples.
  if (!recorder || !frames_since_last_report_) {
    return;
  }

#define RECORD_METRIC(name)                                      \
  builder.Set##name(current_sample_.sub_metrics_counts[k##name]) \
      .Set##name##BeginMainFrame(                                \
          current_sample_.sub_main_frame_counts[k##name]);

#define RECORD_BUCKETED_METRIC(name)                                          \
  builder.Set##name(ApplyBucket(current_sample_.sub_metrics_counts[k##name])) \
      .Set##name##BeginMainFrame(                                             \
          ApplyBucket(current_sample_.sub_main_frame_counts[k##name]));

  ukm::builders::Blink_UpdateTime builder(source_id);
  builder.SetMainFrame(current_sample_.primary_metric_count);
  builder.SetMainFrameIsBeforeFCP(fcp_state_ != kHavePassedFCP);
  builder.SetMainFrameReasons(current_sample_.trackers);
  RECORD_METRIC(CompositingCommit);
  RECORD_METRIC(CompositingInputs);
  RECORD_METRIC(ImplCompositorCommit);
  RECORD_METRIC(IntersectionObservation);
  RECORD_BUCKETED_METRIC(IntersectionObservationInternalCount);
  RECORD_BUCKETED_METRIC(IntersectionObservationJavascriptCount);
  RECORD_METRIC(Paint);
  RECORD_METRIC(PrePaint);
  RECORD_METRIC(Style);
  RECORD_METRIC(Layout);
  RECORD_METRIC(ForcedStyleAndLayout);
  RECORD_METRIC(HandleInputEvents);
  RECORD_METRIC(Animate);
  RECORD_METRIC(UpdateLayers);
  RECORD_METRIC(WaitForCommit);
  RECORD_METRIC(DisplayLockIntersectionObserver);
  RECORD_METRIC(JavascriptIntersectionObserver);
  RECORD_METRIC(LazyLoadIntersectionObserver);
  RECORD_METRIC(MediaIntersectionObserver);
  RECORD_METRIC(PermissionElementIntersectionObserver);
  RECORD_METRIC(AnchorElementMetricsIntersectionObserver);
  RECORD_METRIC(UpdateViewportIntersection);
  RECORD_METRIC(VisualUpdateDelay);
  RECORD_METRIC(UserDrivenDocumentUpdate);
  RECORD_METRIC(ServiceDocumentUpdate);
  RECORD_METRIC(ContentDocumentUpdate);
  RECORD_METRIC(HitTestDocumentUpdate);
  RECORD_METRIC(JavascriptDocumentUpdate);
  RECORD_METRIC(ParseStyleSheet);
  RECORD_METRIC(Accessibility);
  RECORD_METRIC(PossibleSynchronizedScrollCount2);

  builder.Record(recorder);
#undef RECORD_METRIC
#undef RECORD_BUCKETED_METRIC

  // Reset the frames since last report to ensure correct sampling.
  frames_since_last_report_ = 0;
}

void LocalFrameUkmAggregator::ResetAllMetrics() {
  primary_metric_.reset();
  for (auto& record : absolute_metric_records_)
    record.reset();
  request_timestamp_for_current_frame_.reset();
}

void LocalFrameUkmAggregator::BeginForcedLayout() {
  TRACE_EVENT_BEGIN0("blink", metrics_data()[kForcedStyleAndLayout].name);
}

void LocalFrameUkmAggregator::EndForcedLayout(
    DocumentUpdateReason reason,
    base::TimeDelta duration,
    bool avoid_unnecessary_forced_layout_measurements,
    bool should_report_uma_this_frame,
    bool is_pre_fcp) {
  TRACE_EVENT_END1("blink", metrics_data()[kForcedStyleAndLayout].name,
                   "preFCP", fcp_state_ == kBeforeFCPSignal);

  if (avoid_unnecessary_forced_layout_measurements &&
      !(should_report_uma_this_frame || is_pre_fcp ||
        record_ukm_for_current_frame_)) {
    return;
  }

  int64_t count = duration.InMicroseconds();

  auto& record =
      absolute_metric_records_[static_cast<size_t>(kForcedStyleAndLayout)];
  record.interval_count += count;
  if (in_main_frame_update_) {
    record.main_frame_count += count;
  }
  if (is_pre_fcp) {
    record.pre_fcp_aggregate += count;
  }

  if (should_report_uma_this_frame) {
    if (is_pre_fcp) {
      record.pre_fcp_uma_counter->Count(ToSample(count));
    } else {
      record.post_fcp_uma_counter->Count(ToSample(count));
    }
  }

  // Record a variety of DocumentUpdateReasons as distinct metrics
  // Figure out which sub-metric, if any, we wish to report for UKM.
  MetricId sub_metric = kCount;
  switch (reason) {
    case DocumentUpdateReason::kContextMenu:
    case DocumentUpdateReason::kDragImage:
    case DocumentUpdateReason::kEditing:
    case DocumentUpdateReason::kFindInPage:
    case DocumentUpdateReason::kFocus:
    case DocumentUpdateReason::kFocusgroup:
    case DocumentUpdateReason::kForm:
    case DocumentUpdateReason::kInput:
    case DocumentUpdateReason::kInspector:
    case DocumentUpdateReason::kPrinting:
    case DocumentUpdateReason::kScroll:
    case DocumentUpdateReason::kSelection:
    case DocumentUpdateReason::kSpatialNavigation:
    case DocumentUpdateReason::kTapHighlight:
      sub_metric = kUserDrivenDocumentUpdate;
      break;

    case DocumentUpdateReason::kAccessibility:
    case DocumentUpdateReason::kBaseColor:
    case DocumentUpdateReason::kComputedStyle:
    case DocumentUpdateReason::kDisplayLock:
    case DocumentUpdateReason::kViewTransition:
    case DocumentUpdateReason::kIntersectionObservation:
    case DocumentUpdateReason::kOverlay:
    case DocumentUpdateReason::kPagePopup:
    case DocumentUpdateReason::kPopover:
    case DocumentUpdateReason::kSizeChange:
    case DocumentUpdateReason::kSpellCheck:
    case DocumentUpdateReason::kSMILAnimation:
    case DocumentUpdateReason::kWebAnimation:
      sub_metric = kServiceDocumentUpdate;
      break;

    case DocumentUpdateReason::kCanvas:
    case DocumentUpdateReason::kPlugin:
    case DocumentUpdateReason::kSVGImage:
      sub_metric = kContentDocumentUpdate;
      break;

    case DocumentUpdateReason::kHitTest:
      sub_metric = kHitTestDocumentUpdate;
      break;

    case DocumentUpdateReason::kJavaScript:
      sub_metric = kJavascriptDocumentUpdate;
      break;

    // Do not report main frame because we have it already from
    // in_main_frame_update_ above.
    case DocumentUpdateReason::kBeginMainFrame:
    // No metrics from testing.
    case DocumentUpdateReason::kTest:
    // Don't report if we don't know why.
    case DocumentUpdateReason::kUnknown:
    // TODO(https://crbug.com/336963892): Give prerender a dedicated metric.
    case DocumentUpdateReason::kPrerender:
      break;
  }

  if (sub_metric != kCount) {
    auto& sub_record =
        absolute_metric_records_[static_cast<size_t>(sub_metric)];
    sub_record.interval_count += count;
    if (in_main_frame_update_) {
      sub_record.main_frame_count += count;
    }
    if (is_pre_fcp) {
      sub_record.pre_fcp_aggregate += count;
    }
    if (should_report_uma_this_frame) {
      if (is_pre_fcp) {
        sub_record.pre_fcp_uma_counter->Count(ToSample(count));
      } else {
        sub_record.post_fcp_uma_counter->Count(ToSample(count));
      }
    }
  }
}

void LocalFrameUkmAggregator::RecordImplCompositorSample(
    base::TimeTicks requested,
    base::TimeTicks started,
    base::TimeTicks completed) {
  // Record the time spent waiting for the commit based on requested
  // (which came from ProxyImpl::BeginMainFrame) and started as reported by
  // the impl thread. If started is zero, no time was spent
  // processing. This can only happen if the commit was aborted because there
  // was no change and we did not wait for the impl thread at all. Attribute
  // all time to the compositor commit so as to not imply that wait time was
  // consumed.
  if (started == base::TimeTicks()) {
    RecordTimerSample(kImplCompositorCommit, requested, completed);
  } else {
    RecordTimerSample(kWaitForCommit, requested, started);
    RecordTimerSample(kImplCompositorCommit, started, completed);
  }
}

void LocalFrameUkmAggregator::ChooseNextFrameForTest() {
  next_frame_sample_control_for_test_ = kMustChooseNextFrame;
}

void LocalFrameUkmAggregator::DoNotChooseNextFrameForTest() {
  next_frame_sample_control_for_test_ = kMustNotChooseNextFrame;
}

bool LocalFrameUkmAggregator::IsBeforeFCPForTesting() const {
  return fcp_state_ == kBeforeFCPSignal;
}

void LocalFrameUkmAggregator::OnCommitRequested() {
  // This can't be a DCHECK because this method can be called during the early
  // stages of cc::ProxyMain::BeginMainFrame, before
  // LocalFrameUkmAggregator::BeginMainFrame() has been invoked.
  if (!animation_request_timestamp_.has_value())
    animation_request_timestamp_.emplace(clock_->NowTicks());
}

}  // namespace blink
