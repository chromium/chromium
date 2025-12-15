// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

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

  void UpdateWebExposedLargestContentfulPaintIfNeeded(
      const TextRecord* largest_text,
      const ImageRecord* largest_image);

  bool NotifyMetricsIfLargestImagePaintChanged(const ImageRecord&);
  bool NotifyMetricsIfLargestTextPaintChanged(const TextRecord&);

  const LargestContentfulPaintDetails& LatestLcpDetails() const {
    return latest_lcp_details_;
  }

  void Trace(Visitor* visitor) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;

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
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
