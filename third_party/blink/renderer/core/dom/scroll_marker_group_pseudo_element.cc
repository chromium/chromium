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

ScrollMarkerChooser::ScrollAxis GetPrimaryScrollAxis(
    const ScrollOffset& min_offset,
    const ScrollOffset& max_offset) {
  using ScrollAxis = ScrollMarkerChooser::ScrollAxis;
  const float vertical_range = std::abs(max_offset.y() - min_offset.y());
  const float horizontal_range = std::abs(max_offset.x() - min_offset.x());
  return vertical_range >= horizontal_range ? ScrollAxis::kY : ScrollAxis::kX;
}

ScrollMarkerChooser::ScrollTargetOffsetData
ScrollMarkerChooser::GetScrollTargetOffsetData(
    const ScrollMarkerPseudoElement* scroll_marker) {
  const LayoutBox* target_box =
      scroll_marker->UltimateOriginatingElement()->GetLayoutBox();
  CHECK(target_box);
  const LayoutObject* scroll_marker_object = scroll_marker->GetLayoutObject();
  CHECK(scroll_marker_object);
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
  PhysicalRect rect_to_scroll = scroller_box_->AbsoluteToLocalRect(
      scroll_marker_object->AbsoluteBoundingBoxRectForScrollIntoView(), flag);
  rect_to_scroll.Expand(scroll_margin);
  ScrollOffset target_scroll_offset =
      scroll_into_view_util::GetScrollOffsetToExpose(
          *scrollable_area_, rect_to_scroll, scroll_margin,
          scroll_into_view_util::PhysicalAlignmentFromSnapAlignStyle(
              *target_box, kHorizontalScroll),
          scroll_into_view_util::PhysicalAlignmentFromSnapAlignStyle(
              *target_box, kVerticalScroll));
  // The result of GetScrollOffsetToExpose is adjusted for the current scroll
  // offset. Undo this adjustment as ScrollTargetOffsetData::layout_offset
  // represents the offset in coordinates within the scrollable content
  // area.
  const ScrollOffset current_scroll_offset =
      scrollable_area_->GetScrollOffset();
  float current_scroll_position = axis_ == ScrollAxis::kY
                                      ? current_scroll_offset.y()
                                      : current_scroll_offset.x();
  return axis_ == ScrollAxis::kY
             ? ScrollTargetOffsetData(
                   target_scroll_offset.y(),
                   rect_to_scroll.Y() + current_scroll_position,
                   rect_to_scroll.size.height)
             : ScrollTargetOffsetData(
                   target_scroll_offset.x(),
                   rect_to_scroll.X() + current_scroll_position,
                   rect_to_scroll.size.width);
}

ScrollMarkerGroupPseudoElement::ScrollMarkerGroupPseudoElement(
    Element* originating_element,
    PseudoId pseudo_id)
    : PseudoElement(originating_element, pseudo_id),
      ScrollSnapshotClient(originating_element->GetDocument().GetFrame()) {}

void ScrollMarkerGroupPseudoElement::Trace(Visitor* v) const {
  v->Trace(selected_marker_);
  v->Trace(pending_selected_marker_);
  v->Trace(focus_group_);
  PseudoElement::Trace(v);
}

void ScrollMarkerGroupPseudoElement::AddToFocusGroup(
    ScrollMarkerPseudoElement& scroll_marker) {
  focus_group_.push_back(scroll_marker);
}

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::FindNextScrollMarker(
    const Element* current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[std::min(index + 1, focus_group_.size() - 1)];
  }
  return nullptr;
}

ScrollMarkerPseudoElement*
ScrollMarkerGroupPseudoElement::FindPreviousScrollMarker(
    const Element* current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[index == 0 ? 0u : index - 1];
  }
  return nullptr;
}

void ScrollMarkerGroupPseudoElement::RemoveFromFocusGroup(
    const ScrollMarkerPseudoElement& scroll_marker) {
  if (wtf_size_t index = focus_group_.Find(scroll_marker); index != kNotFound) {
    focus_group_.EraseAt(index);
    if (selected_marker_ == scroll_marker) {
      if (index == focus_group_.size()) {
        if (index == 0) {
          selected_marker_ = nullptr;
          return;
        }
        --index;
      }
      selected_marker_ = focus_group_[index];
    }
  }
}

void ScrollMarkerGroupPseudoElement::ActivateNextScrollMarker() {
  ActivateScrollMarker(FindNextScrollMarker(Selected()));
}

void ScrollMarkerGroupPseudoElement::ActivatePrevScrollMarker() {
  ActivateScrollMarker(FindPreviousScrollMarker(Selected()));
}

void ScrollMarkerGroupPseudoElement::ActivateScrollMarker(
    ScrollMarkerPseudoElement* scroll_marker) {
  if (!scroll_marker || scroll_marker == selected_marker_) {
    return;
  }
  // parentElement is ::column for column scroll marker and
  // ultimate originating element for regular scroll marker.
  mojom::blink::ScrollIntoViewParamsPtr params =
      scroll_into_view_util::CreateScrollIntoViewParams(
          *scroll_marker->parentElement()->GetComputedStyle());
  scroll_marker->ScrollIntoViewNoVisualUpdate(std::move(params));
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
  pending_selected_marker_.Clear();
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

ScrollMarkerPseudoElement* ScrollMarkerGroupPseudoElement::ChooseMarker(
    const ScrollOffset& scroll_offset,
    ScrollableArea* scrollable_area,
    LayoutBox* scroller_box) {
  using ScrollAxis = ScrollMarkerChooser::ScrollAxis;
  ScrollOffset max_offset = scrollable_area->MaximumScrollOffset();
  ScrollOffset min_offset = scrollable_area->MinimumScrollOffset();
  ScrollAxis primary_axis = GetPrimaryScrollAxis(min_offset, max_offset);

  ScrollMarkerPseudoElement* selected = nullptr;

  ScrollMarkerChooser primary_chooser(scroll_offset, primary_axis,
                                      scrollable_area, ScrollMarkers(),
                                      scroller_box);
  HeapVector<Member<ScrollMarkerPseudoElement>> primary_selection =
      primary_chooser.Choose();
  if (primary_selection.size() == 1) {
    selected = primary_selection.at(0);
  } else {
    const ScrollAxis secondary_axis =
        primary_axis == ScrollAxis::kY ? ScrollAxis::kX : ScrollAxis::kY;
    const HeapVector<Member<ScrollMarkerPseudoElement>>& secondary_candidates =
        primary_selection.empty() ? ScrollMarkers() : primary_selection;
    ScrollMarkerChooser secondary_chooser(scroll_offset, secondary_axis,
                                          scrollable_area, secondary_candidates,
                                          scroller_box);
    HeapVector<Member<ScrollMarkerPseudoElement>> secondary_selection =
        secondary_chooser.Choose();
    if (!secondary_selection.empty()) {
      selected = secondary_selection.at(secondary_selection.size() - 1);
    }
  }

  return selected;
}

bool ScrollMarkerGroupPseudoElement::UpdateSelectedScrollMarker(
    const ScrollOffset& offset) {
  // Implements scroll tracking for scroll marker controls as per
  // https://drafts.csswg.org/css-overflow-5/#scroll-container-scroll.
  Element* originating_element = UltimateOriginatingElement();
  if (!originating_element) {
    return false;
  }
  auto* scroller = DynamicTo<LayoutBox>(originating_element->GetLayoutObject());
  if (!scroller || !scroller->IsScrollContainer()) {
    return false;
  }
  ScrollableArea* scrollable_area = scroller->GetScrollableArea();
  CHECK(scrollable_area);

  if (ScrollMarkerPseudoElement* selected =
          ChooseMarker(offset, scrollable_area, scroller)) {
    // We avoid calling ScrollMarkerPseudoElement::SetSelected here so as not to
    // cause style to be dirty right after layout, which might violate lifecycle
    // expectations.
    pending_selected_marker_ = selected;
  }
  return false;
}

bool ScrollMarkerGroupPseudoElement::UpdateSnapshotInternal() {
  if (pending_selected_marker_) {
    return SetSelected(*pending_selected_marker_);
  }
  return false;
}

void ScrollMarkerGroupPseudoElement::UpdateSnapshot() {
  UpdateSnapshotInternal();
}

bool ScrollMarkerGroupPseudoElement::ValidateSnapshot() {
  return !UpdateSnapshotInternal();
}

bool ScrollMarkerGroupPseudoElement::ShouldScheduleNextService() {
  return false;
}

void ScrollMarkerGroupPseudoElement::DetachLayoutTree(
    bool performing_reattach) {
  // Swap out the focus_group_ before iterating because
  // ScrollMarkerPseudoElement::DetachLayoutTree() will modify focus_group_.
  HeapVector<Member<ScrollMarkerPseudoElement>> focus_group;
  std::swap(focus_group_, focus_group);
  for (ScrollMarkerPseudoElement* scroll_marker : focus_group) {
    scroll_marker->DetachLayoutTree(performing_reattach);
  }
  selected_marker_ = nullptr;
  pending_selected_marker_ = nullptr;
  PseudoElement::DetachLayoutTree(performing_reattach);
}

HeapVector<Member<ScrollMarkerPseudoElement>> ScrollMarkerChooser::Choose() {
  if (min_position_ == max_position_) {
    return candidates_;
  }

  bool within_start = intended_position_ < min_position_ + reserved_length_;
  bool within_end = intended_position_ > max_position_ - reserved_length_;
  HeapVector<Member<ScrollMarkerPseudoElement>> selection;
  if (within_start || within_end) {
    selection = ChooseReserved(candidates_);
  } else {
    selection = ChooseGeneric(candidates_);
  }

  if (selection.size() > 1) {
    // There may be more than one item whose aligned scroll positions are the
    // same. We might be able to separate them based on their visual/layout
    // positions.
    selection = ChooseVisual(selection);
  }

  return selection;
}

HeapVector<Member<ScrollMarkerPseudoElement>>
ScrollMarkerChooser::ChooseReserved(
    const HeapVector<Member<ScrollMarkerPseudoElement>>& candidates) {
  bool within_start = intended_position_ < min_position_ + reserved_length_;

  // First, find all candidates within the reserved region. Group candidates
  // with the same offset together so we don't split the reserved range over
  // more candidates than necessary.
  HeapVector<Member<ScrollMarkerPseudoElement>> candidates_in_range;
  std::set<int> unique_offsets;
  for (const auto& candidate : candidates) {
    ScrollTargetOffsetData candidate_data =
        GetScrollTargetOffsetData(candidate);
    float candidate_offset = candidate_data.aligned_scroll_offset;
    bool keep_candidate =
        within_start ? (candidate_offset < min_position_ + reserved_length_)
                     : (candidate_offset > max_position_ - reserved_length_);
    if (keep_candidate) {
      int floored_offset = std::floor(candidate_offset);
      auto find_it = unique_offsets.find(floored_offset);
      if (find_it == unique_offsets.end()) {
        unique_offsets.insert(floored_offset);
        candidates_in_range.push_back(candidate);
      }
    }
  }

  // Next, extract only the candidate(s) at the offset that corresponds to the
  // scroller's position within the reserved region.
  HeapVector<Member<ScrollMarkerPseudoElement>> selection;
  if (candidates_in_range.size()) {
    const int num_within_range = candidates_in_range.size();
    const float range_start =
        within_start ? min_position_ : max_position_ - reserved_length_;
    int winning_index_within_reserved =
        ((intended_position_ - range_start) / reserved_length_) *
        num_within_range;
    winning_index_within_reserved =
        std::clamp(winning_index_within_reserved, 0, num_within_range - 1);
    const ScrollMarkerPseudoElement* winning_candidate =
        candidates_in_range[winning_index_within_reserved];

    const ScrollTargetOffsetData winning_candidate_data =
        GetScrollTargetOffsetData(winning_candidate);
    const float winning_offset = winning_candidate_data.aligned_scroll_offset;
    for (const auto& candidate : candidates) {
      const ScrollTargetOffsetData offset_data =
          GetScrollTargetOffsetData(candidate);
      const float candidate_offset = offset_data.aligned_scroll_offset;
      // TODO: Some epsilon tolerance?
      if (candidate_offset == winning_offset) {
        selection.push_back(candidate);
      }
    }
  }

  return selection;
}

HeapVector<Member<ScrollMarkerPseudoElement>>
ScrollMarkerChooser::ChooseGeneric(
    const HeapVector<Member<ScrollMarkerPseudoElement>>& candidates) {
  HeapVector<Member<ScrollMarkerPseudoElement>> selection;
  std::optional<float> max_observed_position;
  for (ScrollMarkerPseudoElement* scroll_marker : candidates) {
    if (selection.empty()) {
      selection.push_back(scroll_marker);
      continue;
    }
    ScrollTargetOffsetData target_data =
        GetScrollTargetOffsetData(scroll_marker);
    float candidate_position = target_data.aligned_scroll_offset;

    if (candidate_position <= intended_position_) {
      if (!max_observed_position ||
          (candidate_position > max_observed_position)) {
        max_observed_position = candidate_position;
        selection.clear();
        selection.push_back(scroll_marker);
      } else if (candidate_position == max_observed_position) {
        selection.push_back(scroll_marker);
      }
    }
  }
  return selection;
}

HeapVector<Member<ScrollMarkerPseudoElement>> ScrollMarkerChooser::ChooseVisual(
    const HeapVector<Member<ScrollMarkerPseudoElement>>& candidates) {
  HeapVector<Member<ScrollMarkerPseudoElement>> selection;

  bool within_end = intended_position_ > max_position_ - reserved_length_;
  // If we are using the scroll targets' layout positions, pick the one whose
  // start is closest to the start of the scrollport, unless we are in the end
  // region in which case the winner is the one whose end edge is closest to the
  // end of the scrollport. This allows a scroll container at the end of the
  // scrollable content to be selected even if its start edge cannot be reached.
  float scroll_position = intended_position_;
  if (within_end) {
    scroll_position +=
        (axis_ == ScrollAxis::kY ? scrollable_area_->VisibleHeight()
                                 : scrollable_area_->VisibleWidth());
  }

  std::optional<float> smallest_distance;
  for (auto& candidate : candidates) {
    ScrollTargetOffsetData target_data = GetScrollTargetOffsetData(candidate);
    float candidate_position = target_data.layout_offset;
    if (within_end) {
      candidate_position += target_data.layout_size;
    }

    float distance = std::abs(candidate_position - scroll_position);
    if (!smallest_distance) {
      smallest_distance = distance;
      selection.push_back(candidate);
      continue;
    }

    if (distance < smallest_distance) {
      smallest_distance = distance;
      selection.clear();
      selection.push_back(candidate);
    } else if (distance == smallest_distance) {
      selection.push_back(candidate);
    }
  }

  return selection;
}

}  // namespace blink
