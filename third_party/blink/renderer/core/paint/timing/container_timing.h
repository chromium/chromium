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
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callbacks.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

class ContainerTimingPaintAttributionTracker;
class PerformanceContainerTiming;

// ContainerTiming is responsible for aggregating the text and image element
// timing events for a given document. It is owned by the document's
// PaintTiming.
class CORE_EXPORT ContainerTiming final
    : public GarbageCollected<ContainerTiming> {
 public:
  explicit ContainerTiming(LocalDOMWindow&);
  ContainerTiming(const ContainerTiming&) = delete;
  ContainerTiming& operator=(const ContainerTiming&) = delete;

  // Returns the ContainerTiming of the window's document, created on first use.
  static ContainerTiming& From(LocalDOMWindow&);

  // The container-timing decision is made via the paint attribution tracker,
  // populated during the pre-paint walk. Returns false once every container
  // root of `element` has stopped recording.
  static bool ContributesToContainerTiming(Element* element);

  // Stops recording for the container roots that painted before this. Paints
  // committed before this keep their presentation callbacks, so they are still
  // reported.
  void OnInputOrScroll() { ++input_count_; }

  void MaybeUpdateContainerRootIdentifier(HTMLElement* element,
                                          const AtomicString& new_value);
  // Drops the record of a disconnected container root.
  void OnContainerRootDisconnected(HTMLElement* element);

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

  bool HasRecordForTesting(HTMLElement* container_root) const {
    return container_root_records_.Contains(container_root);
  }

  void Trace(Visitor* visitor) const;

 private:
  class Record final : public GarbageCollected<Record> {
   public:
    Record(HTMLElement* container_root,
           const AtomicString& identifier,
           uint64_t first_paint_input_count);
    Record(const Record&) = delete;
    Record& operator=(const Record&) = delete;

    const AtomicString& identifier() const { return identifier_; }

    // A record takes the paints committed before the first input or scroll
    // after its first paint.
    bool IsRecording(uint64_t input_count) const {
      return input_count == first_paint_input_count_;
    }

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
    const WeakMember<HTMLElement> container_root_;
    const AtomicString identifier_;
    const uint64_t first_paint_input_count_;
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

  // Returns true if `container_root` or one of the roots it propagates to
  // would take a paint committed now.
  bool IsRecordingFor(HTMLElement* container_root) const;

  // Paints propagate from a root to its ancestor roots. Both return null where
  // the propagation stops.
  static HTMLElement* LiveContainerRoot(HTMLElement* container_root);
  HTMLElement* NextPropagationRoot(HTMLElement* container_root) const;

  void FlushTimingsOnFramePresented(GCedHeapVector<PendingEntry>*,
                                    const base::TimeTicks&,
                                    const DOMPaintTimingInfo&,
                                    uint64_t navigation_id);
  Record* GetOrCreateRecord(HTMLElement* container_root);

  Member<LocalDOMWindow> window_;
  Member<WindowPerformance> performance_;
  HeapHashMap<WeakMember<HTMLElement>, Member<Record>> container_root_records_;
  // Records that grew since the last TakePaintTimingCallback().
  HeapVector<Member<Record>> dirty_records_;
  // Never null: created in the constructor, which CHECKs that container timing
  // is enabled, and never reassigned.
  const Member<ContainerTimingPaintAttributionTracker>
      paint_attribution_tracker_;
  // Each container root stops recording after the first input or scroll
  // since its first paint, while element timing keeps reporting. See
  // https://w3c.github.io/container-timing/#handle-element-painted.
  uint64_t input_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_CONTAINER_TIMING_H_
