// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_element_timing.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/timing/element_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "ui/gfx/geometry/rect.h"

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
gfx::RectF TextElementTiming::ComputeIntersectionRect(
    const LayoutObject& object,
    const gfx::Rect& aggregated_visual_rect,
    const PropertyTreeStateOrAlias& property_tree_state,
    const LocalFrameView* frame_view) {
  Node* node = object.GetNode();
  DCHECK(node);
  if (!NeededForElementTiming(*node))
    return gfx::RectF();

  return ElementTimingUtils::ComputeIntersectionRect(
      &frame_view->GetFrame(), aggregated_visual_rect, property_tree_state);
}

bool TextElementTiming::CanReportElements() const {
  DCHECK(performance_);
  return performance_->HasObserverFor(PerformanceEntry::kElement) ||
         !performance_->IsElementTimingBufferFull();
}

void TextElementTiming::OnTextObjectPainted(const TextRecord& record) {
  Node* node = record.node_;

  // Text aggregators need to be Elements. This will not be the case if the
  // aggregator is the LayoutView (a Document node), though. This will be the
  // only aggregator we have if the text is for an @page margin, since that is
  // on the outside of the DOM.
  //
  // TODO(paint-dev): Document why it's necessary to check for null, and whether
  // we're in a shadow tree.
  if (!node || node->IsInShadowTree() || !node->IsElementNode()) {
    return;
  }

  auto* element = To<Element>(node);
  const AtomicString& id = element->GetIdAttribute();
  if (!element->FastHasAttribute(html_names::kElementtimingAttr))
    return;

  DEFINE_STATIC_LOCAL(const AtomicString, kTextPaint, ("text-paint"));
  performance_->AddElementTiming(
      kTextPaint, g_empty_string, record.element_timing_rect_,
      record.paint_time, base::TimeTicks(),
      element->FastGetAttribute(html_names::kElementtimingAttr), gfx::Size(),
      id, element);
}

void TextElementTiming::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(performance_);
}

}  // namespace blink
