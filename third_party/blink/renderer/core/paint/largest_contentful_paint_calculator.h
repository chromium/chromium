// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_

#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"

namespace blink {

// LargestContentfulPaintCalculator is responsible for tracking the largest
// image paint and the largest text paint and notifying WindowPerformance
// whenever a new LatestLargestContentfulPaint entry should be dispatched.
class CORE_EXPORT LargestContentfulPaintCalculator final
    : public GarbageCollected<LargestContentfulPaintCalculator> {
 public:
  explicit LargestContentfulPaintCalculator(WindowPerformance*);

  void UpdateLargestContentPaintIfNeeded(
      base::Optional<base::WeakPtr<TextRecord>> largest_text,
      base::Optional<const ImageRecord*> largest_image);

  void Trace(blink::Visitor* visitor);

 private:
  friend class LargestContentfulPaintCalculatorTest;

  enum class LargestContentType {
    kUnknown,
    kImage,
    kText,
  };
  void OnLargestImageUpdated(const ImageRecord* largest_image);
  void OnLargestTextUpdated(base::WeakPtr<TextRecord> largest_text);
  void UpdateLargestContentfulPaint(LargestContentType type);

  uint64_t LargestTextSize() {
    return largest_text_ ? largest_text_->first_size : 0u;
  }

  uint64_t LargestImageSize() {
    return largest_image_ ? largest_image_->first_size : 0u;
  }

  std::unique_ptr<TracedValue> TextCandidateTraceData();
  std::unique_ptr<TracedValue> ImageCandidateTraceData();
  std::unique_ptr<TracedValue> InvalidationTraceData();

  Member<WindowPerformance> window_performance_;

  // Largest image information. Stores its own copy of the information so that
  // the lifetime is not dependent on that of ImagePaintTimingDetector.
  std::unique_ptr<ImageRecord> largest_image_;
  // Largest text information. Stores its own copy of the information so that
  // the lifetime is not dependent on that of TextPaintTimingDetector.
  std::unique_ptr<TextRecord> largest_text_;
  LargestContentType last_type_ = LargestContentType::kUnknown;

  unsigned count_candidates_ = 0;

  DISALLOW_COPY_AND_ASSIGN(LargestContentfulPaintCalculator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LARGEST_CONTENTFUL_PAINT_CALCULATOR_H_
