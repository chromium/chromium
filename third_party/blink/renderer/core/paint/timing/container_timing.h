// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_H_

#include <utility>

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
class PerformanceContainerTiming;

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

  // Adds the paint of `element` to the container roots it is attributed to, if
  // any. Called during paint.
  void OnElementPainted(Element& element, const gfx::RectF& intersection_rect);

  // Creates an entry for each container root that grew since the last call,
  // and returns a callback that reports them once the frame is presented, or
  // nullopt if none grew.
  OptionalPaintTimingCallback TakePaintTimingCallback();

  bool HasRecordForTesting(Element* container_root) const {
    return container_root_records_.Contains(container_root);
  }

  void Trace(Visitor* visitor) const override;

 private:
  class Record final : public GarbageCollected<Record> {
   public:
    Record(Element* container_root, const AtomicString& identifier);
    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    const AtomicString& identifier() const { return identifier_; }

    // Returns true if the container grew for the first time since the last
    // entry was created.
    bool MaybeUpdateLastNewPaintedArea(Element* element,
                                       const gfx::Rect& enclosing_rect);

    // Returns an entry for the growth since the last call. Its timing is set
    // once the frame is presented.
    PerformanceContainerTiming* CreatePerformanceEntry(DOMWindow* source);

    // Sets the presentation time of the first frame that reported this
    // container, unless already set.
    void SetFirstRenderTimeIfUnset(DOMHighResTimeStamp presentation_time);
    DOMHighResTimeStamp FirstRenderTime() const { return first_render_time_; }

    void Trace(Visitor*) const;

   private:
    // 0 until the first frame that reported this container is presented.
    DOMHighResTimeStamp first_render_time_ = 0.0;
    const WeakMember<Element> container_root_;
    const AtomicString identifier_;
    // The largest element, by its own clipped area, painted since the last
    // entry was created, and that area. Both reset when an entry is created.
    WeakMember<Element> largest_painted_area_element_;
    uint64_t largest_painted_area_ = 0;
    cc::Region painted_region_;
    bool has_pending_changes_ = false;
  };

  // An entry created at paint time, and the record it reports.
  using PendingEntry =
      std::pair<Member<Record>, Member<PerformanceContainerTiming>>;

  void FlushTimingsOnFramePresented(GCedHeapVector<PendingEntry>*,
                                    const base::TimeTicks&,
                                    const DOMPaintTimingInfo&,
                                    uint64_t navigation_id);
  Record* GetOrCreateRecord(Element* container_root);

  Member<WindowPerformance> performance_;
  HeapHashMap<WeakMember<Element>, Member<Record>> container_root_records_;
  // Records that grew since the last TakePaintTimingCallback().
  HeapVector<Member<Record>> dirty_records_;
  // Never null: created in the constructor, which CHECKs that container timing
  // is enabled, and never reassigned.
  const Member<ContainerTimingPaintAttributionTracker>
      paint_attribution_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_H_
