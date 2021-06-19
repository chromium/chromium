// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include <memory>

#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "cc/metrics/begin_main_frame_metrics.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace {

inline base::HistogramBase::Sample ToSample(int64_t value) {
  return base::saturated_cast<base::HistogramBase::Sample>(value);
}

}  // namespace

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
    aggregator_->RecordTimerSample(metric_index_, start_time_,
                                   clock_->NowTicks());
  }
}

LocalFrameUkmAggregator::IterativeTimer::IterativeTimer(
    LocalFrameUkmAggregator& aggregator)
    : aggregator_(base::TimeTicks::IsHighResolution() ? &aggregator : nullptr) {
}

LocalFrameUkmAggregator::IterativeTimer::~IterativeTimer() {
  if (aggregator_.get() && metric_index_ != -1)
    Record();
}

void LocalFrameUkmAggregator::IterativeTimer::StartInterval(
    int64_t metric_index) {
  if (aggregator_.get() && metric_index != metric_index_) {
    Record();
    metric_index_ = metric_index;
  }
}

void LocalFrameUkmAggregator::IterativeTimer::Record() {
  DCHECK(aggregator_.get());
  base::TimeTicks now = aggregator_->GetClock()->NowTicks();
  if (metric_index_ != -1) {
    aggregator_->RecordTimerSample(base::saturated_cast<size_t>(metric_index_),
                                   start_time_, now);
  }
  metric_index_ = -1;
  start_time_ = now;
}

void LocalFrameUkmAggregator::AbsoluteMetricRecord::reset() {
  interval_count = 0;
  main_frame_count = 0;
}

LocalFrameUkmAggregator::LocalFrameUkmAggregator(int64_t source_id,
                                                 ukm::UkmRecorder* recorder)
    : source_id_(source_id),
      recorder_(recorder),
      clock_(base::DefaultTickClock::GetInstance()),
      event_name_("Blink.UpdateTime") {
  // All of these are assumed to have one entry per sub-metric.
  DCHECK_EQ(base::size(absolute_metric_records_), metrics_data().size());
  DCHECK_EQ(base::size(current_sample_.sub_metrics_counts),
            metrics_data().size());
  DCHECK_EQ(base::size(current_sample_.sub_main_frame_counts),
            metrics_data().size());

  // Record average and worst case for the primary metric.
  primary_metric_.reset();

  // Define the UMA for the primary metric.
  primary_metric_.pre_fcp_uma_counter = std::make_unique<CustomCountHistogram>(
      "Blink.MainFrame.UpdateTime.PreFCP", 1, 10000000, 50);
  primary_metric_.post_fcp_uma_counter = std::make_unique<CustomCountHistogram>(
      "Blink.MainFrame.UpdateTime.PostFCP", 1, 10000000, 50);
  primary_metric_.uma_aggregate_counter =
      std::make_unique<CustomCountHistogram>(
          "Blink.MainFrame.UpdateTime.AggregatedPreFCP", 1, 10000000, 50);

  // Set up the substrings to create the UMA names
  const char* const uma_preamble = "Blink.";
  const char* const uma_postscript = ".UpdateTime";
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
      StringBuilder uma_name;
      uma_name.Append(uma_preamble);
      uma_name.Append(metric_data.name);
      uma_name.Append(uma_postscript);
      StringBuilder pre_fcp_uma_name;
      pre_fcp_uma_name.Append(uma_name);
      pre_fcp_uma_name.Append(uma_prefcp_postscript);
      absolute_record.pre_fcp_uma_counter =
          std::make_unique<CustomCountHistogram>(
              pre_fcp_uma_name.ToString().Utf8().c_str(), 1, 10000000, 50);
      StringBuilder post_fcp_uma_name;
      post_fcp_uma_name.Append(uma_name);
      post_fcp_uma_name.Append(uma_postfcp_postscript);
      absolute_record.post_fcp_uma_counter =
          std::make_unique<CustomCountHistogram>(
              post_fcp_uma_name.ToString().Utf8().c_str(), 1, 10000000, 50);
      StringBuilder aggregated_uma_name;
      aggregated_uma_name.Append(uma_name);
      aggregated_uma_name.Append(uma_pre_fcp_aggregated_postscript);
      absolute_record.uma_aggregate_counter =
          std::make_unique<CustomCountHistogram>(
              aggregated_uma_name.ToString().Utf8().c_str(), 1, 10000000, 50);
    }

    metric_index++;
  }
}

LocalFrameUkmAggregator::~LocalFrameUkmAggregator() {
  ReportUpdateTimeEvent();
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
  // Do not report hit testing because it is a sub-portion of the other
  // metrics and would result in double counting.
  std::unique_ptr<cc::BeginMainFrameMetrics> metrics_data =
      std::make_unique<cc::BeginMainFrameMetrics>();
  metrics_data->handle_input_events = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kHandleInputEvents)]
          .main_frame_count);
  metrics_data->animate = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kAnimate)]
          .main_frame_count);
  metrics_data->style_update = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kStyle)]
          .main_frame_count);
  metrics_data->layout_update = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kLayout)]
          .main_frame_count);
  metrics_data->prepaint = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kPrePaint)]
          .main_frame_count);
  metrics_data->compositing_assignments = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kCompositingAssignments)]
          .main_frame_count);
  metrics_data->compositing_inputs = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(
                                   MetricId::kCompositingInputs)]
          .main_frame_count);
  metrics_data->paint = base::TimeDelta::FromMicroseconds(
      absolute_metric_records_[static_cast<unsigned>(MetricId::kPaint)]
          .main_frame_count);
  metrics_data->composite_commit = base::TimeDelta::FromMicroseconds(
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

void LocalFrameUkmAggregator::DidReachFirstContentfulPaint(
    bool are_painting_main_frame) {
  DCHECK(fcp_state_ != kHavePassedFCP);

  if (!are_painting_main_frame) {
    DCHECK(AllMetricsAreZero());
    return;
  }

  fcp_state_ = kThisFrameReachedFCP;
}

void LocalFrameUkmAggregator::RecordTimerSample(size_t metric_index,
                                                base::TimeTicks start,
                                                base::TimeTicks end) {
  RecordCountSample(metric_index, (end - start).InMicroseconds());
}

void LocalFrameUkmAggregator::RecordCountSample(size_t metric_index,
                                                int64_t count) {
  // Always use RecordForcedLayoutSample for the kForcedStyleAndLayout
  // metric id.
  DCHECK_NE(metric_index, static_cast<size_t>(kForcedStyleAndLayout));

  bool is_pre_fcp = (fcp_state_ != kHavePassedFCP);

  // Accumulate for UKM and record the UMA
  DCHECK_LT(metric_index, base::size(absolute_metric_records_));
  auto& record = absolute_metric_records_[metric_index];
  record.interval_count += count;
  if (in_main_frame_update_)
    record.main_frame_count += count;
  if (is_pre_fcp)
    record.pre_fcp_aggregate += count;
  // Record the UMA
  // ForcedStyleAndLayout happen so frequently on some pages that we overflow
  // the signed 32 counter for number of events in a 30 minute period. So
  // randomly record with probability 1/100.
  if (record.pre_fcp_uma_counter) {
    if (is_pre_fcp)
      record.pre_fcp_uma_counter->Count(ToSample(count));
    else
      record.post_fcp_uma_counter->Count(ToSample(count));
  }
}

void LocalFrameUkmAggregator::RecordForcedLayoutSample(
    DocumentUpdateReason reason,
    base::TimeTicks start,
    base::TimeTicks end) {
  int64_t count = (end - start).InMicroseconds();
  bool is_pre_fcp = (fcp_state_ != kHavePassedFCP);

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

  auto& record =
      absolute_metric_records_[static_cast<size_t>(kForcedStyleAndLayout)];
  record.interval_count += count;
  if (in_main_frame_update_)
    record.main_frame_count += count;
  if (is_pre_fcp)
    record.pre_fcp_aggregate += count;

  if (should_report_uma_this_frame) {
    if (is_pre_fcp)
      record.pre_fcp_uma_counter->Count(ToSample(count));
    else
      record.post_fcp_uma_counter->Count(ToSample(count));
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
    case DocumentUpdateReason::kForm:
    case DocumentUpdateReason::kInput:
    case DocumentUpdateReason::kInspector:
    case DocumentUpdateReason::kPrinting:
    case DocumentUpdateReason::kSelection:
    case DocumentUpdateReason::kSpatialNavigation:
    case DocumentUpdateReason::kTapHighlight:
      sub_metric = kUserDrivenDocumentUpdate;
      break;

    case DocumentUpdateReason::kAccessibility:
    case DocumentUpdateReason::kBaseColor:
    case DocumentUpdateReason::kDisplayLock:
    case DocumentUpdateReason::kIntersectionObservation:
    case DocumentUpdateReason::kOverlay:
    case DocumentUpdateReason::kPagePopup:
    case DocumentUpdateReason::kSizeChange:
    case DocumentUpdateReason::kSpellCheck:
      sub_metric = kServiceDocumentUpdate;
      break;

    case DocumentUpdateReason::kCanvas:
    case DocumentUpdateReason::kPlugin:
    case DocumentUpdateReason::kSVGImage:
      sub_metric = kContentDocumentUpdate;
      break;

    case DocumentUpdateReason::kScroll:
      sub_metric = kScrollDocumentUpdate;
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
      break;
  }

  if (sub_metric != kCount) {
    auto& sub_record =
        absolute_metric_records_[static_cast<size_t>(sub_metric)];
    sub_record.interval_count += count;
    if (in_main_frame_update_)
      sub_record.main_frame_count += count;
    if (is_pre_fcp)
      sub_record.pre_fcp_aggregate += count;
    if (should_report_uma_this_frame) {
      if (is_pre_fcp)
        sub_record.pre_fcp_uma_counter->Count(ToSample(count));
      else
        sub_record.post_fcp_uma_counter->Count(ToSample(count));
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

void LocalFrameUkmAggregator::RecordEndOfFrameMetrics(
    base::TimeTicks start,
    base::TimeTicks end,
    cc::ActiveFrameSequenceTrackers trackers) {
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

  UpdateEventTimeAndUpdateSampleIfNeeded(trackers);

  // Report the FCP metrics, if necessary, after updating the sample so that
  // the sample has been recorded for the frame that produced FCP.
  if (report_fcp_metrics) {
    ReportPreFCPEvent();
    ReportUpdateTimeEvent();
    // Update the state to prevent future reporting.
    fcp_state_ = kHavePassedFCP;
  }

  // Reset for the next frame.
  ResetAllMetrics();
}

void LocalFrameUkmAggregator::UpdateEventTimeAndUpdateSampleIfNeeded(
    cc::ActiveFrameSequenceTrackers trackers) {
  // Update the frame count first, because it must include this frame
  frames_since_last_report_++;

  // Regardless of test requests, always capture the first frame.
  if (frames_since_last_report_ == 1) {
    UpdateSample(trackers);
    return;
  }

  // Exit if in testing and we do not want to update this frame
  if (next_frame_sample_control_for_test_ == kMustNotChooseNextFrame)
    return;

  // Update the sample with probability 1/frames_since_last_report_, or if
  // testing demand is.
  if ((next_frame_sample_control_for_test_ == kMustChooseNextFrame) ||
      base::RandDouble() < 1 / static_cast<double>(frames_since_last_report_)) {
    UpdateSample(trackers);
  }
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

void LocalFrameUkmAggregator::ReportPreFCPEvent() {
#define RECORD_METRIC(name)                                         \
  {                                                                 \
    auto& absolute_record = absolute_metric_records_[k##name];      \
    if (absolute_record.uma_aggregate_counter) {                    \
      absolute_record.uma_aggregate_counter->Count(                 \
          ToSample(absolute_record.pre_fcp_aggregate));             \
    }                                                               \
    builder.Set##name(ToSample(absolute_record.pre_fcp_aggregate)); \
  }

  ukm::builders::Blink_PageLoad builder(source_id_);
  primary_metric_.uma_aggregate_counter->Count(
      ToSample(primary_metric_.pre_fcp_aggregate));
  builder.SetMainFrame(ToSample(primary_metric_.pre_fcp_aggregate));

  RECORD_METRIC(CompositingAssignments);
  RECORD_METRIC(CompositingCommit);
  RECORD_METRIC(CompositingInputs);
  RECORD_METRIC(ImplCompositorCommit);
  RECORD_METRIC(IntersectionObservation);
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
  RECORD_METRIC(AnchorElementMetricsIntersectionObserver);
  RECORD_METRIC(UpdateViewportIntersection);
  RECORD_METRIC(UserDrivenDocumentUpdate);
  RECORD_METRIC(ServiceDocumentUpdate);
  RECORD_METRIC(ContentDocumentUpdate);
  RECORD_METRIC(ScrollDocumentUpdate);
  RECORD_METRIC(HitTestDocumentUpdate);
  RECORD_METRIC(JavascriptDocumentUpdate);

  builder.Record(recorder_);
#undef RECORD_METRIC
}

void LocalFrameUkmAggregator::ReportUpdateTimeEvent() {
  // Don't report if we haven't generated any samples.
  if (!frames_since_last_report_)
    return;

#define RECORD_METRIC(name)                                      \
  builder.Set##name(current_sample_.sub_metrics_counts[k##name]) \
      .Set##name##BeginMainFrame(                                \
          current_sample_.sub_main_frame_counts[k##name]);

  ukm::builders::Blink_UpdateTime builder(source_id_);
  builder.SetMainFrame(current_sample_.primary_metric_count);
  builder.SetMainFrameIsBeforeFCP(fcp_state_ != kHavePassedFCP);
  builder.SetMainFrameReasons(current_sample_.trackers);
  RECORD_METRIC(CompositingAssignments);
  RECORD_METRIC(CompositingCommit);
  RECORD_METRIC(CompositingInputs);
  RECORD_METRIC(ImplCompositorCommit);
  RECORD_METRIC(IntersectionObservation);
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
  RECORD_METRIC(AnchorElementMetricsIntersectionObserver);
  RECORD_METRIC(UpdateViewportIntersection);
  RECORD_METRIC(UserDrivenDocumentUpdate);
  RECORD_METRIC(ServiceDocumentUpdate);
  RECORD_METRIC(ContentDocumentUpdate);
  RECORD_METRIC(ScrollDocumentUpdate);
  RECORD_METRIC(HitTestDocumentUpdate);
  RECORD_METRIC(JavascriptDocumentUpdate);

  builder.Record(recorder_);
#undef RECORD_METRIC

  // Reset the frames since last report to ensure correct sampling.
  frames_since_last_report_ = 0;
}

void LocalFrameUkmAggregator::ResetAllMetrics() {
  primary_metric_.reset();
  for (auto& record : absolute_metric_records_)
    record.reset();
}

bool LocalFrameUkmAggregator::AllMetricsAreZero() {
  if (primary_metric_.interval_count != 0)
    return false;
  for (auto& record : absolute_metric_records_) {
    if (record.interval_count != 0) {
      return false;
    }
    if (record.main_frame_count != 0) {
      return false;
    }
  }
  return true;
}

void LocalFrameUkmAggregator::ChooseNextFrameForTest() {
  next_frame_sample_control_for_test_ = kMustChooseNextFrame;
}

void LocalFrameUkmAggregator::DoNotChooseNextFrameForTest() {
  next_frame_sample_control_for_test_ = kMustNotChooseNextFrame;
}

}  // namespace blink
