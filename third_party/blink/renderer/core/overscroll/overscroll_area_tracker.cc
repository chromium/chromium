// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/overscroll/overscroll_area_tracker.h"

#include "cc/input/scroll_snap_data.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

PaintLayerScrollableArea* GetScrollableAreaFor(Element* overscroll_area) {
  auto* overscroll_area_parent =
      overscroll_area->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  if (!overscroll_area_parent) {
    return nullptr;
  }
  auto* overscroll_area_object =
      DynamicTo<LayoutBox>(overscroll_area_parent->GetLayoutObject());
  if (!overscroll_area_object) {
    return nullptr;
  }
  return DynamicTo<PaintLayerScrollableArea>(
      overscroll_area_object->GetScrollableArea());
}

void ScrollTo(PaintLayerScrollableArea* scrollable_area, ScrollOffset offset) {
  ScrollOffset old_offset = scrollable_area->GetScrollOffset();
  bool x_changed = offset.x() != old_offset.x();
  bool y_changed = offset.y() != old_offset.y();

  std::unique_ptr<cc::SnapSelectionStrategy> strategy =
      cc::SnapSelectionStrategy::CreateForEndPosition(
          scrollable_area->ScrollOffsetToPosition(offset), x_changed,
          y_changed);
  std::optional<gfx::PointF> snap_point =
      scrollable_area->GetSnapPositionAndSetTarget(*strategy);
  if (snap_point.has_value()) {
    offset = scrollable_area->ScrollPositionToOffset(snap_point.value());
  }

  scrollable_area->SetScrollOffset(offset,
                                   mojom::blink::ScrollType::kProgrammatic,
                                   cc::ScrollSourceType::kAbsoluteScroll,
                                   mojom::blink::ScrollBehavior::kAuto);
}

}  // namespace

OverscrollAreaTracker::OverscrollAreaTracker(Element* element)
    : container_(element) {}

void OverscrollAreaTracker::AddOverscroll(Element* element) {
  CHECK(!element->GetOverscrollContainer());
  DCHECK(element->isConnected());
  element->SetOverscrollContainer(container_);
  overscroll_members_.push_back(element);
  container_->SetNeedsReattachLayoutTree();
  needs_dom_sort_ = overscroll_members_.size() > 1;
}

const VectorOf<Element>& OverscrollAreaTracker::DOMSortedElements() {
  if (needs_dom_sort_) {
    std::sort(overscroll_members_.begin(), overscroll_members_.end(),
              [](const Member<Element>& a, const Member<Element>& b) {
                return a->compareDocumentPosition(b) &
                       Node::kDocumentPositionFollowing;
              });
    needs_dom_sort_ = false;
  }
  return overscroll_members_;
}

void OverscrollAreaTracker::RemoveAllOverscroll() {
  for (auto& member : overscroll_members_) {
    member->ClearOverscrollContainer();
  }
  overscroll_members_.clear();
  needs_dom_sort_ = false;
}

void OverscrollAreaTracker::RemoveOverscroll(Element* element) {
  CHECK_EQ(element->GetOverscrollContainer(), container_);
  element->ClearOverscrollContainer();
  Erase(overscroll_members_, element);
  needs_dom_sort_ = needs_dom_sort_ && overscroll_members_.size() > 1;
}

void OverscrollAreaTracker::ToggleArea(Element* overscroll_area) {
  CHECK(RuntimeEnabledFeatures::OverscrollGesturesEnabled());
  auto* scrollable_area = GetScrollableAreaFor(overscroll_area);
  if (!scrollable_area) {
    return;
  }

  const cc::SnapContainerData* container_data =
      scrollable_area->GetSnapContainerData();
  CHECK(container_data && container_data->size() >= 2);

  const cc::TargetSnapAreaElementIds& previous_snap_targets =
      container_data->GetTargetSnapAreaElementIds();
  const auto& first_data = container_data->at(0);

  if (previous_snap_targets.x == first_data.element_id &&
      previous_snap_targets.y == first_data.element_id) {
    OpenArea(overscroll_area);
  } else {
    CloseArea(overscroll_area);
  }
}

void OverscrollAreaTracker::OpenArea(Element* overscroll_area) {
  CHECK(RuntimeEnabledFeatures::OverscrollGesturesEnabled());
  auto* scrollable_area = GetScrollableAreaFor(overscroll_area);
  if (!scrollable_area) {
    return;
  }

  const cc::SnapContainerData* container_data =
      scrollable_area->GetSnapContainerData();
  if (!container_data || container_data->size() < 2) {
    return;
  }

  auto* overscroll_area_parent =
      overscroll_area->GetPseudoElement(kPseudoIdOverscrollAreaParent);
  auto* overscroll_area_object =
      DynamicTo<LayoutBox>(overscroll_area_parent->GetLayoutObject());
  CHECK(overscroll_area_object);

  const auto& second_data = container_data->at(1);
  gfx::PointF scroll_origin(scrollable_area->ScrollOrigin());
  gfx::RectF target_rect = second_data.rect;
  PhysicalSize box_size = overscroll_area_object->PhysicalContentBoxSize();

  float min_x_offset = std::min(target_rect.x() - scroll_origin.x(), 0.f);
  float min_y_offset = std::min(target_rect.y() - scroll_origin.y(), 0.f);
  float max_x_offset = std::max(
      target_rect.right() - box_size.width.ToFloat() - scroll_origin.x(), 0.f);
  float max_y_offset = std::max(
      target_rect.bottom() - box_size.height.ToFloat() - scroll_origin.y(),
      0.f);

  ScrollOffset new_offset;
  if (std::max(-min_x_offset, max_x_offset) >
      std::max(-min_y_offset, max_y_offset)) {
    new_offset.set_x(-min_x_offset >= max_x_offset ? min_x_offset
                                                   : max_x_offset);
  } else {
    new_offset.set_y(-min_y_offset >= max_y_offset ? min_y_offset
                                                   : max_y_offset);
  }

  ScrollTo(scrollable_area, new_offset);
}

void OverscrollAreaTracker::CloseArea(Element* overscroll_area) {
  CHECK(RuntimeEnabledFeatures::OverscrollGesturesEnabled());
  auto* scrollable_area = GetScrollableAreaFor(overscroll_area);
  if (!scrollable_area) {
    return;
  }
  ScrollTo(scrollable_area, ScrollOffset());
}

void OverscrollAreaTracker::CloseAllAreas() {
  for (auto& member : overscroll_members_) {
    CloseArea(member);
  }
}

void OverscrollAreaTracker::Trace(Visitor* visitor) const {
  ElementRareDataField::Trace(visitor);

  visitor->Trace(container_);
  visitor->Trace(overscroll_members_);
}

}  // namespace blink
