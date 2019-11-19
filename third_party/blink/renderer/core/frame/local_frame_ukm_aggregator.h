// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace base {
class TickClock;
}

namespace cc {
struct BeginMainFrameMetrics;
}

namespace ukm {
class UkmRecorder;
}

namespace blink {

// This class aggregaties and records time based UKM and UMA metrics
// for LocalFrameView. The simplest way to use it is via the
// SCOPED_UMA_AND_UKM_TIMER macro combined with
// LocalFrameView::RecordEndOfFrameMetrics.
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
// the object is destroyed for sub-metrics. When destroyed, it may record
// a sample into the aggregator and the current frame's accumulated time for
// that metric, and it always reports UMA values.
//
// See the MetricNames enum below for the set of metrics recorded. Add an
// entry to that enum to add a new metric.
//
// When the primary timed execution completes, this aggregator stores the
// primary time and computes metrics that depend on it. UMA metrics are updated
// at this time.
//
// A UKM event is generated according to a sampling strategy. A record is always
// generated on the first lifecycle update, and then additional samples are
// taken at random frames simulating a poisson process with mean number of
// frames between events of mean_frames_between_samples_. The first primary
// metric recording after the frame count has passed will produce an event with
// all the data for that frame (i.e. the period since the last BeginMainFrame).
//
// Sample usage (see also SCOPED_UMA_AND_UKM_TIMER):
//   std::unique_ptr<UkmHierarchicalTimeAggregator> aggregator(
//      new UkmHierarchicalTimeAggregator(
//              GetSourceId(),
//              GetUkmRecorder());
//   ...
//   {
//     auto timer =
//         aggregator->GetScopedTimer(static_cast<size_t>(
//             LocalFrameUkmAggregator::MetricNames::kMetric2));
//     ...
//   }
//   // At this point data for kMetric2 is recorded.
//   ...
//   // When the primary time completes
//   aggregator->RecordEndOfFrameMetrics(time_delta);
//   // This records a primary sample and the sub-metrics that depend on it.
//   // It may generate an event.
//
// In the example above, the event name is "my_event". It will measure 7
// metrics:
//   "primary_metric",
//   "sub_metric1",
//   "sub_metric2",
//   "sub_metric3"
//   "sub_metric1Percentage",
//   "sub_metric2Percentage",
//   "sub_metric3Percentage"
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

// Defines a UKM that is part of a hierarchical ukm, recorded in
// microseconds equal to the duration of the current lexical scope after
// declaration of the macro. Example usage:
//
// void LocalFrameView::DoExpensiveThing() {
//   SCOPED_UMA_AND_UKM_TIMER(kUkmEnumName);
//   // Do computation of expensive thing
//
// }
//
// |ukm_enum| should be an entry in LocalFrameUkmAggregator's enum of
// metric names (which in turn corresponds to names in from ukm.xml).
#define SCOPED_UMA_AND_UKM_TIMER(aggregator, ukm_enum) \
  auto scoped_ukm_hierarchical_timer =                 \
      aggregator.GetScopedTimer(static_cast<size_t>(ukm_enum));

class CORE_EXPORT LocalFrameUkmAggregator
    : public RefCounted<LocalFrameUkmAggregator> {
 public:
  // Changing these values requires changing the names of metrics specified
  // below. For every metric name added here, add an entry in the
  // metric_strings_ array below.
  enum MetricId {
    kCompositing,
    kCompositingCommit,
    kIntersectionObservation,
    kPaint,
    kPrePaint,
    kStyleAndLayout,  // Remove for M-80
    kStyle,
    kLayout,
    kForcedStyleAndLayout,
    kScrollingCoordinator,
    kHandleInputEvents,
    kAnimate,
    kUpdateLayers,
    kProxyCommit,
    kCount,
    kMainFrame
  };

  typedef struct MetricInitializationData {
    String name;
    bool has_uma;
  } MetricInitializationData;

 private:
  friend class LocalFrameUkmAggregatorTest;

  // Primary metric name
  static const String& primary_metric_name() {
    DEFINE_STATIC_LOCAL(String, primary_name, ("MainFrame"));
    return primary_name;
  }

  // Add an entry in this arrray every time a new metric is added.
  static const Vector<MetricInitializationData>& metrics_data() {
    // Leaky construction to avoid exit-time destruction.
    static const Vector<MetricInitializationData>* data =
        new Vector<MetricInitializationData>{{"Compositing", true},
                                             {"CompositingCommit", true},
                                             {"IntersectionObservation", true},
                                             {"Paint", true},
                                             {"PrePaint", true},
                                             {"StyleAndLayout", true},
                                             {"Style", true},
                                             {"Layout", true},
                                             {"ForcedStyleAndLayout", true},
                                             {"ScrollingCoordinator", true},
                                             {"HandleInputEvents", true},
                                             {"Animate", true},
                                             {"UpdateLayers", false},
                                             {"ProxyCommit", true}};
    return *data;
  }

  // Modify this array if the UMA ratio metrics should be bucketed in a
  // different way.
  static const Vector<base::TimeDelta>& bucket_thresholds() {
    // Leaky construction to avoid exit-time destruction.
    static const Vector<base::TimeDelta>* thresholds =
        new Vector<base::TimeDelta>{base::TimeDelta::FromMilliseconds(1),
                                    base::TimeDelta::FromMilliseconds(5)};
    return *thresholds;
  }

 public:
  // This class will start a timer upon creation, which will end when the
  // object is destroyed. Upon destruction it will record a sample into the
  // aggregator that created the scoped timer. It will also record an event
  // into the histogram counter.
  class CORE_EXPORT ScopedUkmHierarchicalTimer {
    STACK_ALLOCATED();

   public:
    ScopedUkmHierarchicalTimer(ScopedUkmHierarchicalTimer&&);
    ~ScopedUkmHierarchicalTimer();

   private:
    friend class LocalFrameUkmAggregator;

    ScopedUkmHierarchicalTimer(scoped_refptr<LocalFrameUkmAggregator>,
                               size_t metric_index,
                               const base::TickClock* clock);

    scoped_refptr<LocalFrameUkmAggregator> aggregator_;
    const size_t metric_index_;
    const base::TickClock* clock_;
    const base::TimeTicks start_time_;

    DISALLOW_COPY_AND_ASSIGN(ScopedUkmHierarchicalTimer);
  };

  LocalFrameUkmAggregator(int64_t source_id, ukm::UkmRecorder*);
  ~LocalFrameUkmAggregator() = default;

  // Create a scoped timer with the index of the metric. Note the index must
  // correspond to the matching index in metric_names.
  ScopedUkmHierarchicalTimer GetScopedTimer(size_t metric_index);

  // Record a main frame time metric, that also computes the ratios for the
  // sub-metrics and generates UMA samples. UKM is only reported when
  // BeginMainFrame() had been called. All counters are cleared when this method
  // is called.
  void RecordEndOfFrameMetrics(base::TimeTicks start, base::TimeTicks end);

  // Record a sample for a sub-metric. This should only be used when
  // a ScopedUkmHierarchicalTimer cannot be used (such as when the timed
  // interval does not fall inside a single calling function).
  void RecordSample(size_t metric_index,
                    base::TimeTicks start,
                    base::TimeTicks end);

  // Mark the beginning of a main frame update.
  void BeginMainFrame();

  // Inform the aggregator that we have reached First Contentful Paint.
  // The UKM event reports this and UMA for aggregated contributions to
  // FCP are reported if are_painting_main_frame is true.
  void DidReachFirstContentfulPaint(bool are_painting_main_frame);

  bool InMainFrameUpdate() { return in_main_frame_update_; }

  // Populate a BeginMainFrameMetrics structure with the latency numbers for
  // the most recent frame. Must be called when within a main frame update.
  // That is, after calling BeginMainFrame and before calling
  // RecordEndOfFrameMetrics.
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics();

  // The caller is the owner of the |clock|. The |clock| must outlive the
  // LocalFrameUkmAggregator.
  void SetTickClockForTesting(const base::TickClock* clock);

 private:
  struct AbsoluteMetricRecord {
    std::unique_ptr<CustomCountHistogram> uma_counter;
    std::unique_ptr<CustomCountHistogram> pre_fcp_uma_counter;
    std::unique_ptr<CustomCountHistogram> post_fcp_uma_counter;
    std::unique_ptr<CustomCountHistogram> uma_aggregate_counter;

    // Accumulated at each sample, then reset with a call to
    // RecordEndOfFrameMetrics.
    base::TimeDelta interval_duration;
    base::TimeDelta pre_fcp_aggregate;

    void reset() { interval_duration = base::TimeDelta(); }
  };

  struct MainFramePercentageRecord {
    Vector<std::unique_ptr<CustomCountHistogram>> uma_counters_per_bucket;

    // Accumulated at each sample, then reset with a call to
    // RecordEndOfFrameMetrics.
    base::TimeDelta interval_duration;

    void reset() { interval_duration = base::TimeDelta(); }
  };

  void UpdateEventTimeAndRecordEventIfNeeded();
  void RecordEvent();
  void ResetAllMetrics();
  unsigned SampleFramesToNextEvent();

  // Implements throttling of the ForcedStyleAndLayoutUMA metric.
  void RecordForcedStyleLayoutUMA(base::TimeDelta& duration);

  // To test event sampling. This and all future intervals will be the given
  // frame count, until this is called again.
  void FramesToNextEventForTest(unsigned num_frames) {
    frames_to_next_event_for_test_ = num_frames;
  }

  // Used to check that we only for the MainFrame of a document.
  bool AllMetricsAreZero();

  // UKM system data
  const int64_t source_id_;
  ukm::UkmRecorder* const recorder_;
  const base::TickClock* clock_;

  // Event and metric data
  const String event_name_;
  AbsoluteMetricRecord primary_metric_;
  Vector<AbsoluteMetricRecord> absolute_metric_records_;
  Vector<MainFramePercentageRecord> main_frame_percentage_records_;

  // Sampling control. Currently we sample a bit more than every 30s assuming we
  // are achieving 60fps. Better to sample less rather than more given our data
  // is already beyond the throtting threshold.
  unsigned mean_frames_between_samples_ = 2000;
  unsigned frames_to_next_event_ = 0;

  // Control for the ForcedStyleAndUpdate UMA metric sampling
  unsigned mean_calls_between_forced_style_layout_uma_ = 100;
  unsigned calls_to_next_forced_style_layout_uma_ = 0;

  // Test data, used for SampleFramesToNextEvent if present
  unsigned frames_to_next_event_for_test_ = 0;

  // Set by BeginMainFrame() and cleared in RecordMEndOfFrameMetrics.
  // Main frame metrics are only recorded if this is true.
  bool in_main_frame_update_ = false;

  // Record whether or not it is before the First Contentful Paint.
  bool is_before_fcp_ = true;

  DISALLOW_COPY_AND_ASSIGN(LocalFrameUkmAggregator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_
