// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_ELEMENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_ELEMENT_TIMING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/container_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_client.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace gfx {
class Rect;
class RectF;
}  // namespace gfx

namespace blink {
class ImageRecord;
class LayoutObject;
class PropertyTreeStateOrAlias;
class TextRecord;

// TextElementTiming is responsible for tracking the paint timings for groups of
// text nodes associated with elements of a given window.
class CORE_EXPORT TextElementTiming final
    : public GarbageCollected<TextElementTiming>,
      public PaintTimingClient {
 public:
  // Returns whether a text node needs paint timing tracking, i.e. it is
  // registered for element timing or contributes to a container timing root.
  static bool NeededForTiming(Node& node);

  explicit TextElementTiming(LocalDOMWindow&);

  TextElementTiming(const TextElementTiming&) = delete;
  TextElementTiming& operator=(const TextElementTiming&) = delete;

  static gfx::RectF ComputeIntersectionRect(
      const LayoutObject&,
      const gfx::Rect& aggregated_visual_rect,
      const PropertyTreeStateOrAlias&);

  // PaintTimingClient:
  void OnElementLastContentfulPaint(TextRecord*,
                                    bool was_previously_reported) override;
  void OnFramePresented(const HeapVector<Member<ImageRecord>>&,
                        const HeapVector<Member<TextRecord>>&,
                        const HeapVector<Member<ElementTimingInfo>>&,
                        const DOMPaintTimingInfo&) override;
  void Trace(Visitor* visitor) const override;

  bool CanReportToElementTiming() const;
  bool CanReportToContainerTiming();
  bool CanReportElements();

 private:
  void OnTextNodePresented(const TextRecord&);

  // Returns false if container timing is not enabled, in which case
  // `container_timing_` is left null.
  bool EnsureContainerTiming();

  Member<WindowPerformance> performance_;
  Member<ContainerTiming> container_timing_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TIMING_TEXT_ELEMENT_TIMING_H_
