// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"

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
      ScrollSnapshotClient(originating_element->GetDocument().GetFrame()) {}

void ScrollMarkerGroupPseudoElement::Trace(Visitor* v) const {
  v->Trace(selected_marker_);
  v->Trace(focus_group_);
  PseudoElement::Trace(v);
}

void ScrollMarkerGroupPseudoElement::AddToFocusGroup(
    ScrollMarkerPseudoElement& scroll_marker) {
  scroll_marker.SetScrollMarkerGroup(this);
  focus_group_.push_back(scroll_marker);
}

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::FindNextScrollMarker(
    const Element& current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[index == focus_group_.size() - 1 ? 0u : index + 1];
  }
  return nullptr;
}

ScrollMarkerPseudoElement*
ScrollMarkerGroupPseudoElement::FindPreviousScrollMarker(
    const Element& current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[index == 0u ? focus_group_.size() - 1 : index - 1];
  }
  return nullptr;
}

void ScrollMarkerGroupPseudoElement::RemoveFromFocusGroup(
    const ScrollMarkerPseudoElement& scroll_marker) {
  if (wtf_size_t index = focus_group_.Find(scroll_marker); index != kNotFound) {
    focus_group_.EraseAt(index);
    if (selected_marker_ == scroll_marker) {
      selected_marker_->SetSelected(false);
      if (index == focus_group_.size()) {
        if (index == 0) {
          selected_marker_ = nullptr;
          return;
        }
        --index;
      }
      selected_marker_ = focus_group_[index];
      selected_marker_->SetSelected(true);
    }
  }
}

void ScrollMarkerGroupPseudoElement::ActivateNextScrollMarker() {
  ActivateScrollMarker(&ScrollMarkerGroupPseudoElement::FindNextScrollMarker);
}

void ScrollMarkerGroupPseudoElement::ActivatePrevScrollMarker() {
  ActivateScrollMarker(
      &ScrollMarkerGroupPseudoElement::FindPreviousScrollMarker);
}

void ScrollMarkerGroupPseudoElement::ActivateScrollMarker(
    ScrollMarkerPseudoElement* (ScrollMarkerGroupPseudoElement::*
                                    find_scroll_marker_func)(const Element&)) {
  if (!selected_marker_) {
    return;
  }
  ScrollMarkerPseudoElement* scroll_marker =
      (this->*find_scroll_marker_func)(*Selected());
  if (!scroll_marker || scroll_marker == selected_marker_) {
    return;
  }
  mojom::blink::ScrollIntoViewParamsPtr params =
      scroll_into_view_util::CreateScrollIntoViewParams(
          *scroll_marker->OriginatingElement()->GetComputedStyle());
  scroll_marker->OriginatingElement()->ScrollIntoViewNoVisualUpdate(
      std::move(params));
  GetDocument().SetFocusedElement(scroll_marker,
                                  FocusParams(SelectionBehaviorOnFocus::kNone,
                                              mojom::blink::FocusType::kNone,
                                              /*capabilities=*/nullptr));
  SetSelected(*scroll_marker);
}

bool ScrollMarkerGroupPseudoElement::SetSelected(
    ScrollMarkerPseudoElement& scroll_marker) {
  if (selected_marker_ == scroll_marker) {
    return false;
  }
  if (selected_marker_) {
    selected_marker_->SetSelected(false);
  }
  scroll_marker.SetSelected(true);
  selected_marker_ = scroll_marker;
  return true;
}

void ScrollMarkerGroupPseudoElement::Dispose() {
  HeapVector<Member<ScrollMarkerPseudoElement>> focus_group =
      std::move(focus_group_);
  for (ScrollMarkerPseudoElement* scroll_marker : focus_group) {
    scroll_marker->SetScrollMarkerGroup(nullptr);
  }
  if (selected_marker_) {
    selected_marker_->SetSelected(false);
    selected_marker_ = nullptr;
  }
  PseudoElement::Dispose();
}

void ScrollMarkerGroupPseudoElement::ClearFocusGroup() {
  focus_group_.clear();
}

bool ScrollMarkerGroupPseudoElement::UpdateSelectedScrollMarker() {
  // Implements scroll tracking for scroll marker controls as per
  // https://drafts.csswg.org/css-overflow-5/#scroll-container-scroll.
  Element* originating_element = OriginatingElement();
  if (!originating_element) {
    return false;
  }
  auto* scroller = DynamicTo<LayoutBox>(originating_element->GetLayoutObject());
  if (!scroller || !scroller->IsScrollContainer()) {
    return false;
  }
  ScrollMarkerPseudoElement* selected = nullptr;
  PhysicalOffset scroll_offset = scroller->ScrolledContentOffset();
  for (ScrollMarkerPseudoElement* scroll_marker : ScrollMarkers()) {
    if (!selected) {
      selected = scroll_marker;
    }
    const LayoutBox* target_box =
        scroll_marker->OriginatingElement()->GetLayoutBox();
    if (!target_box) {
      continue;
    }
    PhysicalBoxStrut scroll_margin =
        target_box->Style() ? target_box->Style()->ScrollMarginStrut()
                            : PhysicalBoxStrut();
    // Ignore sticky position offsets for the purposes of scrolling elements
    // into view. See https://www.w3.org/TR/css-position-3/#stickypos-scroll for
    // details
    const MapCoordinatesFlags flag =
        (RuntimeEnabledFeatures::CSSPositionStickyStaticScrollPositionEnabled())
            ? kIgnoreStickyOffset
            : 0;
    PhysicalRect rect_to_scroll = scroller->AbsoluteToLocalRect(
        target_box->AbsoluteBoundingBoxRectForScrollIntoView(), flag);
    rect_to_scroll.Expand(scroll_margin);
    ScrollableArea* scrollable_area = scroller->GetScrollableArea();
    CHECK(scrollable_area);
    ScrollOffset target_scroll_offset =
        scroll_into_view_util::GetScrollOffsetToExpose(
            *scrollable_area, rect_to_scroll, scroll_margin,
            scroll_into_view_util::PhysicalAlignmentFromSnapAlignStyle(
                *target_box, kHorizontalScroll),
            scroll_into_view_util::PhysicalAlignmentFromSnapAlignStyle(
                *target_box, kVerticalScroll));
    PhysicalOffset target_offset(LayoutUnit(target_scroll_offset.x()),
                                 LayoutUnit(target_scroll_offset.y()));
    // Note: use of abs here is determined by the fact that for direction: rtl
    // the scroll offset starts at zero and goes to the negative side, all the
    // target offsets go to the negative side as well. We can't end up in
    // situation of scroll offset to be on the wrong side of zero, so it's safe
    // to do so.
    // TODO(crbug.com/332396355): We should not really have to check the
    // min/max-offsets.
    ScrollOffset max_offset = scrollable_area->MaximumScrollOffset();
    ScrollOffset min_offset = scrollable_area->MinimumScrollOffset();
    if ((target_offset.left.Abs() <= scroll_offset.left.Abs() ||
         max_offset.x() == min_offset.x()) &&
        (target_offset.top.Abs() <= scroll_offset.top.Abs() ||
         max_offset.y() == min_offset.y())) {
      selected = scroll_marker;
    }
  }
  if (selected) {
    return SetSelected(*selected);
  }
  return false;
}

void ScrollMarkerGroupPseudoElement::UpdateSnapshot() {
  UpdateSelectedScrollMarker();
}

bool ScrollMarkerGroupPseudoElement::ValidateSnapshot() {
  return !UpdateSelectedScrollMarker();
}

bool ScrollMarkerGroupPseudoElement::ShouldScheduleNextService() {
  return false;
}

}  // namespace blink
