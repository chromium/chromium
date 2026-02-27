// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_

#include "base/feature_list.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// Kill switch for Soft Nav/LCP trace events.
BASE_DECLARE_FEATURE(kSoftNavigationTraceEvents);

class ImageRecord;
class PaintTimingRecord;
class TextRecord;

// `LargestContentfulPaintCalculator` is responsible for tracking the largest
// image and the largest text paints and notifying its `Delegate` whenever a new
// PerformanceEntry entry should be dispatched, as well as maintaintaining the
// data sent to metrics.
class CORE_EXPORT LargestContentfulPaintCalculator final
    : public GarbageCollected<LargestContentfulPaintCalculator> {
 public:
  class Delegate : public GarbageCollectedMixin {
   public:
    virtual ~Delegate() = default;

    // Called when a PerformanceEntry for a new largest paint candidate should
    // be emitted. The `Delegate` is responsible for emitting an entry of the
    // appropriate type.
    virtual void EmitLcpPerformanceEntry(
        const DOMPaintTimingInfo& paint_timing_info,
        uint64_t paint_size,
        base::TimeTicks load_time,
        const AtomicString& id,
        const String& url,
        Element* element) = 0;

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

  LargestContentfulPaintCalculator(WindowPerformance*, Delegate*);

  LargestContentfulPaintCalculator(const LargestContentfulPaintCalculator&) =
      delete;
  LargestContentfulPaintCalculator& operator=(
      const LargestContentfulPaintCalculator&) = delete;

  const LargestContentfulPaintDetails& LatestLcpDetails() const {
    return latest_lcp_details_;
  }

  void MaybeRecordRemovedCandidateUseCounter(const ImageRecord&);
  void MaybeRecordRemovedCandidateUseCounter(const TextRecord&);

  // Flushes the pending largest text and largest image candidates to metrics
  // and performance timeline and invokes the relevant `Delegate` callbacks, if
  // needed.
  void MaybeFlushCandidates();

  // Updates the largest text candidate if the given `TextRecord` is larger.
  // Candidates are not emitted to the performance timeline until
  // `UpdateWebExposedLargestContentfulText()` is called, and metrics are not
  // updated until `NotifyMetricsIfLargestTextPaintChanged()` is called.
  void MaybeUpdateLargestText(TextRecord*);

  // Updates the largest painted image candidate if the given `ImageRecord` is
  // larger. Candidates are not emitted to the performance timeline until
  // `UpdateWebExposedLargestContentfulImage()` is called, and metrics are not
  // updated until `NotifyMetricsIfLargestImagePaintChanged()` is called.
  void MaybeUpdateLargestPaintedImage(ImageRecord*);

  // Returns true iff an image of `size` should be tracked for computing LCP.
  bool IsImageNeededForLcp(uint64_t size) const;

  // Called when an image is painted for the first time, regardless of whether
  // or not it's sufficiently loaded enough to be considered for paint timing.
  // The LCP algorithm assumes such images will be painted, and if the user
  // abandons the page before the image has finished loading and the image is
  // the LCP candidate, the page load isn't reported to UKM.
  void OnImageFirstPaint(ImageRecord*);

  // Called when a pending image, one that has been painted but whose paint and
  // presentation times are not yet set, is removed from the DOM.
  void OnPendingImageRemoved(ImageRecord* record);

  void Trace(Visitor* visitor) const;

  ImageRecord* LargestPaintedOrPendingImageForTest() const {
    return LargestPaintedOrPendingImage();
  }
  ImageRecord* LargestPaintedImageForTest() const {
    return largest_painted_image_;
  }
  TextRecord* LargestTextForTest() const { return largest_text_; }

 private:
  friend class LargestContentfulPaintCalculatorTest;

  bool UpdateMetricsIfLargestImagePaintChanged();
  bool UpdateMetricsIfLargestTextPaintChanged();

  void UpdateWebExposedLargestContentfulPaintIfNeeded();
  void UpdateWebExposedLargestContentfulImage(const ImageRecord& largest_image);
  void UpdateWebExposedLargestContentfulText(const TextRecord& largest_text);

  std::unique_ptr<TracedValue> CreateWebExposedCandidateTraceData(
      const TextRecord&);
  std::unique_ptr<TracedValue> CreateWebExposedCandidateTraceData(
      const ImageRecord&);
  std::unique_ptr<TracedValue> CreateWebExposedCandidateTraceDataCommon(
      const PaintTimingRecord&);

  bool HasLargestImagePaintChangedForMetrics(
      base::TimeTicks largest_image_paint_time,
      uint64_t largest_image_paint_size) const;

  bool HasLargestTextPaintChangedForMetrics(
      base::TimeTicks largest_text_paint_time,
      uint64_t largest_text_paint_size) const;

  void ReportMetricsCandidateToTrace(const ImageRecord&, base::TimeTicks);
  void ReportMetricsCandidateToTrace(const TextRecord&);
  void ReportNoMetricsImageCandidateToTrace();

  void UpdateLatestLcpDetailsTypeIfNeeded();

  ImageRecord* LargestPaintedOrPendingImage() const;

  Member<WindowPerformance> window_performance_;

  uint64_t largest_reported_size_ = 0u;
  double largest_image_bpp_ = 0.0;
  unsigned web_exposed_candidate_count_ = 0;
  unsigned ukm_largest_image_candidate_count_ = 0;
  unsigned ukm_largest_text_candidate_count_ = 0;

  // The |latest_lcp_details_| struct is just for internal accounting purposes
  // and is not reported anywhere (neither to metrics, nor to the web exposed
  // API).
  LargestContentfulPaintDetails latest_lcp_details_;

  Member<Delegate> delegate_;

  // The current largest text. For hard navs, this is the largest presented
  // text. For soft navs, it's the largest painted text.
  //
  // TODO(crbug.com/454082771): Soft nav behavior should match hard nav behavior
  // for this and and `largest_painted_image_`.
  Member<TextRecord> largest_text_;

  // The current largest image "committed" candidate. For hard navs, this is the
  // largest presented image, and it's the LCP image candidate if it's larger
  // than the `largest_pending_image_`. For soft navs, this is the largest
  // painted image, and it represents the current LCP image candidate.
  Member<ImageRecord> largest_painted_image_;

  // The largest image that has been encountered, but not necessarily loaded,
  // painted, or presented yet. This is used to prevent recording LCP for pages
  // that are unloaded when the largest image is still pending.
  //
  // TODO(crbug.com/449779010): This is currently only used for hard navs, but
  // soft navs should also have this behavior.
  //
  // TODO(crbug.com/454067883): This also affects which intermediate candidates
  // are emitted to performance timeline, since this gets passed to
  // `UpdateWebExposedLargestContentfulPaintIfNeeded().
  Member<ImageRecord> largest_pending_image_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
