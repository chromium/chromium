// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_position_visibility_observer.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

AnchorPositionVisibilityObserver::AnchorPositionVisibilityObserver(
    Element& anchored_element)
    : anchored_element_(anchored_element) {}

void AnchorPositionVisibilityObserver::MonitorAnchor(const Element* anchor) {
  if (anchor_element_) {
    observer_->disconnect();
    observer_ = nullptr;
  }

  anchor_element_ = anchor;

  // Setup an intersection observer to monitor intersection visibility.
  if (anchor_element_) {
    Node* root = nullptr;
    if (LayoutObject* anchored_object = anchored_element_->GetLayoutObject()) {
      root = anchored_object->Container()->GetNode();
    }

    observer_ = IntersectionObserver::Create(
        anchor_element_->GetDocument(),
        WTF::BindRepeating(
            &AnchorPositionVisibilityObserver::OnIntersectionVisibilityChanged,
            WrapWeakPersistent(this)),
        // Do not record metrics for this internal intersection observer.
        std::nullopt,
        IntersectionObserver::Params{
            .root = root,
            .thresholds = {IntersectionObserver::kMinimumThreshold},
            .behavior = IntersectionObserver::kDeliverDuringPostLayoutSteps,
        });
    // TODO(pdr): Refactor intersection observer to take const objects.
    observer_->observe(const_cast<Element*>(anchor_element_.Get()));
  } else {
    SetLayerInvisible(LayerPositionVisibility::kAnchorsIntersectionVisible,
                      false);
    SetLayerInvisible(LayerPositionVisibility::kChainedAnchorsVisible, false);
  }
}

void AnchorPositionVisibilityObserver::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(anchored_element_);
  visitor->Trace(anchor_element_);
}

void AnchorPositionVisibilityObserver::UpdateForCssAnchorVisibility() {
  bool invisible = false;
  if (anchor_element_) {
    if (LayoutObject* anchor = anchor_element_->GetLayoutObject()) {
      invisible = anchor->StyleRef().UsedVisibility() != EVisibility::kVisible;
    }
  }
  SetLayerInvisible(LayerPositionVisibility::kAnchorsCssVisible, invisible);
}

void AnchorPositionVisibilityObserver::UpdateForChainedAnchorVisibility(
    const HeapHashSet<WeakMember<ScrollSnapshotClient>>& clients) {
  HeapVector<Member<AnchorPositionVisibilityObserver>>
      observers_with_chained_anchor;
  for (auto& client : clients) {
    if (auto* scroll_data = DynamicTo<AnchorPositionScrollData>(client.Get())) {
      if (auto* observer = scroll_data->GetAnchorPositionVisibilityObserver()) {
        observer->SetLayerInvisible(
            LayerPositionVisibility::kChainedAnchorsVisible, false);
        if (scroll_data->DefaultAnchorHasChainedAnchor()) {
          observers_with_chained_anchor.push_back(observer);
        }
      }
    }
  }
  for (auto& observer : observers_with_chained_anchor) {
    observer->SetLayerInvisible(
        LayerPositionVisibility::kChainedAnchorsVisible,
        observer->IsInvisibleForChainedAnchorVisibility());
  }
}

bool AnchorPositionVisibilityObserver::IsInvisibleForChainedAnchorVisibility()
    const {
  DCHECK(anchored_element_->GetAnchorPositionScrollData()
             ->DefaultAnchorHasChainedAnchor());
  if (!anchor_element_ || !anchor_element_->GetLayoutObject()) {
    return false;
  }
  for (auto* layer = anchor_element_->GetLayoutObject()->EnclosingLayer();
       layer; layer = layer->Parent()) {
    if (auto* box = layer->GetLayoutBox()) {
      if (auto* chained_data = box->GetAnchorPositionScrollData()) {
        // `layer` is a chained anchor.
        if (auto* chained_layer = box->Layer()) {
          // UpdateForChainedAnchorVisibility() has cleared the invisible flag
          // for LayerPositionVisibility::kChainedAnchorsVisible, so if any
          // invisible flag is set, we are sure it's up-to-date.
          if (chained_layer->InvisibleForPositionVisibility()) {
            return true;
          }
        }
        if (auto* chained_observer =
                chained_data->GetAnchorPositionVisibilityObserver();
            chained_observer && chained_data->DefaultAnchorHasChainedAnchor()) {
          // If the chained anchor's visibility also depends on other chained
          // anchors, check visibility recursively.
          if (chained_observer->IsInvisibleForChainedAnchorVisibility()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}

void AnchorPositionVisibilityObserver::SetLayerInvisible(
    LayerPositionVisibility position_visibility,
    bool invisible) {
  LayoutBoxModelObject* layout_object =
      anchored_element_->GetLayoutBoxModelObject();
  if (!layout_object) {
    return;
  }
  if (PaintLayer* layer = layout_object->Layer()) {
    layer->SetInvisibleForPositionVisibility(position_visibility, invisible);
  }
}

void AnchorPositionVisibilityObserver::OnIntersectionVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  CHECK_EQ(entries.size(), 1u);
  CHECK_EQ(entries.front()->target(), anchor_element_);
  bool invisible = !entries.front()->isIntersecting();
  SetLayerInvisible(LayerPositionVisibility::kAnchorsIntersectionVisible,
                    invisible);
}

}  // namespace blink
