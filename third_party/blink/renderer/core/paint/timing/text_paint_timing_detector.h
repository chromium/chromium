// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_PAINT_TIMING_DETECTOR_H_

#include "base/functional/function_ref.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callbacks.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {
class LargestContentfulPaintManager;
class LayoutBoxModelObject;
class PaintTimingClient;
class PaintTimingDetector;
class PropertyTreeStateOrAlias;

// TextPaintTimingDetector contains Largest Text Paint and support for Text
// Element Timing.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within the viewport. Specifically, it:
// 1. Tracks all texts' first paints. If the text may be a largest text or is
// required by Element Timing, it records the visual size and paint time.
// 2. It keeps track of information regarding the largest text paint seen so
// far. Because the new version of LCP includes removed content, this record may
// only increase in size over time. See also this doc, which is now somewhat
// outdated: http://bit.ly/fcp_plus_plus.
class CORE_EXPORT TextPaintTimingDetector final
    : public GarbageCollected<TextPaintTimingDetector> {
  friend class TextPaintTimingDetectorTest;

 public:
  explicit TextPaintTimingDetector(PaintTimingDetector*);
  TextPaintTimingDetector(const TextPaintTimingDetector&) = delete;
  TextPaintTimingDetector& operator=(const TextPaintTimingDetector&) = delete;

  bool ShouldWalkObject(const LayoutBoxModelObject&);
  void RecordAggregatedText(const LayoutBoxModelObject& aggregator,
                            const gfx::Rect& aggregated_visual_rect,
                            const PropertyTreeStateOrAlias&);

  // Returns the set of `TextRecord`s that were rendered in the current frame
  // and need paint and presentation time. Called by `PaintTiming` at the end of
  // the current frame's paint stage.
  HeapVector<Member<TextRecord>> TakeTextRecordsOnPaintFinished();

  // Mark that the `LayoutObject` should be considered for paint timing, even if
  // it's already been painted, because it was modified as part of an
  // interaction (after hard LCP has stopped). This will not cause new element
  // timing entries to be emitted.
  void ResetPaintTrackingOnInteraction(const LayoutObject&);

  void ReportLargestIgnoredText();
  void Trace(Visitor*) const;

 private:
  void SendRectsToHud();
  friend class LargestContentfulPaintCalculatorTest;

  // The state of `LayoutObject`s being tracked in the `recorded_set_`.
  enum class TextPaintStatus { kPainted, kAllowRepaint };

  TextRecord* CreateTextRecord(
      const LayoutObject& object,
      uint64_t visual_size,
      const PropertyTreeStateOrAlias& property_tree_state,
      const gfx::Rect& frame_visual_rect,
      const gfx::RectF& root_visual_rect);

  void ForEachPaintTimingClient(base::FunctionRef<void(PaintTimingClient*)>);

  LargestContentfulPaintManager* GetLargestContentfulPaintManager() const;

  // LayoutObjects for which text has been aggregated.
  HeapHashMap<WeakMember<const LayoutObject>, TextPaintStatus> recorded_set_;

  // Text records queued for paint time for the current frame. This is returned
  // and cleared in `TakeTextRecordsOnPaintFinished()`.
  HeapVector<Member<TextRecord>> texts_queued_for_paint_time_;

  Member<PaintTimingDetector> paint_timing_detector_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_PAINT_TIMING_DETECTOR_H_
