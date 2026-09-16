// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_element_timing.h"

#include "base/check_deref.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/timing/element_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/graphics/paint/float_clip_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

TextElementTiming::TextElementTiming(LocalDOMWindow& window)
    : performance_(DOMWindowPerformance::performance(window)) {}

// static
bool TextElementTiming::NeededForTiming(Node& node) {
  auto* element = DynamicTo<Element>(node);
  if (node.IsInShadowTree() || !element) {
    return false;
  }
  return element->FastHasAttribute(html_names::kElementtimingAttr) ||
         ContainerTiming::ContributesToContainerTiming(element);
}

// static
gfx::RectF TextElementTiming::ComputeIntersectionRect(
    const LayoutObject& object,
    const gfx::Rect& aggregated_visual_rect,
    const PropertyTreeStateOrAlias& property_tree_state) {
  Node* node = object.GetNode();
  DCHECK(node);
  return ElementTimingUtils::ComputeIntersectionRect(
      object.GetFrame(), aggregated_visual_rect, property_tree_state);
}

bool TextElementTiming::CanReportToElementTiming() const {
  DCHECK(performance_);
  return performance_->HasObserverFor(PerformanceEntry::kElement) ||
         !performance_->IsElementTimingBufferFull();
}

bool TextElementTiming::CanReportToContainerTiming() {
  DCHECK(performance_);
  if (!performance_->IsContainerTimingEnabled()) {
    return false;
  }
  return EnsureContainerTiming() &&
         container_timing_->CanReportToContainerTiming();
}

bool TextElementTiming::CanReportElements() {
  return CanReportToElementTiming() || CanReportToContainerTiming();
}

void TextElementTiming::OnFramePresented(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records,
    const HeapVector<Member<ElementTimingInfo>>&,
    const DOMPaintTimingInfo&) {
  if (!CanReportElements()) {
    return;
  }
  for (auto& record : text_records) {
    if (record->IsNeededForElementTiming()) {
      OnTextNodePresented(*record.Get());
    }
  }
}

void TextElementTiming::OnTextNodePresented(const TextRecord& record) {
  CHECK(record.IsNeededForElementTiming());

  // TODO(crbug.com/454082773): we should consider reporting these to
  // ElementTiming independently of LCP.
  if (record.WasNodeRemoved()) {
    return;
  }

  Node* node = record.GetNode();

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

  if (CanReportToElementTiming() &&
      element->FastHasAttribute(html_names::kElementtimingAttr)) {
    DEFINE_STATIC_LOCAL(const AtomicString, kTextPaint, ("text-paint"));
    const AtomicString& id = element->GetIdAttribute();
    performance_->AddElementTiming(
        kTextPaint, g_empty_string, record.ElementTimingRect(),
        record.PaintTimingInfo(), base::TimeTicks(),
        element->FastGetAttribute(html_names::kElementtimingAttr), gfx::Size(),
        id, element);
  }
  if (CanReportToContainerTiming()) {
    container_timing_->OnElementPainted(record.PaintTimingInfo(), element,
                                        record.ElementTimingRect());
  }
}

void TextElementTiming::OnElementLastContentfulPaint(
    TextRecord* record,
    bool was_previously_reported) {
  CHECK(!record->IsNeededForElementTiming());
  if (was_previously_reported ||
      !NeededForTiming(CHECK_DEREF(record->GetNode()))) {
    return;
  }
  record->SetIsNeededForElementTiming(true);
}

void TextElementTiming::Trace(Visitor* visitor) const {
  visitor->Trace(performance_);
  visitor->Trace(container_timing_);
}

bool TextElementTiming::EnsureContainerTiming() {
  if (container_timing_) {
    return true;
  }
  auto* window = To<LocalDOMWindow>(performance_->GetExecutionContext());
  DCHECK(window);
  // WindowPerformance memoizes its answer, so it can outlive the live feature
  // state, while ContainerTiming::From() CHECKs the live one. Check it here so
  // a stale cache cannot become a crash.
  if (!RuntimeEnabledFeatures::ContainerTimingEnabled(window)) {
    return false;
  }
  container_timing_ = ContainerTiming::From(*window);
  return true;
}

}  // namespace blink
