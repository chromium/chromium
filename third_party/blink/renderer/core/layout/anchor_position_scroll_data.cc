// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/non_overflowing_scroll_range.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace {

// Finds the LayoutObject of the anchor element given by anchor-default.
const LayoutObject* AnchorDefaultObject(const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    return nullptr;
  }
  const LayoutBox* box = To<LayoutBox>(layout_object);
  const ComputedStyle& style = box->StyleRef();
  return style.AnchorDefault() ? box->FindTargetAnchor(*style.AnchorDefault())
                               : box->AcceptableImplicitAnchor();
}

// Finds the LayoutObject of the element given by position-fallback-bounds.
const LayoutObject* PositionFallbackBoundsObject(
    const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned() ||
      !layout_object->StyleRef().PositionFallbackBounds()) {
    return nullptr;
  }

  return To<LayoutBox>(layout_object)
      ->FindTargetAnchor(*layout_object->StyleRef().PositionFallbackBounds());
}

const Vector<NonOverflowingScrollRange>* GetNonOverflowingScrollRanges(
    const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    return nullptr;
  }
  CHECK(layout_object->IsBox());
  return To<LayoutBox>(layout_object)->PositionFallbackNonOverflowingRanges();
}

// First return value for x axis, second for y axis.
std::pair<bool, bool> CheckHasDefaultAnchorReferences(
    const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    return std::make_pair(false, false);
  }
  CHECK(layout_object->IsBox());
  const LayoutBox* box = To<LayoutBox>(layout_object);
  return std::make_pair(box->NeedsAnchorPositionScrollAdjustmentInX(),
                        box->NeedsAnchorPositionScrollAdjustmentInY());
}

AnchorPositionScrollData::ScrollContainersData GetScrollContainersData(
    const LayoutObject* layout_object,
    const LayoutObject* anchor_or_bounds,
    bool accumulate_offsets_in_x = true,
    bool accumulate_offsets_in_y = true) {
  AnchorPositionScrollData::ScrollContainersData result;
  if (!layout_object || !anchor_or_bounds ||
      (!accumulate_offsets_in_x && !accumulate_offsets_in_y)) {
    return result;
  }

  CHECK(layout_object->IsBox());
  const PaintLayer* starting_layer =
      anchor_or_bounds->ContainingScrollContainerLayer(
          true /*ignore_layout_view_for_fixed_pos*/);
  const PaintLayer* bounding_layer =
      layout_object->ContainingScrollContainerLayer(
          true /*ignore_layout_view_for_fixed_pos*/);
  for (const PaintLayer* layer = starting_layer;
       layer && layer != bounding_layer;
       layer = layer->GetLayoutObject().ContainingScrollContainerLayer(
           true /*ignore_layout_view_for_fixed_pos*/)) {
    const PaintLayerScrollableArea* scrollable_area =
        layer->GetScrollableArea();
    result.scroll_container_ids.push_back(
        scrollable_area->GetScrollElementId());
    result.accumulated_scroll_offset += scrollable_area->GetScrollOffset();
    result.accumulated_scroll_origin +=
        scrollable_area->ScrollOrigin().OffsetFromOrigin();
    if (scrollable_area->GetLayoutBox()->IsLayoutView()) {
      result.scroll_containers_include_viewport = true;
    }
  }

  if (!accumulate_offsets_in_x) {
    result.accumulated_scroll_offset.set_x(0);
    result.accumulated_scroll_origin.set_x(0);
  }
  if (!accumulate_offsets_in_y) {
    result.accumulated_scroll_offset.set_y(0);
    result.accumulated_scroll_origin.set_y(0);
  }
  return result;
}

}  // namespace

AnchorPositionScrollData::AnchorPositionScrollData(Element* element)
    : ScrollSnapshotClient(element->GetDocument().GetFrame()),
      owner_(element) {}

AnchorPositionScrollData::~AnchorPositionScrollData() = default;

bool AnchorPositionScrollData::IsActive() const {
  return owner_->GetAnchorPositionScrollData() == this;
}

AnchorPositionScrollData::SnapshotDiff
AnchorPositionScrollData::TakeAndCompareSnapshot(bool update) {
  DCHECK(IsActive());

  const LayoutObject* layout_object = owner_->GetLayoutObject();
  bool needs_scroll_adjustment_in_x;
  bool needs_scroll_adjustment_in_y;
  std::tie(needs_scroll_adjustment_in_x, needs_scroll_adjustment_in_y) =
      CheckHasDefaultAnchorReferences(layout_object);

  const LayoutObject* anchor_default_object =
      AnchorDefaultObject(layout_object);
  ScrollContainersData new_scrollers_data = GetScrollContainersData(
      layout_object, anchor_default_object, needs_scroll_adjustment_in_x,
      needs_scroll_adjustment_in_y);
  if (!new_scrollers_data.scroll_container_ids.size()) {
    needs_scroll_adjustment_in_x = false;
    needs_scroll_adjustment_in_y = false;
  }

  gfx::Vector2dF new_additional_bounds_scroll_offset;
  if (const LayoutObject* position_fallback_bounds_object =
          PositionFallbackBoundsObject(layout_object)) {
    new_additional_bounds_scroll_offset =
        GetScrollContainersData(layout_object, position_fallback_bounds_object)
            .accumulated_scroll_offset;
  }

  SnapshotDiff diff;
  if (scroll_container_ids_ != new_scrollers_data.scroll_container_ids) {
    diff = SnapshotDiff::kScrollersOrFallbackPosition;
  } else {
    const bool anchor_scrolled =
        accumulated_scroll_offset_ !=
            new_scrollers_data.accumulated_scroll_offset ||
        accumulated_scroll_origin_ !=
            new_scrollers_data.accumulated_scroll_origin;
    const bool additional_bounds_scrolled =
        additional_bounds_scroll_offset_ != new_additional_bounds_scroll_offset;
    if ((anchor_scrolled || additional_bounds_scrolled) &&
        !IsFallbackPositionValid(new_scrollers_data.accumulated_scroll_offset,
                                 new_additional_bounds_scroll_offset)) {
      diff = SnapshotDiff::kScrollersOrFallbackPosition;
    } else if (anchor_scrolled ||
               needs_scroll_adjustment_in_x_ != needs_scroll_adjustment_in_x ||
               needs_scroll_adjustment_in_y_ != needs_scroll_adjustment_in_y) {
      // When needs_scroll_adjustment_in_x/y changes, we still need to update
      // paint properties so that compositor can calculate the translation
      // offset correctly.
      diff = SnapshotDiff::kOffsetOnly;
    } else {
      // When the additional bounds rect is scrolled without invalidating the
      // current fallback position, `owner_` doesn't need paint update.
      diff = SnapshotDiff::kNone;
    }
  }

  if (update && diff != SnapshotDiff::kNone) {
    scroll_container_ids_.swap(new_scrollers_data.scroll_container_ids);
    accumulated_scroll_offset_ = new_scrollers_data.accumulated_scroll_offset;
    accumulated_scroll_origin_ = new_scrollers_data.accumulated_scroll_origin;
    additional_bounds_scroll_offset_ = new_additional_bounds_scroll_offset;
    is_affected_by_viewport_scrolling_ =
        new_scrollers_data.scroll_containers_include_viewport;
    needs_scroll_adjustment_in_x_ = needs_scroll_adjustment_in_x;
    needs_scroll_adjustment_in_y_ = needs_scroll_adjustment_in_y;
  }

  return diff;
}

bool AnchorPositionScrollData::IsFallbackPositionValid(
    const gfx::Vector2dF& new_accumulated_scroll_offset,
    const gfx::Vector2dF& new_additional_bounds_scroll_offset) const {
  const Vector<NonOverflowingScrollRange>* non_overflowing_scroll_ranges =
      GetNonOverflowingScrollRanges(owner_->GetLayoutObject());
  if (!non_overflowing_scroll_ranges ||
      non_overflowing_scroll_ranges->empty()) {
    return true;
  }

  for (const NonOverflowingScrollRange& range :
       *non_overflowing_scroll_ranges) {
    if (range.Contains(accumulated_scroll_offset_,
                       additional_bounds_scroll_offset_) !=
        range.Contains(new_accumulated_scroll_offset,
                       new_additional_bounds_scroll_offset)) {
      return false;
    }
  }
  return true;
}

void AnchorPositionScrollData::UpdateSnapshot() {
  if (!IsActive()) {
    return;
  }

  SnapshotDiff diff = TakeAndCompareSnapshot(true /* update */);
  switch (diff) {
    case SnapshotDiff::kNone:
      return;
    case SnapshotDiff::kOffsetOnly:
      InvalidatePaint();
      return;
    case SnapshotDiff::kScrollersOrFallbackPosition:
      InvalidateLayoutAndPaint();
      return;
  }
}

bool AnchorPositionScrollData::ValidateSnapshot() {
  if (is_snapshot_validated_) {
    return true;
  }
  is_snapshot_validated_ = true;

  // If this AnchorPositionScrollData is detached in the previous style recalc,
  // we no longer need to validate it.
  if (!IsActive()) {
    return true;
  }

  SnapshotDiff diff = TakeAndCompareSnapshot(true /* update */);
  switch (diff) {
    case SnapshotDiff::kNone:
    case SnapshotDiff::kOffsetOnly:
      // We don't need to rewind to layout recalc for offset-only diff, as this
      // function is called at LayoutClean during lifecycle update, and
      // offset-only diff only needs paint update.
      return true;
    case SnapshotDiff::kScrollersOrFallbackPosition:
      InvalidateLayoutAndPaint();
      return false;
  }
}

bool AnchorPositionScrollData::ShouldScheduleNextService() {
  return IsActive() &&
         TakeAndCompareSnapshot(false /*update*/) != SnapshotDiff::kNone;
}

void AnchorPositionScrollData::InvalidateLayoutAndPaint() {
  DCHECK(IsActive());
  DCHECK(owner_->GetLayoutObject());
  owner_->GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kAnchorPositioning);
  owner_->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
}

void AnchorPositionScrollData::InvalidatePaint() {
  DCHECK(IsActive());
  DCHECK(owner_->GetLayoutObject());
  owner_->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
}

void AnchorPositionScrollData::Trace(Visitor* visitor) const {
  visitor->Trace(owner_);
  ScrollSnapshotClient::Trace(visitor);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
