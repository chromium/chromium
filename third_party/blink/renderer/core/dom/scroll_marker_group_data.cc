// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <third_party/blink/renderer/core/dom/scroll_marker_group_data.h>

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"

namespace blink {

namespace {

Element* ScrollTargetElement(Element* scroll_marker) {
  if (auto* scroll_marker_pseudo =
          DynamicTo<ScrollMarkerPseudoElement>(scroll_marker)) {
    return &scroll_marker_pseudo->UltimateOriginatingElement();
  }
  if (auto* anchor_scroll_marker =
          DynamicTo<HTMLAnchorElement>(scroll_marker)) {
    return anchor_scroll_marker->ScrollTargetElement();
  }
  return scroll_marker;
}

}  // namespace

std::optional<ScrollMarkerChooser::ScrollTargetOffsetData>
ScrollMarkerChooser::GetScrollTargetOffsetData(Element* scroll_marker) {
  Element* target = ScrollTargetElement(scroll_marker);
  if (!target) {
    return std::nullopt;
  }
  const LayoutBox* target_box = target->GetLayoutBox();
  if (!target_box) {
    return std::nullopt;
  }
  // TODO(sakhapov): Typically, we use the bounding box of the target box as the
  // rectangle to scroll into view, as we are not scrolling the scroll marker
  // into view, but its target.
  // However, AbsoluteBoundingBoxRectForScrollIntoView() expects to be invoked
  // on the marker instead of the target box for the ::scroll-marker pseudo
  // element. That method uses that marker box to e.g. find the correct ::column
  // rectangle to scroll to.
  const LayoutObject* bounding_box_object =
      scroll_marker->IsScrollMarkerPseudoElement()
          ? scroll_marker->GetLayoutObject()
          : target_box;
  CHECK(bounding_box_object);
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
      bounding_box_object->AbsoluteBoundingBoxRectForScrollIntoView(), flag);
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

HeapVector<Member<Element>> ScrollMarkerChooser::Choose() {
  if (min_position_ == max_position_) {
    return candidates_;
  }

  bool within_start = intended_position_ < min_position_ + reserved_length_;
  bool within_end = intended_position_ > max_position_ - reserved_length_;
  HeapVector<Member<Element>> selection;
  if (within_start || within_end) {
    selection = ChooseReserved(candidates_);
  }

  if (selection.empty()) {
    // This is independent of the within_{start, end} check because it can
    // happen that we are within the reserved region but the scroll
    // targets are positioned such that the first target is beyond the
    // reserved region. In this case we should use generic selection.
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

HeapVector<Member<Element>> ScrollMarkerChooser::ChooseReserved(
    const HeapVector<Member<Element>>& candidates) {
  bool within_start = intended_position_ < min_position_ + reserved_length_;

  // First, find all candidates within the reserved region. Group candidates
  // with the same offset together so we don't split the reserved range over
  // more candidates than necessary.
  HeapVector<Member<Element>> candidates_in_range;
  std::set<int> unique_offsets;
  for (Element* candidate : candidates) {
    std::optional<ScrollTargetOffsetData> candidate_data =
        GetScrollTargetOffsetData(candidate);
    if (!candidate_data) {
      continue;
    }
    float candidate_offset = candidate_data->aligned_scroll_offset;
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
  HeapVector<Member<Element>> selection;
  if (candidates_in_range.size()) {
    const int num_within_range = candidates_in_range.size();
    const float range_start =
        within_start ? min_position_ : max_position_ - reserved_length_;
    int winning_index_within_reserved =
        ((intended_position_ - range_start) / reserved_length_) *
        num_within_range;
    winning_index_within_reserved =
        std::clamp(winning_index_within_reserved, 0, num_within_range - 1);
    Element* winning_candidate =
        candidates_in_range[winning_index_within_reserved];

    const ScrollTargetOffsetData winning_candidate_data =
        *GetScrollTargetOffsetData(winning_candidate);
    const float winning_offset = winning_candidate_data.aligned_scroll_offset;
    for (Element* candidate : candidates) {
      const std::optional<ScrollTargetOffsetData> offset_data =
          GetScrollTargetOffsetData(candidate);
      if (!offset_data) {
        continue;
      }
      const float candidate_offset = offset_data->aligned_scroll_offset;
      // TODO: Some epsilon tolerance?
      if (candidate_offset == winning_offset) {
        selection.push_back(candidate);
      }
    }
  }

  return selection;
}

HeapVector<Member<Element>> ScrollMarkerChooser::ChooseGeneric(
    const HeapVector<Member<Element>>& candidates) {
  HeapVector<Member<Element>> selection;
  std::optional<float> smallest_distance;
  for (Element* scroll_marker : candidates) {
    std::optional<ScrollTargetOffsetData> target_data =
        GetScrollTargetOffsetData(scroll_marker);
    if (!target_data) {
      continue;
    }
    float candidate_position = target_data->aligned_scroll_offset;
    float candidate_distance =
        std::abs(candidate_position - intended_position_);

    if (selection.empty()) {
      selection.push_back(scroll_marker);
      smallest_distance = candidate_distance;
      continue;
    }

    if (candidate_distance < smallest_distance) {
      smallest_distance = candidate_distance;
      selection.clear();
      selection.push_back(scroll_marker);
    } else if (candidate_distance == smallest_distance) {
      selection.push_back(scroll_marker);
    }
  }
  return selection;
}

HeapVector<Member<Element>> ScrollMarkerChooser::ChooseVisual(
    const HeapVector<Member<Element>>& candidates) {
  HeapVector<Member<Element>> selection;

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
  for (Element* candidate : candidates) {
    std::optional<ScrollTargetOffsetData> target_data =
        GetScrollTargetOffsetData(candidate);
    if (!target_data) {
      continue;
    }
    float candidate_position = target_data->layout_offset;
    if (within_end) {
      candidate_position += target_data->layout_size;
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

void ScrollMarkerGroupData::AddToFocusGroup(Element& scroll_marker) {
  DCHECK(scroll_marker.IsScrollMarkerPseudoElement() ||
         scroll_marker.HasTagName(html_names::kATag));
  // We need to update scrollers map for this scroll marker group if we
  // have added HTMLAnchorElement.
  if (scroll_marker.HasTagName(html_names::kATag)) {
    SetNeedsScrollersMapUpdate();
    scroll_marker.GetDocument().SetNeedsScrollMarkerGroupsMapUpdate();
    scroll_marker.SetScrollMarkerGroupContainerData(this);
  }
  focus_group_.push_back(scroll_marker);
}

void ScrollMarkerGroupData::RemoveFromFocusGroup(Element& scroll_marker) {
  if (wtf_size_t index = focus_group_.Find(scroll_marker); index != kNotFound) {
    focus_group_.EraseAt(index);
    // We need to update scrollers map for this scroll marker group if we
    // have added HTMLAnchorElement.
    if (scroll_marker.HasTagName(html_names::kATag)) {
      SetNeedsScrollersMapUpdate();
      scroll_marker.GetDocument().SetNeedsScrollMarkerGroupsMapUpdate();
      scroll_marker.SetScrollMarkerGroupContainerData(nullptr);
    }
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

void ScrollMarkerGroupData::ClearFocusGroup() {
  focus_group_.clear();
}

bool ScrollMarkerGroupData::SetSelected(Element* scroll_marker,
                                        bool apply_snap_alignment) {
  if (selected_marker_ == scroll_marker) {
    return false;
  }
  pending_selected_marker_.Clear();
  if (auto* scroll_marker_pseudo =
          DynamicTo<ScrollMarkerPseudoElement>(selected_marker_.Get())) {
    scroll_marker_pseudo->SetSelected(false);
    // When updating the active marker the following is meant to ensure that
    // if the previously active marker was focused we update the focus to the
    // new active marker.
    if (scroll_marker_pseudo->IsFocused()) {
      scroll_marker_pseudo->GetDocument().SetFocusedElement(
          scroll_marker, FocusParams(SelectionBehaviorOnFocus::kNone,
                                     mojom::blink::FocusType::kNone,
                                     /*capabilities=*/nullptr));
    }
  }
  if (auto* anchor_scroll_marker =
          DynamicTo<HTMLAnchorElement>(selected_marker_.Get())) {
    anchor_scroll_marker->PseudoStateChanged(CSSSelector::kPseudoTargetCurrent);
  }
  selected_marker_ = scroll_marker;
  if (!scroll_marker) {
    return true;
  }
  if (auto* scroll_marker_pseudo =
          DynamicTo<ScrollMarkerPseudoElement>(scroll_marker)) {
    scroll_marker_pseudo->SetSelected(true, apply_snap_alignment);
  }
  if (auto* anchor_scroll_marker =
          DynamicTo<HTMLAnchorElement>(scroll_marker)) {
    anchor_scroll_marker->PseudoStateChanged(CSSSelector::kPseudoTargetCurrent);
  }
  return true;
}

Element* ScrollMarkerGroupData::Selected() const {
  return selected_marker_;
}

Element* ScrollMarkerGroupData::ChooseMarker(
    const ScrollOffset& scroll_offset,
    ScrollableArea* scrollable_area,
    LayoutBox* scroller_box,
    const HeapVector<Member<Element>>& candidates) {
  using ScrollAxis = ScrollMarkerChooser::ScrollAxis;
  // The primary axis is, by default, the block axis.
  ScrollAxis primary_axis =
      IsHorizontalWritingMode(scroller_box->Style()->GetWritingMode())
          ? ScrollAxis::kY
          : ScrollAxis::kX;

  Element* selected = nullptr;

  ScrollMarkerChooser primary_chooser(
      scroll_offset, primary_axis, scrollable_area, candidates, scroller_box);
  HeapVector<Member<Element>> primary_selection = primary_chooser.Choose();
  if (primary_selection.size() == 1) {
    selected = primary_selection.at(0);
  } else {
    const ScrollAxis secondary_axis =
        primary_axis == ScrollAxis::kY ? ScrollAxis::kX : ScrollAxis::kY;
    const HeapVector<Member<Element>>& secondary_candidates =
        primary_selection.empty() ? candidates : primary_selection;
    ScrollMarkerChooser secondary_chooser(scroll_offset, secondary_axis,
                                          scrollable_area, secondary_candidates,
                                          scroller_box);
    HeapVector<Member<Element>> secondary_selection =
        secondary_chooser.Choose();
    if (!secondary_selection.empty()) {
      selected = secondary_selection.at(secondary_selection.size() - 1);
    }
  }

  return selected;
}

namespace {

Node* NearestCommonAncestorScrollContainer(
    const HeapVector<Member<Node>>& scroll_containers) {
  DCHECK(!scroll_containers.empty());
  Node* nearest_common_ancestor = scroll_containers.front();
  for (Node* scroller : scroll_containers) {
    // Not all scroll markers have scroll target or not all scroll targets have
    // scroller ancestor.
    if (!scroller) {
      nearest_common_ancestor = nullptr;
      break;
    }
    if (nearest_common_ancestor) {
      nearest_common_ancestor = nearest_common_ancestor->CommonAncestor(
          *scroller, LayoutTreeBuilderTraversal::Parent);
    }
  }
  for (Node* ancestor = nearest_common_ancestor; ancestor;
       ancestor = LayoutTreeBuilderTraversal::Parent(*ancestor)) {
    const LayoutObject* object = ancestor->GetLayoutObject();
    if (object && object->IsScrollContainer()) {
      return ancestor;
    }
  }
  return nullptr;
}

Node* NearestScrollContainer(const Node& node) {
  for (Node* ancestor = LayoutTreeBuilderTraversal::Parent(node); ancestor;
       ancestor = LayoutTreeBuilderTraversal::Parent(*ancestor)) {
    if (ancestor->GetLayoutObject() &&
        ancestor->GetLayoutObject()->IsScrollContainer()) {
      return ancestor;
    }
  }
  return nullptr;
}

}  // namespace

// This function follows:
// https://drafts.csswg.org/css-overflow-5/#example-d2ca6884.
Element* ScrollMarkerGroupData::ChooseMarkerRecursively() {
  if (focus_group_.empty()) {
    return nullptr;
  }
  HeapVector<Member<Element>> scroll_marker_targets;
  HeapVector<Member<Node>> nearest_ancestor_scroll_container;
  for (Element* scroll_marker : focus_group_) {
    Element* target = scroll_marker->IsScrollMarkerPseudoElement()
                          ? scroll_marker
                          : ScrollTargetElement(scroll_marker);
    scroll_marker_targets.push_back(target);
    nearest_ancestor_scroll_container.push_back(
        target ? NearestScrollContainer(*target) : nullptr);
  }
  // 1. Let scroller be the nearest common ancestor scroll container of all of
  // the scroll marker elements in group.
  Node* scroller =
      NearestCommonAncestorScrollContainer(nearest_ancestor_scroll_container);
  // 2. Let active be scroller.
  Node* active = scroller;
  // 3. While active is a scroll container containing scroll target elements
  // targeted by group:
  while (active && active->GetLayoutObject() &&
         active->GetLayoutObject()->IsScrollContainer()) {
    // 3.1. Let scroller be active.
    scroller = active;
    // 3.2. Let targets be: and the scroll container elements
    // which contain scroll target elements targeted by the scroll marker group
    // whose nearest ancestor scroll container is scroller.
    HeapVector<Member<Element>> targets;
    for (wtf_size_t i = 0; i < focus_group_.size(); ++i) {
      Element* scroll_marker = focus_group_[i];
      Element* target = scroll_marker_targets[i];
      Node* target_scroller = nearest_ancestor_scroll_container[i];
      // 3.2.a. Let targets be the set of the scroll target elements whose
      // nearest ancestor scroll container is scroller and the scroll container
      // elements which contain scroll target elements targeted by the scroll
      // marker group whose nearest ancestor scroll container is scroller.
      if (target && target_scroller == scroller) {
        // Adding scroll_marker here instead of target, as later
        // the algo relies on candidates to be scroll markers.
        // TODO(sakhapov): rewrite algo to use targets instead,
        // currently blocked by ::column::scroll-marker's bounding box.
        targets.push_back(scroll_marker);
      }
      // 3.2.b. the scroll container elements which contain scroll target
      // elements targeted by the scroll marker group whose nearest ancestor
      // scroll container is scroller.
      if (target_scroller &&
          NearestScrollContainer(*target_scroller) == scroller) {
        // The only Node scroller is viewscroll, which will never be
        // target_scroller.
        DCHECK(target_scroller->IsElementNode());
        targets.push_back(To<Element>(target_scroller));
      }
    }
    // Stop if `active` does not contain scroll target elements targeted by
    // group.
    if (targets.empty()) {
      break;
    }
    LayoutBox* scroller_box = scroller->GetLayoutBox();
    DCHECK(scroller_box);
    ScrollableArea* scrollable_area = scroller_box->GetScrollableArea();
    DCHECK(scrollable_area);
    // 3.3. Otherwise.
    active =
        ChooseMarker(scrollable_area->GetScrollOffsetForScrollMarkerUpdate(),
                     scrollable_area, scroller_box, targets);
  }
  // 4. Let selected marker be the scroll marker associated with active. If
  // multiple scroll marker elements are associated with active, set selected
  // marker to be the marker that is earliest in tree order among them.
  // 5. Return selected marker.
  return DynamicTo<Element>(active);
}

void ScrollMarkerGroupData::UpdateSelectedScrollMarker() {
  if (selected_marker_is_pinned_) {
    return;
  }

  if (Element* selected = ChooseMarkerRecursively()) {
    // We avoid calling ScrollMarkerPseudoElement::SetSelected here so as not to
    // cause style to be dirty right after layout, which might violate lifecycle
    // expectations.
    pending_selected_marker_ = selected;
  }
}

void ScrollMarkerGroupData::UpdateScrollableAreaSubscriptions(
    HeapHashSet<Member<PaintLayerScrollableArea>>& scrollable_areas) {
  if (!needs_scrollers_map_update_) {
    return;
  }
  for (PaintLayerScrollableArea* scrollable_area : scrollable_areas) {
    scrollable_area->RemoveScrollMarkerGroupContainerData(this);
  }
  scrollable_areas.clear();
  for (Element* anchor_scroll_marker : focus_group_) {
    if (PaintLayerScrollableArea* scrollable_area =
            To<HTMLAnchorElement>(anchor_scroll_marker)
                ->AncestorScrollableAreaOfScrollTargetElement()) {
      scrollable_areas.insert(scrollable_area);
      scrollable_area->AddScrollMarkerGroupContainerData(this);
    }
  }
  needs_scrollers_map_update_ = false;
}

Element* ScrollMarkerGroupData::FindNextScrollMarker(const Element* current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[index == focus_group_.size() - 1 ? 0u : index + 1];
  }
  return nullptr;
}

Element* ScrollMarkerGroupData::FindPreviousScrollMarker(
    const Element* current) {
  if (wtf_size_t index = focus_group_.Find(current); index != kNotFound) {
    return focus_group_[index == 0u ? focus_group_.size() - 1 : index - 1];
  }
  return nullptr;
}

bool ScrollMarkerGroupData::UpdateSnapshotInternal() {
  if (pending_selected_marker_) {
    return SetSelected(pending_selected_marker_);
  }
  return false;
}

void ScrollMarkerGroupData::UpdateSnapshot() {
  UpdateSnapshotInternal();
}

bool ScrollMarkerGroupData::ValidateSnapshot() {
  return !UpdateSnapshotInternal();
}

bool ScrollMarkerGroupData::ShouldScheduleNextService() {
  return false;
}

void ScrollMarkerGroupData::Trace(Visitor* v) const {
  v->Trace(selected_marker_);
  v->Trace(pending_selected_marker_);
  v->Trace(focus_group_);
  ScrollSnapshotClient::Trace(v);
  ElementRareDataField::Trace(v);
}

}  // namespace blink
