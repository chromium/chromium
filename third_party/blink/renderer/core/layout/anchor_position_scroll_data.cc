// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_position_scroll_data.h"

#include "third_party/blink/renderer/core/css/out_of_flow_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/layout/anchor_position_visibility_observer.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/non_overflowing_scroll_range.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace {

// Finds the LayoutObject of the anchor element given by position-anchor.
const LayoutObject* PositionAnchorObject(const LayoutBox& box) {
  const StylePositionAnchor& position_anchor = box.StyleRef().PositionAnchor();
  using Type = StylePositionAnchor::Type;
  switch (position_anchor.GetType()) {
    case Type::kNone:
      return nullptr;
    case Type::kAuto:
      return box.AcceptableImplicitAnchor();
    case Type::kName:
      return box.FindTargetAnchor(position_anchor.GetName());
  }
}

const HeapVector<NonOverflowingScrollRange>* GetNonOverflowingScrollRanges(
    const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    return nullptr;
  }
  CHECK(layout_object->IsBox());
  return To<LayoutBox>(layout_object)->NonOverflowingScrollRanges();
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

AnchorPositionScrollData::AnchorPositionScrollData(Element* anchored_element)
    : PostLayoutSnapshotClient(anchored_element->GetDocument().GetFrame()),
      anchored_element_(anchored_element) {}

AnchorPositionScrollData::~AnchorPositionScrollData() = default;

bool AnchorPositionScrollData::IsActive() const {
  return anchored_element_->GetAnchorPositionScrollData() == this;
}

PhysicalOffset AnchorPositionScrollData::TotalOffset(
    const LayoutObject* anchor_object) const {
  if (!anchor_object ||
      (default_anchor_adjustment_data_.anchor_element &&
       anchor_object ==
           default_anchor_adjustment_data_.anchor_element->GetLayoutObject())) {
    return default_anchor_adjustment_data_.TotalOffset();
  }

  return ComputeAdjustmentContainersData(anchored_element_, *anchor_object)
      .TotalOffset();
}

PhysicalOffset
AnchorPositionScrollData::SpeculativeDefaultAnchorRememberedOffset() const {
  OutOfFlowData* out_of_flow_data = anchored_element_->GetOutOfFlowData();

  const OutOfFlowData::RememberedScrollOffsets* offsets =
      out_of_flow_data
          ? out_of_flow_data->GetSpeculativeRememberedScrollOffsets()
          : nullptr;

  if (offsets) {
    return offsets
        ->GetOffsetForAnchor(default_anchor_adjustment_data_.anchor_element)
        .value_or(PhysicalOffset());
  }
  return PhysicalOffset();
}

// static
AnchorPositionScrollData::AdjustmentData
AnchorPositionScrollData::ComputeAdjustmentContainersData(
    const Element* anchored_element,
    const LayoutObject& anchor) {
  CHECK(anchored_element->GetLayoutObject());
  AnchorPositionScrollData::AdjustmentData result;

  auto container_ignore_layout_view_for_fixed_pos =
      [](const LayoutObject& o) -> const LayoutObject* {
    const auto* container = o.Container();
    if (o.IsFixedPositioned() && container->IsLayoutView()) {
      return nullptr;
    }
    return container;
  };

  auto may_need_scroll_adjustment = [](const LayoutBox* box) -> bool {
    if (RuntimeEnabledFeatures::
            AnchorPositionAdjustmentWithoutOverflowEnabled()) {
      if (box->IsLayoutView()) {
        // We may need to adjust scroll for overscroll effects, even if there
        // is no scrollable overflow.
        if (box->GetDocument()
                .GetPage()
                ->GetVisualViewport()
                .GetOverscrollType() == OverscrollType::kTransform) {
          return true;
        }
      }
    }
    return box->HasScrollableOverflow();
  };

  const auto* anchor_element = DynamicTo<Element>(anchor.GetNode());
  CHECK(anchor_element);
  result.anchor_element = anchor_element;
  const auto* bounding_container = container_ignore_layout_view_for_fixed_pos(
      *anchored_element->GetLayoutObject());

  if (bounding_container && bounding_container->IsScrollContainer()) {
    const ScrollableArea* scrollable_area =
        To<LayoutBox>(bounding_container)->GetScrollableArea();
    result.anchored_element_container_scroll_offset =
        PhysicalOffset::FromVector2dFFloor(scrollable_area->GetScrollOffset());
  }

  for (const auto* container = &anchor;
       container && container != bounding_container;
       container = container_ignore_layout_view_for_fixed_pos(*container)) {
    if (container->IsScrollContainer()) {
      const PaintLayerScrollableArea* scrollable_area =
          To<LayoutBox>(container)->GetScrollableArea();
      if (container != anchor && container != bounding_container &&
          may_need_scroll_adjustment(To<LayoutBox>(container))) {
        result.adjustment_container_ids.push_back(
            scrollable_area->GetScrollElementId());
        result.accumulated_adjustment += PhysicalOffset::FromVector2dFFloor(
            scrollable_area->GetScrollOffset());
        result.accumulated_adjustment_scroll_origin +=
            scrollable_area->ScrollOrigin().OffsetFromOrigin();
        if (scrollable_area->GetLayoutBox()->IsLayoutView()) {
          result.containers_include_viewport = true;
        }
      }
    }
    if (const auto* box_model = DynamicTo<LayoutBoxModelObject>(container)) {
      if (box_model->StickyConstraints()) {
        result.adjustment_container_ids.push_back(
            CompositorElementIdFromUniqueObjectId(
                box_model->UniqueId(),
                CompositorElementIdNamespace::kStickyTranslation));
        result.accumulated_adjustment -= box_model->StickyPositionOffset();
      }
    }
    if (const auto* box = DynamicTo<LayoutBox>(container)) {
      if (auto* data = box->GetAnchorPositionScrollData()) {
        result.has_chained_anchor = true;
        if (data->NeedsScrollAdjustment()) {
          // Add accumulated offset from chained anchor-positioned element.
          // If the data of that element is not up-to-date, when it's updated,
          // we'll schedule needed update according to the type of the change.
          result.adjustment_container_ids.push_back(
              CompositorElementIdFromUniqueObjectId(
                  box->UniqueId(), CompositorElementIdNamespace::
                                       kAnchorPositionScrollTranslation));
          result.accumulated_adjustment +=
              data->ComputeDefaultAnchorAdjustmentData().accumulated_adjustment;
        }
      }
    }
  }
  return result;
}

AnchorPositionScrollData::AdjustmentData
AnchorPositionScrollData::ComputeDefaultAnchorAdjustmentData() const {
  const LayoutObject* layout_object = anchored_element_->GetLayoutObject();
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

  auto result = ComputeAdjustmentContainersData(anchored_element_,
                                                *anchor_default_object);
  if (result.adjustment_container_ids.empty()) {
    needs_scroll_adjustment_in_x = false;
    needs_scroll_adjustment_in_y = false;
  }
  // These don't reset anchored_element_container_scroll_offset because the
  // scroll container always scrolls the anchored element.
  if (!needs_scroll_adjustment_in_x) {
    result.accumulated_adjustment.left = LayoutUnit();
    result.accumulated_adjustment_scroll_origin.set_x(0);
  }
  if (!needs_scroll_adjustment_in_y) {
    result.accumulated_adjustment.top = LayoutUnit();
    result.accumulated_adjustment_scroll_origin.set_y(0);
  }
  result.needs_scroll_adjustment_in_x = needs_scroll_adjustment_in_x;
  result.needs_scroll_adjustment_in_y = needs_scroll_adjustment_in_y;
  return result;
}

AnchorPositionScrollData::SnapshotDiff
AnchorPositionScrollData::TakeAndCompareSnapshot(bool update) {
  DCHECK(IsActive());

  AdjustmentData new_adjustment_data = ComputeDefaultAnchorAdjustmentData();

  SnapshotDiff diff = SnapshotDiff::kNone;
  if (default_anchor_adjustment_data_.anchor_element !=
          new_adjustment_data.anchor_element ||
      AdjustmentContainerIds() !=
          new_adjustment_data.adjustment_container_ids ||
      !IsFallbackPositionValid(new_adjustment_data)) {
    diff = SnapshotDiff::kScrollersOrFallbackPosition;
  } else if (NeedsScrollAdjustmentInX() !=
                 new_adjustment_data.needs_scroll_adjustment_in_x ||
             NeedsScrollAdjustmentInY() !=
                 new_adjustment_data.needs_scroll_adjustment_in_y ||
             default_anchor_adjustment_data_.TotalOffset() !=
                 new_adjustment_data.TotalOffset() ||
             AccumulatedAdjustmentScrollOrigin() !=
                 new_adjustment_data.accumulated_adjustment_scroll_origin) {
    // When needs_scroll_adjustment_in_x/y changes, we still need to update
    // paint properties so that compositor can calculate the translation
    // offset correctly.
    diff = SnapshotDiff::kOffsetOnly;
  }

  if (update && diff != SnapshotDiff::kNone) {
    default_anchor_adjustment_data_ = std::move(new_adjustment_data);
  }

  return diff;
}

bool AnchorPositionScrollData::IsFallbackPositionValid(
    const AdjustmentData& new_adjustment_data) const {
  const HeapVector<NonOverflowingScrollRange>* non_overflowing_scroll_ranges =
      GetNonOverflowingScrollRanges(anchored_element_->GetLayoutObject());
  if (!non_overflowing_scroll_ranges ||
      non_overflowing_scroll_ranges->empty()) {
    return true;
  }

  for (const NonOverflowingScrollRange& range :
       *non_overflowing_scroll_ranges) {
    const Element* range_element = new_adjustment_data.anchor_element;
    const Element* new_element = range.anchor_element;
    const LayoutObject* range_object =
        range_element ? range_element->GetLayoutObject() : nullptr;
    const LayoutObject* new_object =
        new_element ? new_element->GetLayoutObject() : nullptr;
    if (new_object != range_object) {
      // The range was calculated with a different anchor object. Check if the
      // anchored element (which previously overflowed with the try option that
      // specified that anchor) will become non-overflowing with that option.
      if (range.Contains(TotalOffset(range_object))) {
        return false;
      }
    } else {
      // The range was calculated with the same anchor object as this data.
      // Check if the overflow status of the anchored element will change with
      // the new total offset.
      if (range.Contains(default_anchor_adjustment_data_.TotalOffset()) !=
          range.Contains(new_adjustment_data.TotalOffset())) {
        return false;
      }
    }
  }
  return true;
}

bool AnchorPositionScrollData::UpdateSnapshot() {
  // If this AnchorPositionScrollData is detached in the previous style recalc,
  // we no longer need to validate it.
  if (!IsActive()) {
    return false;
  }

  SnapshotDiff diff = TakeAndCompareSnapshot(true /* update */);
  switch (diff) {
    case SnapshotDiff::kNone:
      return false;
    case SnapshotDiff::kOffsetOnly:
      InvalidatePaint();
      return false;
    case SnapshotDiff::kScrollersOrFallbackPosition:
      InvalidateLayoutAndPaint();
      return true;
  }
}

bool AnchorPositionScrollData::ShouldScheduleNextService() {
  return IsActive() &&
         TakeAndCompareSnapshot(false /*update*/) != SnapshotDiff::kNone;
}

AnchorPositionVisibilityObserver&
AnchorPositionScrollData::EnsureAnchorPositionVisibilityObserver() {
  if (!position_visibility_observer_) {
    position_visibility_observer_ =
        MakeGarbageCollected<AnchorPositionVisibilityObserver>(
            *anchored_element_);
  }
  return *position_visibility_observer_;
}

void AnchorPositionScrollData::InvalidateLayoutAndPaint() {
  // Temporary workaround for https://crbug.com/395057435: Skip invalidation if
  // this has been detached.
  if (!IsActive()) {
    return;
  }
  DCHECK(anchored_element_->GetLayoutObject());

  if (OutOfFlowData* out_of_flow_data = anchored_element_->GetOutOfFlowData()) {
    out_of_flow_data->ClearRememberedScrollOffsets();
  }
  anchored_element_->GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kAnchorPositioning);
  anchored_element_->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
}

void AnchorPositionScrollData::InvalidatePaint() {
  // Temporary workaround for https://crbug.com/395057435: Skip invalidation if
  // this has been detached.
  if (!IsActive()) {
    return;
  }
  DCHECK(anchored_element_->GetLayoutObject());
  anchored_element_->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
}

void AnchorPositionScrollData::Trace(Visitor* visitor) const {
  visitor->Trace(anchored_element_);
  visitor->Trace(default_anchor_adjustment_data_);
  visitor->Trace(position_visibility_observer_);
  PostLayoutSnapshotClient::Trace(visitor);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
