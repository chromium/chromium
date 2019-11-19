// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_element_timing.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/element_timing_utils.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
const char TextElementTiming::kSupplementName[] = "TextElementTiming";

// static
TextElementTiming& TextElementTiming::From(LocalDOMWindow& window) {
  TextElementTiming* timing =
      Supplement<LocalDOMWindow>::From<TextElementTiming>(window);
  if (!timing) {
    timing = MakeGarbageCollected<TextElementTiming>(window);
    ProvideTo(window, timing);
  }
  return *timing;
}

TextElementTiming::TextElementTiming(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      performance_(DOMWindowPerformance::performance(window)) {}

// static
FloatRect TextElementTiming::ComputeIntersectionRect(
    const LayoutObject& object,
    const IntRect& aggregated_visual_rect,
    const PropertyTreeState& property_tree_state,
    const LocalFrameView* frame_view) {
  Node* node = object.GetNode();
  DCHECK(node);
  if (!NeededForElementTiming(*node))
    return FloatRect();

  return ElementTimingUtils::ComputeIntersectionRect(
      &frame_view->GetFrame(), aggregated_visual_rect, property_tree_state);
}

bool TextElementTiming::CanReportElements() const {
  DCHECK(performance_);
  return performance_->HasObserverFor(PerformanceEntry::kElement) ||
         !performance_->IsElementTimingBufferFull();
}

void TextElementTiming::OnTextObjectPainted(const TextRecord& record) {
  Node* node = DOMNodeIds::NodeForId(record.node_id);
  if (!node || node->IsInShadowTree())
    return;

  // Text aggregators should be Elements!
  DCHECK(node->IsElementNode());
  auto* element = To<Element>(node);
  if (!element->FastHasAttribute(html_names::kElementtimingAttr))
    return;

  const AtomicString& id = element->GetIdAttribute();
  DEFINE_STATIC_LOCAL(const AtomicString, kTextPaint, ("text-paint"));
  performance_->AddElementTiming(
      kTextPaint, g_empty_string, record.element_timing_rect_,
      record.paint_time, base::TimeTicks(),
      element->FastGetAttribute(html_names::kElementtimingAttr), IntSize(), id,
      element);
}

void TextElementTiming::Trace(blink::Visitor* visitor) {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(performance_);
}

}  // namespace blink
