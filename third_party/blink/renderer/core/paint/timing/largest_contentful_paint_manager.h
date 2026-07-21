// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_MANAGER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/lcp_objects.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class AtomicString;
struct DOMPaintTimingInfo;
class Element;
class LocalDOMWindow;
class ImageRecord;
class String;
class TextRecord;

// `LargestContentfulPaintManager` is responsible for computing Largest
// Contentful Paint (LCP) for hard navigations. It mainly does so by delegating
// to `LargestContentfulPaintCalculator`.
class CORE_EXPORT LargestContentfulPaintManager
    : public GarbageCollected<LargestContentfulPaintManager>,
      public LargestContentfulPaintCalculator::Delegate {
 public:
  explicit LargestContentfulPaintManager(LocalDOMWindow*);

  // LargestContentfulPaintCalculator::Delegate:
  void EmitLcpPerformanceEntry(const DOMPaintTimingInfo& paint_timing_info,
                               uint64_t paint_size,
                               base::TimeTicks load_time,
                               const AtomicString& id,
                               const String& url,
                               Element* element) override;
  void OnLcpMetricsForReportingChanged() override;
  bool IsHardNavigation() const override { return true; }
  void Trace(Visitor* visitor) const override;

  // Called on the first discrete input or scroll. Shuts down the manager and
  // stops recording LCP. The last value pushed to PerformanceTimingForReporting
  // will be the final value.
  void OnFirstInputOrScroll();

  // Updates the current image and text LCP candidates based on `image_records`
  // and `text_records`. Does nothing if the LCP algorithm has already stopped
  // due to input.
  void OnFramePresented(const HeapVector<Member<ImageRecord>>& image_records,
                        const HeapVector<Member<TextRecord>>& text_records);

  // Called by PaintTiming on the first paint for `ImageRecord`. Marks the
  // record as needed for LCP if it's a valid candidate and larger than last
  // emitted candidate. This must not be called after the hard LCP algorithm has
  // stopped.
  void InitializePaintTracking(ImageRecord*);

  // Called by PaintTiming on the first paint for `TextRecord`. Marks the record
  // as needed for LCP if it's a valid candidate and larger than last emitted
  // candidate. This must not be called after the hard LCP algorithm has
  // stopped.
  void InitializePaintTracking(TextRecord*);

  // Called by PaintTiming when an image pending presentation time has been
  // removed. This must not be called after the hard LCP algorithm has stopped.
  void OnPendingImageRemoved(ImageRecord*);

  LargestContentfulPaintCalculator* LargestContentfulPaintCalculatorForTest() {
    return largest_contentful_paint_calculator_;
  }

 private:
  Member<LocalDOMWindow> window_;
  Member<LargestContentfulPaintCalculator> largest_contentful_paint_calculator_;
  bool contains_full_viewport_image_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_LARGEST_CONTENTFUL_PAINT_MANAGER_H_
