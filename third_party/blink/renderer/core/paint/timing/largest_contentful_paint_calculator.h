// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

// LargestContentfulPaintCalculator is responsible for tracking the largest
// image paint and the largest text paint and notifying WindowPerformance
// whenever a new LatestLargestContentfulPaint entry should be dispatched.
class CORE_EXPORT LargestContentfulPaintCalculator final
    : public GarbageCollected<LargestContentfulPaintCalculator> {
 public:
  explicit LargestContentfulPaintCalculator(WindowPerformance*);
  LargestContentfulPaintCalculator(const LargestContentfulPaintCalculator&) =
      delete;
  LargestContentfulPaintCalculator& operator=(
      const LargestContentfulPaintCalculator&) = delete;

  void UpdateWebExposedLargestContentfulPaintIfNeeded(
      const TextRecord* largest_text,
      const ImageRecord* largest_image,
      bool is_triggered_by_soft_navigation);

  void Trace(Visitor* visitor) const;

 private:
  friend class LargestContentfulPaintCalculatorTest;

  void UpdateWebExposedLargestContentfulImage(
      const ImageRecord* largest_image,
      bool is_triggered_by_soft_navigation);
  void UpdateWebExposedLargestContentfulText(
      const TextRecord& largest_text,
      bool is_triggered_by_soft_navigation);

  std::unique_ptr<TracedValue> TextCandidateTraceData(
      const TextRecord& largest_text);
  std::unique_ptr<TracedValue> ImageCandidateTraceData(
      const ImageRecord* largest_image);

  Member<WindowPerformance> window_performance_;

  uint64_t largest_reported_size_ = 0u;
  double largest_image_bpp_ = 0.0;
  unsigned count_candidates_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
