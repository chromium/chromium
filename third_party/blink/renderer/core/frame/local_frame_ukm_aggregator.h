// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_

#include "cc/metrics/frame_sequence_tracker_collection.h"
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
// It constructs and takes ownership of the UMA counters when constructed
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
// A UKM event is generated according to a sampling strategy, with the goal
// being to choose one frame to report before First Contentful Paint and
// one frame to report during the subsequent document lifetime. We maintain
// a copy of the current sample, and randomly choose to update it on each frame
// such that any given frame is equally likely to be the final sample.
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
//   aggregator->RecordEndOfFrameMetrics(start, end, trackers);
//   // This records a primary sample and the sub-metrics that depend on it.
//   // It may generate an event. trackers is a bit encoding of the active frame
//.  // sequence trackers, informing us of why the BeginMainFrame was requested.
//
// In the example above, the event name is "my_event". It will measure 4
// metrics:
//   "primary_metric",
//   "sub_metric1",
//   "sub_metric2",
//   "sub_metric3"
//
// It will report 4 UMA values:
//   "primary_uma_counter",
//   "sub_uma_metric1", "sub_uma_metric2", "sub_uma_metric3"
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
  // below. For every metric name added here, add an entry in the array in
  // metrics_data() below.
  enum MetricId {
    kCompositingAssignments,
    kCompositingCommit,
    kCompositingInputs,
    kImplCompositorCommit,
    kIntersectionObservation,
    kPaint,
    kPrePaint,
    kStyle,
    kLayout,
    kForcedStyleAndLayout,
    kHitTestDocumentUpdate,
    kHandleInputEvents,
    kAnimate,
    kUpdateLayers,
    kWaitForCommit,
    kCount,
    kMainFrame
  };

  typedef struct MetricInitializationData {
    const char* const name;
    bool has_uma;
  } MetricInitializationData;

 private:
  friend class LocalFrameUkmAggregatorTest;

  // Primary metric name
  static const char* primary_metric_name() { return "MainFrame"; }

  // Add an entry in this array every time a new metric is added.
  static base::span<const MetricInitializationData> metrics_data() {
    static const MetricInitializationData data[] = {
        {"CompositingAssignments", true},
        {"CompositingCommit", true},
        {"CompositingInputs", true},
        {"ImplCompositorCommit", true},
        {"IntersectionObservation", true},
        {"Paint", true},
        {"PrePaint", true},
        {"Style", true},
        {"Layout", true},
        {"ForcedStyleAndLayout", true},
        {"HitTestDocumentUpdate", true},
        {"HandleInputEvents", true},
        {"Animate", true},
        {"UpdateLayers", false},
        {"WaitForCommit", true}};
    static_assert(base::size(data) == kCount, "Metrics data mismatch");
    return data;
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
  ~LocalFrameUkmAggregator();

  // Create a scoped timer with the index of the metric. Note the index must
  // correspond to the matching index in metric_names.
  ScopedUkmHierarchicalTimer GetScopedTimer(size_t metric_index);

  // Record a main frame time metric, that also computes the ratios for the
  // sub-metrics and generates UMA samples. UKM is only reported when
  // BeginMainFrame() had been called. All counters are cleared when this method
  // is called. trackers is a bit encoding of the active frame sequence
  // trackers, telling us the reasons for requesting a BeginMainFrame.
  void RecordEndOfFrameMetrics(base::TimeTicks start,
                               base::TimeTicks end,
                               cc::ActiveFrameSequenceTrackers trackers);

  // Record a sample for a sub-metric. This should only be used when
  // a ScopedUkmHierarchicalTimer cannot be used (such as when the timed
  // interval does not fall inside a single calling function).
  void RecordSample(size_t metric_index,
                    base::TimeTicks start,
                    base::TimeTicks end);

  // Record a sample for the impl-side compositor processing.
  // - requested is the time the renderer proxy requests a commit
  // - started is the time the impl thread begins processing the request
  // - completed is the time the renderer proxy receives notification that the
  //   commit is complete.
  void RecordImplCompositorSample(base::TimeTicks requested,
                                  base::TimeTicks started,
                                  base::TimeTicks completed);

  // Mark the beginning of a main frame update.
  void BeginMainFrame();

  // Inform the aggregator that we have reached First Contentful Paint.
  // The UKM event for the pre-FCP period will be recorded and UMA for
  // aggregated contributions to FCP are reported if are_painting_main_frame
  // is true.
  void DidReachFirstContentfulPaint(bool are_painting_main_frame);

  bool InMainFrameUpdate() { return in_main_frame_update_; }

  // Populate a BeginMainFrameMetrics structure with the latency numbers for
  // the most recent frame. Must be called when within a main frame update.
  // That is, after calling BeginMainFrame and before calling
  // RecordEndOfFrameMetrics.
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics();

 private:
  struct AbsoluteMetricRecord {
    std::unique_ptr<CustomCountHistogram> pre_fcp_uma_counter;
    std::unique_ptr<CustomCountHistogram> post_fcp_uma_counter;
    std::unique_ptr<CustomCountHistogram> uma_aggregate_counter;

    // Accumulated at each sample, then reset with a call to
    // RecordEndOfFrameMetrics.
    base::TimeDelta interval_duration;

    // Accumulated at each sample when within a BeginMainFrame,
    // reset with a call to RecordEndOfFrameMetrics.
    base::TimeDelta main_frame_duration;

    // Accumulated at each sample up to the time of First Contentful Paint.
    base::TimeDelta pre_fcp_aggregate;

    void reset();
  };

  struct SampleToRecord {
    base::TimeDelta primary_metric_duration;
    Vector<base::TimeDelta> sub_metrics_durations;
    Vector<base::TimeDelta> sub_main_frame_durations;
    cc::ActiveFrameSequenceTrackers trackers;
  };

  void UpdateEventTimeAndUpdateSampleIfNeeded(
      cc::ActiveFrameSequenceTrackers trackers);
  void UpdateSample(cc::ActiveFrameSequenceTrackers trackers);
  void ResetAllMetrics();

  // Reports the current sample to the UKM system. Called on the first main
  // frame update after First Contentful Paint and at destruction. Also resets
  // the frame count.
  void ReportUpdateTimeEvent();

  // Reports the Blink.PageLoad to the UKM system. Called on the first main
  // frame after First Contentful Paint.
  void ReportPreFCPEvent();

  // Implements throttling of the ForcedStyleAndLayoutUMA metric.
  void RecordForcedStyleLayoutUMA(base::TimeDelta& duration);

  // To test event sampling. Controls whether we update the current sample
  // on the next frame, or do not. Values persist until explicitly changed.
  void ChooseNextFrameForTest();
  void DoNotChooseNextFrameForTest();

  // Used to check that we record only for the MainFrame of a document.
  bool AllMetricsAreZero();

  // The caller is the owner of the |clock|. The |clock| must outlive the
  // LocalFrameUkmAggregator.
  void SetTickClockForTesting(const base::TickClock* clock);

  // UKM system data
  const int64_t source_id_;
  ukm::UkmRecorder* const recorder_;
  const base::TickClock* clock_;

  // Event and metric data
  const char* const event_name_;
  AbsoluteMetricRecord primary_metric_;
  Vector<AbsoluteMetricRecord> absolute_metric_records_;

  // The current sample to report. When RecordEvent() is called we
  // check for uniform_random[0,1) < 1 / n where n is the number of frames
  // we have seen (including this one). If true, we replace the sample with
  // the current frame data. The result is a uniformly randomly chosen frame
  // in the period between the frame counter being reset and the recording
  // to the UKM system of the current sample.
  // This process is designed to get maximum utility while only sending 2
  // events per page load, which in turn maximizes client counts.
  SampleToRecord current_sample_;
  unsigned frames_since_last_report_ = 0;

  // Control for the ForcedStyleAndUpdate UMA metric sampling
  unsigned mean_calls_between_forced_style_layout_uma_ = 100;
  unsigned calls_to_next_forced_style_layout_uma_ = 0;

  // Set by BeginMainFrame() and cleared in RecordMEndOfFrameMetrics.
  // Main frame metrics are only recorded if this is true.
  bool in_main_frame_update_ = false;

  // A bitfield maintaining state for first contentful paint.
  enum FCPState { kBeforeFCPSignal, kThisFrameReachedFCP, kHavePassedFCP };
  FCPState fcp_state_ = kBeforeFCPSignal;

  // A bitfield used to control updating the sample for tests.
  enum SampleControlForTest {
    kNoPreference,
    kMustChooseNextFrame,
    kMustNotChooseNextFrame
  };
  SampleControlForTest next_frame_sample_control_for_test_ = kNoPreference;

  DISALLOW_COPY_AND_ASSIGN(LocalFrameUkmAggregator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_LOCAL_FRAME_UKM_AGGREGATOR_H_
