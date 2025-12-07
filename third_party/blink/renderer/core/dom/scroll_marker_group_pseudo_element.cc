// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_axis.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"

namespace blink {

ScrollMarkerGroupPseudoElement::ScrollMarkerGroupPseudoElement(
    Element* originating_element,
    PseudoId pseudo_id)
    : PseudoElement(originating_element, pseudo_id),
      scroll_marker_group_data_(MakeGarbageCollected<ScrollMarkerGroupData>(
          originating_element->GetDocument().GetFrame())) {}

void ScrollMarkerGroupPseudoElement::Trace(Visitor* v) const {
  v->Trace(scroll_marker_group_data_);
  PseudoElement::Trace(v);
}

void ScrollMarkerGroupPseudoElement::AddToFocusGroup(
    ScrollMarkerPseudoElement& scroll_marker) {
  scroll_marker_group_data_->AddToFocusGroup(scroll_marker);
}

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::FindNextScrollMarker(
    const Element* current) {
  return To<ScrollMarkerPseudoElement>(
      scroll_marker_group_data_->FindNextScrollMarker(current));
}

ScrollMarkerPseudoElement*
ScrollMarkerGroupPseudoElement::FindPreviousScrollMarker(
    const Element* current) {
  return To<ScrollMarkerPseudoElement>(
      scroll_marker_group_data_->FindPreviousScrollMarker(current));
}

void ScrollMarkerGroupPseudoElement::RemoveFromFocusGroup(
    ScrollMarkerPseudoElement& scroll_marker) {
  scroll_marker_group_data_->RemoveFromFocusGroup(scroll_marker);
}

void ScrollMarkerGroupPseudoElement::ActivateNextScrollMarker() {
  ActivateScrollMarker(FindNextScrollMarker(Selected()));
}

void ScrollMarkerGroupPseudoElement::ActivatePrevScrollMarker() {
  ActivateScrollMarker(FindPreviousScrollMarker(Selected()));
}

void ScrollMarkerGroupPseudoElement::ActivateScrollMarker(
    ScrollMarkerPseudoElement* scroll_marker,
    bool apply_snap_alignment) {
  if (!scroll_marker) {
    return;
  }
  // parentElement is ::column for column scroll marker and
  // ultimate originating element for regular scroll marker.
  mojom::blink::ScrollIntoViewParamsPtr params =
      scroll_into_view_util::CreateScrollIntoViewParams(
          *scroll_marker->parentElement()->GetComputedStyle());
  scroll_marker->ScrollIntoViewNoVisualUpdate(
      std::move(params), &UltimateOriginatingElement(),
      /* include_self = */ scroll_marker->UltimateOriginatingElement() ==
          UltimateOriginatingElement());
  const bool tabs_mode =
      ScrollMarkerGroupMode() == ScrollMarkerGroup::ScrollMarkerMode::kTabs;
  const bool links_mode =
      ScrollMarkerGroupMode() == ScrollMarkerGroup::ScrollMarkerMode::kLinks;
  // If the scroller's scroll-marker-group property is set to `tabs`,
  // we retain focus on the ::scroll-marker.
  if (tabs_mode ||
      !RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled()) {
    GetDocument().SetFocusedElement(scroll_marker,
                                    FocusParams(SelectionBehaviorOnFocus::kNone,
                                                mojom::blink::FocusType::kNone,
                                                /*capabilities=*/nullptr));
  }
  if (links_mode &&
      RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled()) {
    // If the scroller's scroll-marker-group property is set to `links`,
    // we lose focus from ::scroll-marker upon activation.
    GetDocument().ClearFocusedElement();
  }
  SetSelected(*scroll_marker, apply_snap_alignment);
  // If the scroller's scroll-marker-group property is set to `links`,
  // per https://drafts.csswg.org/css-overflow-5/#scroll-target-focus
  // we want to start our search from scroll target of ::scroll-marker,
  // which is ultimate originating element for regular scroll marker
  // and TODO(378698659): the first element in ::column's view for column
  // scroll marker, but it's not clear yet what how to implement that.
  if (links_mode ||
      !RuntimeEnabledFeatures::CSSScrollMarkerGroupModesEnabled()) {
    GetDocument().SetSequentialFocusNavigationStartingPoint(
        &scroll_marker->UltimateOriginatingElement());
  }
}

void ScrollMarkerGroupPseudoElement::SetSelected(
    ScrollMarkerPseudoElement& scroll_marker,
    bool apply_snap_alignment) {
  return scroll_marker_group_data_->MaybeSetPendingSelectedMarker(
      &scroll_marker, apply_snap_alignment);
}

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::Selected() const {
  return To<ScrollMarkerPseudoElement>(scroll_marker_group_data_->Selected());
}

void ScrollMarkerGroupPseudoElement::PinSelectedMarker(
    ScrollMarkerPseudoElement* scroll_marker) {
  scroll_marker_group_data_->PinSelectedMarker(scroll_marker);
}

void ScrollMarkerGroupPseudoElement::UnPinSelectedMarker() {
  return scroll_marker_group_data_->UnPinSelectedMarker();
}

bool ScrollMarkerGroupPseudoElement::SelectedMarkerIsPinned() const {
  return scroll_marker_group_data_->SelectedMarkerIsPinned();
}

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::First() const {
  HeapVector<Member<Element>>& focus_group =
      scroll_marker_group_data_->ScrollMarkers();
  if (!focus_group.size()) {
    return nullptr;
  }
  return To<ScrollMarkerPseudoElement>(focus_group.front().Get());
}

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::Last() const {
  HeapVector<Member<Element>>& focus_group =
      scroll_marker_group_data_->ScrollMarkers();
  if (!focus_group.size()) {
    return nullptr;
  }
  return To<ScrollMarkerPseudoElement>(focus_group.back().Get());
}

void ScrollMarkerGroupPseudoElement::Dispose() {
  HeapVector<Member<Element>> focus_group =
      scroll_marker_group_data_->ScrollMarkers();
  for (Element* scroll_marker : focus_group) {
    To<ScrollMarkerPseudoElement>(scroll_marker)->SetScrollMarkerGroup(nullptr);
  }
  if (ScrollMarkerPseudoElement* selected = Selected()) {
    selected->SetSelected(false);
  }
  scroll_marker_group_data_->ClearFocusGroup();
  if (GetLayoutBox() && GetLayoutBox()->GetFrameView()) {
    GetLayoutBox()->GetFrameView()->RemovePendingScrollMarkerSelectionUpdate(
        this);
  }
  PseudoElement::Dispose();
}

void ScrollMarkerGroupPseudoElement::ClearFocusGroup() {
  scroll_marker_group_data_->ClearFocusGroup();
}

void ScrollMarkerGroupPseudoElement::UpdateSelectedScrollMarker() {
  // Implements scroll tracking for scroll marker controls as per
  // https://drafts.csswg.org/css-overflow-5/#scroll-container-scroll.
  auto* scroller =
      DynamicTo<LayoutBox>(UltimateOriginatingElement().GetLayoutObject());
  if (!scroller ||
      (!scroller->IsScrollContainer() && !scroller->IsDocumentElement())) {
    return;
  }

  scroll_marker_group_data_->UpdateSelectedScrollMarker();
}

void ScrollMarkerGroupPseudoElement::DetachLayoutTree(
    bool performing_reattach) {
  HeapVector<Member<Element>> focus_group =
      scroll_marker_group_data_->ScrollMarkers();
  for (Element* scroll_marker : focus_group) {
    To<ScrollMarkerPseudoElement>(scroll_marker)
        ->DetachLayoutTree(performing_reattach);
  }
  scroll_marker_group_data_->ClearFocusGroup();
  PseudoElement::DetachLayoutTree(performing_reattach);
}

void ScrollMarkerGroupPseudoElement::ScrollSelectedIntoView(bool apply_snap) {
  if (ScrollMarkerPseudoElement* selected = Selected()) {
    selected->ScrollIntoView(apply_snap);
  }
}

ScrollMarkerGroup::ScrollMarkerMode
ScrollMarkerGroupPseudoElement::ScrollMarkerGroupMode() const {
  const ComputedStyle& parent_style = parentElement()->ComputedStyleRef();
  CHECK(parent_style.ScrollMarkerGroupMode().has_value());
  return parent_style.ScrollMarkerGroupMode().value();
}

}  // namespace blink
