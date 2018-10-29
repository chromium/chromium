// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace ukm {
class UkmRecorder;
}

namespace blink {
class CustomCountHistogram;

// This class aggregaties and records time based UKM and UMA metrics
// for LocalFrameView. The simplest way to use it is via the
// SCOPED_UMA_AND_UKM_HIERARCHICAL_TIMER macro in LocalFrameView combined
// with LocalFrameView::RecordEndOfFrameMetrics.
//
// It takes the following constructor parameters:
// - source_id: UKM Source ID associated with the events.
// - recorder: UkmRecorder which will handle the events
//
// The aggregator manages all of the UKM and UMA names for LocalFrameView.
// It constructs and takes ownership the UMA counters when constructed
// itself. We do this to localize all UMA and UKM metrics in one place, so
// that adding a metric is localized to the cc file of this class, protected
// from errors that might arise when adding names in multiple places.
//
// After the aggregator is created, one can create ScopedUkmHierarchicalTimer
// objects that will measure the time, in microseconds, from creation until
// the object is destroyed for sub-metrics. When destroyed, it will record
// a sample into the aggregator and the current frame's accumulated time for
// that metric, and report UMA values.
//
// See the MetricNames enum below for the set of metrics recorded. Add an
// entry to that enum to add a new metric.
//
// When the primary timed execution completes, this aggregator stores the
// primary time and computes metrics that depend on it. The results are
// aggregated.  UMA metrics are updated at this time. A UKM event is
// generated in one of two situations:
//  - If a sample is added that lies in the next event frequency interval (this
//    will generate an event for the previous interval)
//  - If the aggregator is destroyed (this will generate an event for any
//    remaining samples in the aggregator)
//
// Note that no event is generated if there were no primary samples in an
// interval.
//
// Sample usage (see also SCOPED_UMA_AND_UKM_HIERARCHICAL_TIMER):
//   std::unique_ptr<UkmHierarchicalTimeAggregator> aggregator(
//      new UkmHierarchicalTimeAggregator(
//              GetSourceId(),
//              GetUkmRecorder());
//
//   ...
//   {
//     auto timer =
//         aggregator->GetScopedTimer(static_cast<size_t>(
//             LocalFrameUkmAggregator::MetricNames::kMetric2));
//     ...
//   }
//   // At this point an sample for kMetric2 is recorded.
//   ...
//   // When the primary time completes
//   aggregator->RecordPrimaryMetric(time_delta);
//   // This records a primary sample and the sub-metrics that depend on it.
//   // It may generate an event.
//   ...
//   // Destroying an aggregator will generate an event as well if there were
//   // samples.
//   aggregator.reset();
//
// In the example above, the event name is "my_event". It will measure 14
// metrics:
//   "primary_metric.Average", "primary_metric.WorstCase",
//   "sub_metric1.Average", "sub_metric1.WorstCase",
//   "sub_metric2.Average", "sub_metric2.WorstCase",
//   "sub_metric3.Average", "sub_metric3.WorstCase"
//   "sub_metric1.AverageRatio", "sub_metric1.WorstCaseRatio",
//   "sub_metric2.AverageRatio", "sub_metric2.WorstCaseRatio",
//   "sub_metric3.AverageRatio", "sub_metric3.WorstCaseRation"
//
// It will report 13 UMA values:
//   "primary_uma_counter",
//   "sub_uma_metric1", "sub_uma_metric2", "sub_uma_metric3",
//   "sub_uma_ratio1.LessThan1ms", "sub_uma_ratio1.1msTo5ms",
//   "sub_uma_ratio1.MoreThan5ms", "sub_uma_ratio2.LessThan1ms",
//   "sub_uma_ratio2.1msTo5ms", "sub_uma_ratio2.MoreThan5ms",
//   "sub_uma_ratio3.LessThan1ms", "sub_uma_ratio3.1msTo5ms",
//   "sub_uma_ratio3.MoreThan5ms"
//
// Note that these have to be specified in the appropriate ukm.xml file
// and histograms.xml file. Runtime errors indicate missing or mis-named
// metrics.
//
// If the source_id/recorder changes then a new
// UkmHierarchicalTimeAggregator has to be created.
class CORE_EXPORT LocalFrameUkmAggregator {
 public:
  // Changing these values requires changing the names of metrics specified
  // below. For every metric name added here, add an entry in the
  // metric_strings_ array below.
  enum {
    kCompositing,
    kCompositingCommit,
    kIntersectionObservation,
    kPaint,
    kPrePaint,
    kStyleAndLayout,
    kForcedStyleAndLayout,
    kCount
  };

 private:
  friend class LocalFrameUkmAggregatorTest;

  // Add an entry in this arrray every time a new metric is added.
  static const Vector<String>& metric_strings() {
    // Leaky construction to avoid exit-time destruction.
    static const Vector<String>* strings = new Vector<String>{
        "Compositing", "CompositingCommit", "IntersectionObservation", "Paint",
        "PrePaint",    "StyleAndLayout",    "ForcedStyleAndLayout"};
    return *strings;
  }

  // Modify this array if the UMA ratio metrics should be bucketed in a
  // different way.
  static const Vector<TimeDelta>& bucket_thresholds() {
    // Leaky construction to avoid exit-time destruction.
    static const Vector<TimeDelta>* thresholds = new Vector<TimeDelta>{
        TimeDelta::FromMilliseconds(1), TimeDelta::FromMilliseconds(5)};
    return *thresholds;
  }

 public:
  // This class will start a timer upon creation, which will end when the
  // object is destroyed. Upon destruction it will record a sample into the
  // aggregator that created the scoped timer. It will also record an event
  // into the histogram counter.
  class CORE_EXPORT ScopedUkmHierarchicalTimer {
   public:
    ScopedUkmHierarchicalTimer(ScopedUkmHierarchicalTimer&&);
    ~ScopedUkmHierarchicalTimer();

   private:
    friend class LocalFrameUkmAggregator;

    ScopedUkmHierarchicalTimer(LocalFrameUkmAggregator*, size_t metric_index);

    LocalFrameUkmAggregator* aggregator_;
    const size_t metric_index_;
    const TimeTicks start_time_;

    DISALLOW_COPY_AND_ASSIGN(ScopedUkmHierarchicalTimer);
  };

  LocalFrameUkmAggregator(int64_t source_id, ukm::UkmRecorder*);
  ~LocalFrameUkmAggregator();

  // Create a scoped timer with the index of the metric. Note the index must
  // correspond to the matching index in metric_names.
  ScopedUkmHierarchicalTimer GetScopedTimer(size_t metric_index);

  // Record a primary sample, that also computes the ratios for the
  // sub-metrics and may generate an event.
  void RecordPrimarySample(TimeTicks start, TimeTicks end);

  // Record a sample for a sub-metric. This should only be used when
  // a ScopedUkmHierarchicalTimer cannot be used (such as when the timed
  // interval does not fall inside a single calling function).
  void RecordSample(size_t metric_index, TimeTicks start, TimeTicks end);

 private:
  struct AbsoluteMetricRecord {
    String worst_case_metric_name;
    String average_metric_name;
    std::unique_ptr<CustomCountHistogram> uma_counter;
    TimeDelta total_duration;
    TimeDelta worst_case_duration;
    size_t sample_count = 0u;

    void reset() {
      total_duration = TimeDelta();
      worst_case_duration = TimeDelta();
      sample_count = 0u;
    }
  };

  struct RatioMetricRecord {
    String worst_case_metric_name;
    String average_metric_name;
    Vector<std::unique_ptr<CustomCountHistogram>> uma_counters_per_bucket;
    TimeDelta interval_duration;
    double total_ratio;
    double worst_case_ratio;
    size_t sample_count;

    void reset() {
      interval_duration = TimeDelta();
      total_ratio = 0;
      worst_case_ratio = 0;
      sample_count = 0u;
    }
  };

  void FlushIfNeeded(TimeTicks current_time);
  void Flush(TimeTicks current_time);

  const int64_t source_id_;
  ukm::UkmRecorder* const recorder_;
  const String event_name_;
  const TimeDelta event_frequency_;
  AbsoluteMetricRecord primary_metric_;
  Vector<AbsoluteMetricRecord> absolute_metric_records_;
  Vector<RatioMetricRecord> ratio_metric_records_;
  TimeTicks last_flushed_time_;

  bool has_data_ = false;

  DISALLOW_COPY_AND_ASSIGN(LocalFrameUkmAggregator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_
