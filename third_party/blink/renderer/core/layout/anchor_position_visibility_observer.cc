// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_position_visibility_observer.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

AnchorPositionVisibilityObserver::AnchorPositionVisibilityObserver(
    Element& anchored_element)
    : anchored_element_(anchored_element) {
  CHECK(RuntimeEnabledFeatures::CSSPositionVisibilityEnabled());
}

void AnchorPositionVisibilityObserver::MonitorAnchor(const Element* anchor) {
  if (anchor_element_) {
    observer_->disconnect();
    observer_ = nullptr;
  }

  anchor_element_ = anchor;

  if (anchor_element_) {
    observer_ = IntersectionObserver::Create(
        anchor_element_->GetDocument(),
        WTF::BindRepeating(
            &AnchorPositionVisibilityObserver::OnVisibilityChanged,
            WrapWeakPersistent(this)),
        // Do not record metrics for this internal intersection observer.
        std::nullopt,
        IntersectionObserver::Params{
            // TODO(pdr): Set the intersection observer root to the containing
            // block of the anchor and anchored elements. See:
            // `position-visibility-anchors-visible-non-intervening-container.tentative.html`.
            .thresholds = {IntersectionObserver::kMinimumThreshold, 1.0f},
            .behavior = IntersectionObserver::kDeliverDuringPostLayoutSteps,
        });
    // TODO(pdr): Refactor intersection observer to take const objects.
    observer_->observe(const_cast<Element*>(anchor_element_.Get()));
  } else {
    UpdateLayerInvisible(false);
  }
}

void AnchorPositionVisibilityObserver::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(anchored_element_);
  visitor->Trace(anchor_element_);
}

void AnchorPositionVisibilityObserver::UpdateLayerInvisible(bool invisible) {
  LayoutBoxModelObject* layout_object =
      anchored_element_->GetLayoutBoxModelObject();
  if (!layout_object) {
    return;
  }
  if (PaintLayer* layer = layout_object->Layer()) {
    layer->SetInvisibleForPositionVisibility(
        PositionVisibility::kAnchorsVisible, invisible);
  }
}

void AnchorPositionVisibilityObserver::OnVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  CHECK_EQ(entries.size(), 1u);
  CHECK_EQ(entries.front()->target(), anchor_element_);

  UpdateLayerInvisible(!entries.front()->isIntersecting());
}

}  // namespace blink
