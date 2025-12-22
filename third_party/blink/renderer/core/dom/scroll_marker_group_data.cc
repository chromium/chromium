// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <third_party/blink/renderer/core/dom/scroll_marker_group_data.h>

#include "third_party/blink/renderer/core/dom/column_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_group_pseudo_element.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

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

mojom::blink::ScrollAlignment GetAlignmentForScrollTarget(
    ScrollOrientation axis,
    const LayoutObject* target_object) {
  cc::ScrollSnapAlign snap = target_object->Style()->GetScrollSnapAlign();

  cc::SnapAlignment x_snap_align =
      snap.alignment_inline == cc::SnapAlignment::kNone
          ? cc::SnapAlignment::kStart
          : snap.alignment_inline;
  cc::SnapAlignment y_snap_align =
      snap.alignment_block == cc::SnapAlignment::kNone
          ? cc::SnapAlignment::kStart
          : snap.alignment_block;
  V8ScrollLogicalPosition::Enum x_position =
      scroll_into_view_util::SnapAlignmentToV8ScrollLogicalPosition(
          x_snap_align);
  V8ScrollLogicalPosition::Enum y_position =
      scroll_into_view_util::SnapAlignmentToV8ScrollLogicalPosition(
          y_snap_align);
  return scroll_into_view_util::ResolveToPhysicalAlignment(
      x_position, y_position, axis, *target_object->Style());
}

// Returns the ColumnPseudoElement that is the direct parent of this scroll
// marker, if this scroll marker is a ::column::scroll-marker and its
// scroll-marker-group is in tabs mode. Returns nullptr otherwise.
ColumnPseudoElement* GetColumnFromScrollMarkerInTabsMode(
    Element* scroll_marker) {
  auto* scroll_marker_pseudo =
      DynamicTo<ScrollMarkerPseudoElement>(scroll_marker);
  if (!scroll_marker_pseudo) {
    return nullptr;
  }
  // Only apply inactive column marking for tabs mode.
  ScrollMarkerGroupPseudoElement* scroll_marker_group =
      scroll_marker_pseudo->ScrollMarkerGroup();
  if (!scroll_marker_group || scroll_marker_group->ScrollMarkerGroupMode() !=
                                  ScrollMarkerGroup::ScrollMarkerMode::kTabs) {
    return nullptr;
  }
  return DynamicTo<ColumnPseudoElement>(scroll_marker_pseudo->parentElement());
}

}  // namespace

std::optional<double> ScrollMarkerChooser::GetScrollTargetPosition(
    Element* scroll_marker) {
  Element* target = ScrollTargetElement(scroll_marker);
  if (!target) {
    return std::nullopt;
  }
  const LayoutObject* target_object = target->GetLayoutObject();
  if (!target_object) {
    return std::nullopt;
  }
  // TODO(sakhapov): Typically, we use the bounding box of the target box as the
  // rectangle to scroll into view, as we are not scrolling the scroll marker
  // into view, but its target.
  // However, AbsoluteBoundingBoxRectForScrollIntoView() expects to be invoked
  // on the marker instead of the target box for the ::scroll-marker pseudo-
  // element. That method uses that marker box to e.g. find the correct ::column
  // rectangle to scroll to.
  const LayoutObject* bounding_box_object =
      scroll_marker->IsScrollMarkerPseudoElement()
          ? scroll_marker->GetLayoutObject()
          : target_object;
  CHECK(bounding_box_object);
  PhysicalBoxStrut scroll_margin =
      target_object->Style() ? target_object->Style()->ScrollMarginStrut()
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

  mojom::blink::ScrollAlignment align_y =
      GetAlignmentForScrollTarget(kVerticalScroll, target_object);
  mojom::blink::ScrollAlignment align_x =
      GetAlignmentForScrollTarget(kHorizontalScroll, target_object);
  ScrollOffset target_scroll_offset =
      scroll_into_view_util::GetScrollOffsetToExpose(
          *scrollable_area_, rect_to_scroll, scroll_margin, align_x, align_y);
  return axis_ == ScrollAxis::kY ? target_scroll_offset.y()
                                 : target_scroll_offset.x();
}

HeapVector<Member<Element>> ScrollMarkerChooser::Choose() {
  if (min_position_ == max_position_) {
    return candidates_;
  }

  return ChooseInternal();
}

HeapVector<Member<Element>> ScrollMarkerChooser::ComputeTargetPositions(
    HeapHashMap<Member<Element>, double>& target_positions) {
  const double distribute_range = reserved_length_;
  HeapVector<Member<Element>> before_targets;
  HeapVector<Member<Element>> after_targets;

  std::optional<double> min_candidate_position;
  std::optional<double> max_candidate_position;

  HeapVector<Member<Element>> candidates;
  for (auto& candidate : candidates_) {
    std::optional<double> position = GetScrollTargetPosition(candidate);
    if (!position) {
      continue;
    }

    double candidate_position = *position;
    if (!min_candidate_position ||
        candidate_position < min_candidate_position) {
      min_candidate_position = candidate_position;
    }
    if (!max_candidate_position ||
        candidate_position > max_candidate_position) {
      max_candidate_position = candidate_position;
    }

    target_positions.insert(candidate, candidate_position);

    if (candidate_position < min_position_ + distribute_range) {
      before_targets.push_back(candidate);
    } else if (candidate_position > max_position_ - distribute_range) {
      after_targets.push_back(candidate);
    }

    candidates.push_back(candidate);
  }

  if (!min_candidate_position || !max_candidate_position) {
    return candidates_;
  }

  // Update the target positions to account for unreachable targets.
  double before_range_start = min_position_;
  for (auto& target : before_targets) {
    double previous_target_position = target_positions.at(target);
    double current_target_position =
        ((previous_target_position - *min_candidate_position) /
         (before_range_start + distribute_range - *min_candidate_position)) *
            distribute_range +
        before_range_start;
    target_positions.Set(target, current_target_position);
  }

  double after_range_start = max_position_ - distribute_range;
  for (auto& target : after_targets) {
    double previous_target_position = target_positions.at(target);
    double current_target_position =
        ((previous_target_position - (after_range_start)) /
         (*max_candidate_position - (after_range_start))) *
            distribute_range +
        after_range_start;
    target_positions.Set(target, current_target_position);
  }

  return candidates;
}

HeapVector<Member<Element>> ScrollMarkerChooser::ChooseInternal() {
  const bool nonnegative_range = max_position_ > 0;

  // Prepare target positions.
  HeapHashMap<Member<Element>, double> target_positions;
  HeapVector<Member<Element>> candidates =
      ComputeTargetPositions(target_positions);

  DCHECK_EQ(target_positions.size(), candidates.size());

  // Sort the candidates because we don't have a guarantee about the order in
  // which we'll encounter them. If, for example, they are all at positions
  // beyond |intended_position| we want to default to the one with the lowest
  // position as, although its position has not yet been reached, it is the
  // closest one.
  std::sort(candidates.begin(), candidates.end(),
            [=](Member<Element> a, Member<Element> b) {
              // The code beyond this point normalizes the scroll offsets to
              // nonnegative scroll range for simplicity. If the original
              // scroll range is negative, sort in descending order here as this
              // will correspond to ascending order when their absolute values
              // are taken.
              return nonnegative_range
                         ? target_positions.at(a) < target_positions.at(b)
                         : target_positions.at(a) > target_positions.at(b);
            });

  // There are two kinds of candidates that we consider:
  //
  // 1. a candidate whose position is before or at the current scroll
  //    position, aka a "before_candidate", and
  // 2. a candidate whose position is after the current scroll position
  //    AND is within one-half of the scroll port length away from the scroll
  //    position, aka an "after_candidate".
  //
  // Among before candidates, we want the candidate(s) with the largest
  // position. Among after candidates we want the candidates with the smallest
  // positions.
  // Between before and after candidates, pick whichever is closest to the
  // current scroll position.
  double scrollport_length =
      (axis_ == ScrollAxis::kY ? scrollable_area_->VisibleHeight()
                               : scrollable_area_->VisibleWidth());
  double after_candidate_window = scrollport_length / 2;

  HeapVector<Member<Element>> before_winners;
  HeapVector<Member<Element>> after_winners;
  std::optional<double> best_before_pos;
  std::optional<double> best_after_pos;

  double intended_position = std::abs(intended_position_);

  for (Member<Element>& candidate : candidates) {
    double candidate_position = std::abs(target_positions.at(candidate));
    if (!best_before_pos) {
      // Default to the candidate with the lowest position.
      best_before_pos = candidate_position;
      before_winners.push_back(candidate);
      continue;
    }

    if (candidate_position <= intended_position) {
      // A before candidate.
      if (candidate_position > *best_before_pos) {
        best_before_pos = candidate_position;
        before_winners.clear();
        before_winners.push_back(candidate);
      } else if (candidate_position == *best_before_pos) {
        before_winners.push_back(candidate);
      }
    } else if (candidate_position <
               intended_position + after_candidate_window) {
      // An after candidate.
      if (!best_after_pos || candidate_position < *best_after_pos) {
        best_after_pos = candidate_position;
        after_winners.clear();
        after_winners.push_back(candidate);
      } else if (candidate_position == *best_after_pos) {
        after_winners.push_back(candidate);
      }
    }
  }

  if (!after_winners.empty()) {
    double before_distance = intended_position - *best_before_pos;
    double after_distance = *best_after_pos - intended_position;
    if (after_distance < before_distance) {
      return after_winners;
    }
  }

  return before_winners;
}

void ScrollMarkerGroupData::AddToFocusGroup(Element& scroll_marker) {
  DCHECK(scroll_marker.IsScrollMarkerPseudoElement() ||
         scroll_marker.HasTagName(html_names::kATag));
  // We need to update scrollers map for this scroll marker group if we
  // have added HTMLAnchorElement.
  if (scroll_marker.HasTagName(html_names::kATag)) {
    SetNeedsScrollersMapUpdate();
    scroll_marker.GetDocument().SetNeedsScrollTargetGroupsMapUpdate();
    scroll_marker.SetScrollTargetGroupContainerData(this);
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
      scroll_marker.GetDocument().SetNeedsScrollTargetGroupsMapUpdate();
      scroll_marker.SetScrollTargetGroupContainerData(nullptr);
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
  MaybeSetPendingSelectedMarker(nullptr, /*apply_snap_alignment=*/true);
  focus_group_.clear();
}

void ScrollMarkerGroupData::ApplyPendingScrollMarker() {
  if (invalidation_state_ == InvalidationState::kClean) {
    return;
  }
  invalidation_state_ = InvalidationState::kClean;

  // Track old and new columns for updating IsInsideInactiveColumnTab bits.
  // Only applies to scroll-marker-groups in tabs mode.
  ColumnPseudoElement* old_column =
      GetColumnFromScrollMarkerInTabsMode(selected_marker_.Get());
  ColumnPseudoElement* new_column =
      GetColumnFromScrollMarkerInTabsMode(pending_selected_marker_.Get());

  // Notify the currently selected marker before updating it.
  if (auto* scroll_marker_pseudo =
          DynamicTo<ScrollMarkerPseudoElement>(selected_marker_.Get())) {
    scroll_marker_pseudo->SetSelected(false);
    // When updating the active marker the following is meant to ensure that
    // if the previously active marker was focused we update the focus to the
    // new active marker.
    if (scroll_marker_pseudo->IsFocused()) {
      scroll_marker_pseudo->GetDocument().SetFocusedElement(
          pending_selected_marker_, FocusParams(SelectionBehaviorOnFocus::kNone,
                                                mojom::blink::FocusType::kNone,
                                                /*capabilities=*/nullptr));
    }
  }
  if (auto* anchor_scroll_marker =
          DynamicTo<HTMLAnchorElement>(selected_marker_.Get())) {
    anchor_scroll_marker->PseudoStateChanged(
        CSSSelector::PseudoType::kPseudoTargetCurrent);
  }
  selected_marker_ = pending_selected_marker_;

  // Notify the newly selected marker.
  if (auto* scroll_marker_pseudo =
          DynamicTo<ScrollMarkerPseudoElement>(selected_marker_.Get())) {
    scroll_marker_pseudo->SetSelected(true, apply_snap_alignment_);
  }
  if (auto* anchor_scroll_marker =
          DynamicTo<HTMLAnchorElement>(selected_marker_.Get())) {
    anchor_scroll_marker->PseudoStateChanged(CSSSelector::kPseudoTargetCurrent);
  }
  // Notify all scroll markers in the group that their
  // :target-before and :target-after pseudo-elements have changed.
  if (RuntimeEnabledFeatures::CSSScrollMarkerTargetBeforeAfterEnabled()) {
    for (Element* scroll_marker : focus_group_) {
      scroll_marker->PseudoStateChanged(CSSSelector::kPseudoTargetBefore);
      scroll_marker->PseudoStateChanged(CSSSelector::kPseudoTargetAfter);
    }
  }

  // Update IsInsideInactiveColumnTab bits on LayoutObjects inside columns.
  // This is used by accessibility to efficiently determine which content
  // should be hidden without walking the fragment tree for each node.
  if (old_column != new_column) {
    if (old_column) {
      // Mark all content in the old active column as inactive.
      old_column->SetIsInsideInactiveColumnTabForDescendants(true);
    } else {
      DCHECK(new_column);
      // On first selection (old_column is null), mark all other columns as
      // inactive. This must be done before marking the new column as active,
      // so that elements spanning multiple columns end up active if any of
      // their fragments is in the active column.
      Element& multicol_container = new_column->UltimateOriginatingElement();
      if (const ColumnPseudoElementsVector* columns =
              multicol_container.GetColumnPseudoElements()) {
        for (ColumnPseudoElement* column : *columns) {
          if (column != new_column) {
            column->SetIsInsideInactiveColumnTabForDescendants(true);
          }
        }
      }
    }
    if (new_column) {
      // Mark all content in the new active column as active.
      new_column->SetIsInsideInactiveColumnTabForDescendants(false);
    }
  }

  apply_snap_alignment_ = false;
  pending_selected_marker_.Clear();
}

void ScrollMarkerGroupData::MaybeSetPendingSelectedMarker(
    Element* scroll_marker,
    bool apply_snap_alignment) {
  // Don't invalidate currently selected marker if it is pinned.
  // However, if the pending selection is null, but we are pinned,
  // we should update make pending scroll marker the selected one,
  // as it means that we just received a targeted scroll, that got
  // us pinned and is setting the pending scroll marker.
  if (!selected_marker_is_pinned_) {
    SetPendingSelectedMarker(scroll_marker, apply_snap_alignment);
  }
}

void ScrollMarkerGroupData::SetPendingSelectedMarker(
    Element* scroll_marker,
    bool apply_snap_alignment) {
  invalidation_state_ = std::max(invalidation_state_,
                                 InvalidationState::kNeedsActiveMarkerUpdate);
  pending_selected_marker_ = scroll_marker;
  apply_snap_alignment_ = apply_snap_alignment;
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
  invalidation_state_ = InvalidationState::kNeedsFullUpdate;
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

bool ScrollMarkerGroupData::UpdateSnapshot() {
  if (invalidation_state_ == InvalidationState::kNeedsFullUpdate) {
    if (Element* selected = ChooseMarkerRecursively()) {
      // We avoid calling ScrollMarkerPseudoElement::SetSelected here so as not
      // to cause style to be dirty right after layout, which might violate
      // lifecycle expectations.
      MaybeSetPendingSelectedMarker(selected, /*apply_snap_alignment=*/true);
    }
  }
  if (invalidation_state_ >= InvalidationState::kNeedsActiveMarkerUpdate) {
    ApplyPendingScrollMarker();
    return true;
  }
  return false;
}

bool ScrollMarkerGroupData::ShouldScheduleNextService() {
  return false;
}

void ScrollMarkerGroupData::Trace(Visitor* v) const {
  v->Trace(selected_marker_);
  v->Trace(pending_selected_marker_);
  v->Trace(focus_group_);
  PostLayoutSnapshotClient::Trace(v);
  ElementRareDataField::Trace(v);
}

}  // namespace blink
