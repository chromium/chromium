// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_

#include <type_traits>

#include "base/feature_list.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/effective_visual_size_result.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callbacks.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace gfx {
class Rect;
class RectF;
class Size;
}  // namespace gfx

namespace blink {
class PaintTimingDetector;

// Kill switch for filtering entropy check on CORS. When enabled, the entropy
// check only applies to images that are CORS-same-origin.
CORE_EXPORT BASE_DECLARE_FEATURE(kLcpEntropyGatedOnCors);

// `LargestContentfulPaintCalculator` is responsible for tracking the largest
// image and the largest text paints, creating `LargestContentfulPaint` entries
// when a new web-exposed candidate is found, and maintaining the data sent to
// metrics.
class CORE_EXPORT LargestContentfulPaintCalculator final
    : public GarbageCollected<LargestContentfulPaintCalculator> {
 public:
  class Delegate : public GarbageCollectedMixin {
   public:
    virtual ~Delegate() = default;

    // Called when the `LatestLcpDetails()` have changed. The `Delegate` is
    // responsible for pushing the updated metrics to UKM.
    virtual void OnLcpMetricsForReportingChanged() = 0;

    // Returns true iff the calculator is associated with a hard navigation.
    //
    // TODO(crbug.com/454082771): This exists because there are some differences
    // between hard LCP and ICP, e.g. the paint timestamp used for metrics for
    // animated images and whether or not traces are emitted for metrics
    // candidates. This should eventually be removed or replaced with more
    // specific methods.
    virtual bool IsHardNavigation() const = 0;
  };

  // Helper class for tracking a single pair of image and text LCP candidates.
  // Used to pick a per-frame largest text and image candidate to feed into the
  // rest of the LCP algorithm.
  class LcpCandidates : public GarbageCollected<LcpCandidates> {
   public:
    LcpCandidates();

    // Updates the image or text candidate with the given record if it's larger
    // than the current candidate of the same type.
    void MaybeUpdateCandidate(ImageRecord*);
    void MaybeUpdateCandidate(TextRecord*);

    template <typename T>
      requires std::is_same_v<T, ImageRecord>
    T* Candidate() const {
      return image_candidate_.Get();
    }

    template <typename T>
      requires std::is_same_v<T, TextRecord>
    T* Candidate() const {
      return text_candidate_.Get();
    }

    void Trace(Visitor* visitor) const;

   private:
    Member<ImageRecord> image_candidate_;
    Member<TextRecord> text_candidate_;
  };

  // `PendingCandidateData` encapsulates the result of computing the new global
  // LCP candidates (new largest image, largest text, and web-exposed entry)
  // from the `LcpCandidates` produced in an animation frame.
  struct PendingCandidateData {
    STACK_ALLOCATED();

   public:
    // Callback to run at presentation time to update metrics and emit metrics
    // trace events.
    OptionalPaintTimingCallback metrics_callback;
    // Non-null if this frame produced a new web-exposed LCP candidate.
    LargestContentfulPaint* lcp_entry = nullptr;
    // Trace data corresponding to `lcp_entry` (non-null iff `lcp_entry` is
    // non-null and tracing is enabled).
    std::unique_ptr<TracedValue> lcp_trace_data;
  };

  // Computes the effective size of an image, downsizing the size of images with
  // low intrinsic size. `EffectiveVisualSizeResult` encapsulates extra details
  // about the computation, such as whether the minimum entropy was met and
  // whether the viewport was covered, which are used by downstream algorithms
  // to filter out invalid candidates.
  //
  // See also:
  // https://www.w3.org/TR/largest-contentful-paint/#sec-effective-visual-size
  static EffectiveVisualSizeResult ComputeEffectiveVisualSize(
      const LayoutObject&,
      const MediaTiming&,
      const gfx::Rect& image_border,
      const gfx::RectF& mapped_visual_rect,
      const gfx::Size& intrinsic_size,
      uint64_t viewport_area,
      const PaintTimingDetector&);

  LargestContentfulPaintCalculator(WindowPerformance*, Delegate*);

  LargestContentfulPaintCalculator(const LargestContentfulPaintCalculator&) =
      delete;
  LargestContentfulPaintCalculator& operator=(
      const LargestContentfulPaintCalculator&) = delete;

  const LargestContentfulPaintDetails& LatestLcpDetails() const {
    return latest_lcp_details_;
  }

  // Updates the metrics and web-exposed LCP candidates based on a single pair
  // of image and text candidates, which represent the largest eligible image
  // and text elements presented in the frame. Returns `PendingCandidateData`
  // containing the presentation callback for metrics and the newly created
  // `LargestContentfulPaint` entry (if any).
  PendingCandidateData MaybeUpdateLargestContentfulPaint(LcpCandidates*);

  // Flushes the pending largest text and largest image candidates to metrics
  // and invokes `Delegate::OnLcpMetricsForReportingChanged()`, if needed.
  //
  // TODO(crbug.com/457794552): This method should be private or inlined into
  // the call site, but this is needed for testing `largest_pending_image_`
  // removal, since that relies on an update to fall back to the
  // `largest_presented_image_details_`. We should instead schedule a separate
  // PaintTimingCallback.
  void MaybeFlushMetricsCandidates();

  // Returns true iff the `ImageRecord` is an eligible LCP candidate, which is
  // true as long as the image is not covering the viewport and, if the image
  // is CORS-same-origin, the minimum entropy requirement has been met.
  // `ImageRecord` is required to have a non-zero size. Note that this differs
  // from `ShouldTrackForPaintTiming()` which also takes into account the size
  // of the current LCP candidate.
  bool IsEligibleForLcp(const ImageRecord&) const;

  // Returns true iff the `TextRecord` is an eligible LCP candidate, which is
  // true as long as the candidate has non-zero size.
  bool IsEligibleForLcp(const TextRecord&) const;

  // Returns true iff the given `ImageRecord` is larger than the current
  // largest image candidate.
  bool IsNewLargestCandidate(const ImageRecord& record) const {
    return record.EffectiveVisualSize() > largest_painted_image_size_;
  }

  // Returns true iff the given `TextRecord` is larger than the current largest
  // text candidate.
  bool IsNewLargestCandidate(const TextRecord& record) const {
    return record.EffectiveVisualSize() > largest_painted_text_size_;
  }

  // Called when an image is painted for the first time, regardless of whether
  // or not it's sufficiently loaded enough to be considered for paint timing.
  // The LCP algorithm assumes such images will be painted, and if the user
  // abandons the page before the image has finished loading and the image is
  // the LCP candidate, the page load isn't reported to UKM.
  void OnImageFirstPaint(ImageRecord*);

  // Called when an image is removed from the DOM. Clears the
  // `largest_pending_image_` if that was removed.
  void OnImageRemoved(const LayoutObject& object, const MediaTiming* timing);

  void Trace(Visitor* visitor) const;

 private:
  struct PendingMetricsData;

  // `PaintTimingCallback` invoked to flush pending metrics candidates.
  void FlushPendingMetricsDataOnFramePresented(
      PendingMetricsData*,
      const base::TimeTicks& presentation_timestamp,
      const DOMPaintTimingInfo&,
      uint64_t navigation_id);

  // Commits the pending image or text metrics candidate, updating internal
  // state in preparation for flushing metrics values.
  void CommitPendingMetricsImageCandidate(
      PendingMetricsData*,
      const base::TimeTicks& presentation_timestamp);
  void CommitPendingMetricsTextCandidate(
      PendingMetricsData*,
      const base::TimeTicks& presentation_timestamp);

  // Returns a `LargestImagePaintDetails` created based on the given
  // `ImageRecord`, with no presentation time set.
  LargestImagePaintDetails CreateLargestImagePaintDetails(
      const ImageRecord&) const;

  // Updates `latest_lcp_details_` with either `largest_pending_image_` data
  // (if non-null) or `largest_presented_image_details_` (otherwise), if
  // needed. Emits metrics trace events and returns true if the metrics values
  // were updated; returns false if metrics weren't updated.
  bool UpdateMetricsIfLargestPendingOrPresentedImageChanged();

  // Updates `latest_lcp_details_` with the `largest_presented_text_details_` if
  // needed. Emits metrics trace events and returns true if the metrics values
  // were updated; returns false if metrics weren't updated.
  bool UpdateMetricsIfLargestPresentedTextChanged();

  // Returns a `LargestContentfulPaint` based on the given `ImageRecord` or
  // `TextRecord`, with the presentation time and `DOMPaintTimingInfo` unset.
  LargestContentfulPaint* CreateLargestContentfulPaintEntry(const ImageRecord&);
  LargestContentfulPaint* CreateLargestContentfulPaintEntry(const TextRecord&);

  // Creates a `TracedValue` populated with the web-exposed LCP candidate trace
  // fields. Returns nullptr if tracing is not enabled.
  std::unique_ptr<TracedValue> CreateWebExposedLcpCandidateTraceData(
      const PaintTimingRecord&);

  // Emits a candidate trace event with the given `TracedValue` computed at
  // paint time.
  void ReportMetricsCandidateToTrace(std::unique_ptr<TracedValue>,
                                     const char* trace_event_name,
                                     base::TimeTicks presentation_time,
                                     unsigned candidate_index);

  // Returns the size of the current LCP candidate for performance timeline,
  // which is either emitted or pending presentation time.
  uint64_t WebExposedCandidateSize() const {
    return std::max(largest_painted_image_size_, largest_painted_text_size_);
  }

  Member<WindowPerformance> window_performance_;

  // TODO(crbug.com/454082773): These values are updated at presentation time in
  // `MaybeUpdateLargestContentfulPaint`. Change this to run at paint time.
  uint64_t largest_painted_image_size_ = 0u;
  uint64_t largest_painted_text_size_ = 0u;

  unsigned web_exposed_candidate_count_ = 0;
  unsigned ukm_largest_image_candidate_count_ = 0;
  unsigned ukm_largest_text_candidate_count_ = 0;

  // The `LargestContentfulPaintDetails` reported to metrics. Combines
  // `largest_presented_text_details_` and either `largest_pending_image_` (if
  // non-null) or `largest_presented_image_details_` (otherwise).
  LargestContentfulPaintDetails latest_lcp_details_;

  // The largest presented image and text details so far.
  LargestImagePaintDetails largest_presented_image_details_;
  LargestTextPaintDetails largest_presented_text_details_;
  std::unique_ptr<TracedValue> largest_presented_image_trace_data_;
  std::unique_ptr<TracedValue> largest_presented_text_trace_data_;

  Member<Delegate> delegate_;

  // The largest image that has been encountered, but not necessarily loaded,
  // painted, or presented yet. This is used to prevent recording LCP for pages
  // that are unloaded when the largest image is still pending.
  //
  // TODO(crbug.com/449779010): This is currently only used for hard navs, but
  // soft navs should also have this behavior.
  Member<ImageRecord> largest_pending_image_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
