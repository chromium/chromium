// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_H_

#include "base/time/time.h"
#include "cc/base/region.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callbacks.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class ContainerTimingPaintAttributionTracker;

// ContainerTiming is responsible for aggregating the text and image element
// timing events for a given window.
class CORE_EXPORT ContainerTiming final
    : public GarbageCollected<ContainerTiming>,
      public Supplement<LocalDOMWindow> {
 public:
  static constexpr const char kSupplementName[] = "ContainerTiming";

  explicit ContainerTiming(LocalDOMWindow&);
  ContainerTiming(const ContainerTiming&) = delete;
  ContainerTiming& operator=(const ContainerTiming&) = delete;

  static ContainerTiming& From(LocalDOMWindow&);

  // The container-timing decision is made via the paint attribution tracker,
  // populated during the pre-paint walk.
  static bool ContributesToContainerTiming(Element* element);

  void MaybeUpdateContainerRootIdentifier(Element* element,
                                          const AtomicString& new_value);
  // Drops the record of a disconnected container root.
  void OnContainerRootDisconnected(Element* element);

  ContainerTimingPaintAttributionTracker* PaintAttributionTracker() {
    return paint_attribution_tracker_.Get();
  }

  // Queues the paint of `element` if it contributes to container timing.
  void MaybeQueuePaint(Element& element, const gfx::RectF& intersection_rect);

  // Returns a callback that records the paints queued since the last call and
  // emits their entries once the frame is presented, or nullopt if none were
  // queued.
  OptionalPaintTimingCallback TakePaintTimingCallback();

  // Emits an entry for each container root updated since the last call.
  void EmitPerformanceEntries();

  void OnElementPaintedForTesting(const DOMPaintTimingInfo& paint_timing_info,
                                  Element* element,
                                  const gfx::RectF& intersection_rect) {
    OnElementPainted(paint_timing_info, element, intersection_rect);
  }

  bool HasRecordForTesting(Element* container_root) const {
    return container_root_records_.Contains(container_root);
  }

  void Trace(Visitor* visitor) const override;

 private:
  // A paint queued at paint time, recorded once the frame is presented.
  struct PendingPaint final : public GarbageCollected<PendingPaint> {
    PendingPaint(const gfx::RectF& intersection_rect, Element* element)
        : intersection_rect(intersection_rect), element(element) {}

    void Trace(Visitor* visitor) const { visitor->Trace(element); }

    const gfx::RectF intersection_rect;
    const WeakMember<Element> element;
  };

  class Record final : public GarbageCollected<Record> {
   public:
    Record(const DOMPaintTimingInfo& paint_timing_info,
           const AtomicString& identifier);
    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    const AtomicString& identifier() const { return identifier_; }

    void MaybeUpdateLastNewPaintedArea(
        const DOMPaintTimingInfo& paint_timing_info,
        Element* element,
        const gfx::Rect& enclosing_rect);

    void MaybeEmitPerformanceEntry(WindowPerformance*, Element* container_root);

    void Trace(Visitor*) const;

   private:
    const DOMPaintTimingInfo first_paint_timing_info_;
    const AtomicString identifier_;
    DOMPaintTimingInfo last_new_painted_area_paint_timing_info_;
    // The largest element, by its own clipped area, painted since the last
    // entry was emitted, and that area. Both reset on emission.
    WeakMember<Element> largest_painted_area_element_;
    uint64_t largest_painted_area_ = 0;
    cc::Region painted_region_;
    bool has_pending_changes_ = false;
  };

  void OnElementPainted(const DOMPaintTimingInfo& paint_timing_info,
                        Element* element,
                        const gfx::RectF& intersection_rect);
  void FlushTimingsOnFramePresented(GCedHeapVector<Member<PendingPaint>>*,
                                    const base::TimeTicks&,
                                    const DOMPaintTimingInfo&,
                                    uint64_t navigation_id);
  Record* GetOrCreateRecord(const DOMPaintTimingInfo& paint_timing_info,
                            Element* container_root);

  Member<WindowPerformance> performance_;
  HeapHashMap<WeakMember<Element>, Member<Record>> container_root_records_;
  HeapVector<Member<PendingPaint>> pending_paints_;
  // Never null: created in the constructor, which CHECKs that container timing
  // is enabled, and never reassigned.
  const Member<ContainerTimingPaintAttributionTracker>
      paint_attribution_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_H_
