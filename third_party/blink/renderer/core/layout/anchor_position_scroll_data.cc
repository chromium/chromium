// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/non_overflowing_scroll_range.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace {

// Finds the LayoutObject of the anchor element given by position-anchor.
const LayoutObject* PositionAnchorObject(const LayoutBox& box) {
  const ComputedStyle& style = box.StyleRef();
  return style.PositionAnchor() ? box.FindTargetAnchor(*style.PositionAnchor())
                                : box.AcceptableImplicitAnchor();
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

}  // namespace

AnchorPositionScrollData::AnchorPositionScrollData(Element* element)
    : ScrollSnapshotClient(element->GetDocument().GetFrame()),
      owner_(element) {}

AnchorPositionScrollData::~AnchorPositionScrollData() = default;

bool AnchorPositionScrollData::IsActive() const {
  return owner_->GetAnchorPositionScrollData() == this;
}

AnchorPositionScrollData::AdjustmentData
AnchorPositionScrollData::ComputeAdjustmentContainersData(
    const LayoutObject& anchor_or_bounds) const {
  CHECK(owner_->GetLayoutObject());
  AnchorPositionScrollData::AdjustmentData result;

  auto container_ignore_layout_view_for_fixed_pos =
      [](const LayoutObject& o) -> const LayoutObject* {
    const auto* container = o.Container();
    if (o.IsFixedPositioned() && container->IsLayoutView()) {
      return nullptr;
    }
    return container;
  };

  const auto* bounding_container =
      container_ignore_layout_view_for_fixed_pos(*owner_->GetLayoutObject());
  for (const auto* container = &anchor_or_bounds;
       container && container != bounding_container;
       container = container_ignore_layout_view_for_fixed_pos(*container)) {
    if (container != &anchor_or_bounds && container->IsScrollContainer()) {
      const PaintLayerScrollableArea* scrollable_area =
          To<LayoutBox>(container)->GetScrollableArea();
      result.adjustment_container_ids.push_back(
          scrollable_area->GetScrollElementId());
      result.accumulated_offset += scrollable_area->GetScrollOffset();
      result.accumulated_scroll_origin +=
          scrollable_area->ScrollOrigin().OffsetFromOrigin();
      if (scrollable_area->GetLayoutBox()->IsLayoutView()) {
        result.containers_include_viewport = true;
      }
    }
    if (const auto* box_model = DynamicTo<LayoutBoxModelObject>(container)) {
      if (box_model->StickyConstraints()) {
        result.adjustment_container_ids.push_back(
            CompositorElementIdFromUniqueObjectId(
                box_model->UniqueId(),
                CompositorElementIdNamespace::kStickyTranslation));
        result.accumulated_offset -=
            gfx::Vector2dF(box_model->StickyPositionOffset());
      }
    }
    if (const auto* box = DynamicTo<LayoutBox>(container)) {
      if (box->NeedsAnchorPositionScrollAdjustment()) {
        // Add accumulated offset from chained anchor-positioned element.
        // If the data of that element is not up-to-date, when it's updated,
        // we'll schedule needed update according to the type of the change.
        result.adjustment_container_ids.push_back(
            CompositorElementIdFromUniqueObjectId(
                box->UniqueId(), CompositorElementIdNamespace::
                                     kAnchorPositionScrollTranslation));
        result.accumulated_offset +=
            gfx::Vector2dF(To<Element>(box->GetNode())
                               ->GetAnchorPositionScrollData()
                               ->ComputeDefaultAnchorAdjustmentData()
                               .accumulated_offset);
      }
    }
  }
  return result;
}

AnchorPositionScrollData::AdjustmentData
AnchorPositionScrollData::ComputeDefaultAnchorAdjustmentData() const {
  const LayoutObject* layout_object = owner_->GetLayoutObject();
  auto [needs_scroll_adjustment_in_x, needs_scroll_adjustment_in_y] =
      CheckHasDefaultAnchorReferences(layout_object);
  if (!needs_scroll_adjustment_in_x && !needs_scroll_adjustment_in_y) {
    return AdjustmentData();
  }

  const LayoutObject* anchor_default_object =
      PositionAnchorObject(To<LayoutBox>(*layout_object));
  if (!anchor_default_object) {
    return AdjustmentData();
  }

  auto result = ComputeAdjustmentContainersData(*anchor_default_object);
  if (result.adjustment_container_ids.empty()) {
    needs_scroll_adjustment_in_x = false;
    needs_scroll_adjustment_in_y = false;
  }
  if (!needs_scroll_adjustment_in_x) {
    result.accumulated_offset.set_x(0);
    result.accumulated_scroll_origin.set_x(0);
  }
  if (!needs_scroll_adjustment_in_y) {
    result.accumulated_offset.set_y(0);
    result.accumulated_scroll_origin.set_y(0);
  }
  result.needs_scroll_adjustment_in_x = needs_scroll_adjustment_in_x;
  result.needs_scroll_adjustment_in_y = needs_scroll_adjustment_in_y;
  return result;
}

gfx::Vector2dF AnchorPositionScrollData::ComputeAdditionalBoundsOffset() const {
  if (const LayoutObject* position_fallback_bounds_object =
          PositionFallbackBoundsObject(owner_->GetLayoutObject())) {
    return ComputeAdjustmentContainersData(*position_fallback_bounds_object)
        .accumulated_offset;
  }
  return gfx::Vector2dF();
}

AnchorPositionScrollData::SnapshotDiff
AnchorPositionScrollData::TakeAndCompareSnapshot(bool update) {
  DCHECK(IsActive());

  AdjustmentData new_adjustment_data = ComputeDefaultAnchorAdjustmentData();
  gfx::Vector2dF new_additional_bounds_offset = ComputeAdditionalBoundsOffset();

  SnapshotDiff diff;
  if (AdjustmentContainerIds() !=
      new_adjustment_data.adjustment_container_ids) {
    diff = SnapshotDiff::kScrollersOrFallbackPosition;
  } else {
    const bool anchor_scrolled =
        AccumulatedOffset() != new_adjustment_data.accumulated_offset ||
        AccumulatedScrollOrigin() !=
            new_adjustment_data.accumulated_scroll_origin;
    const bool additional_bounds_scrolled =
        additional_bounds_offset_ != new_additional_bounds_offset;
    if ((anchor_scrolled || additional_bounds_scrolled) &&
        !IsFallbackPositionValid(new_adjustment_data.accumulated_offset,
                                 new_additional_bounds_offset)) {
      diff = SnapshotDiff::kScrollersOrFallbackPosition;
    } else if (anchor_scrolled ||
               NeedsScrollAdjustmentInX() !=
                   new_adjustment_data.needs_scroll_adjustment_in_x ||
               NeedsScrollAdjustmentInY() !=
                   new_adjustment_data.needs_scroll_adjustment_in_y) {
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
    default_anchor_adjustment_data_ = std::move(new_adjustment_data);
    additional_bounds_offset_ = new_additional_bounds_offset;
  }

  return diff;
}

bool AnchorPositionScrollData::IsFallbackPositionValid(
    const gfx::Vector2dF& new_accumulated_offset,
    const gfx::Vector2dF& new_additional_bounds_offset) const {
  const Vector<NonOverflowingScrollRange>* non_overflowing_scroll_ranges =
      GetNonOverflowingScrollRanges(owner_->GetLayoutObject());
  if (!non_overflowing_scroll_ranges ||
      non_overflowing_scroll_ranges->empty()) {
    return true;
  }

  for (const NonOverflowingScrollRange& range :
       *non_overflowing_scroll_ranges) {
    if (range.Contains(AccumulatedOffset(), additional_bounds_offset_) !=
        range.Contains(new_accumulated_offset, new_additional_bounds_offset)) {
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
