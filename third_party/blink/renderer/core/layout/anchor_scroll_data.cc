// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_scroll_data.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_anchor_query.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace {

// Finds the LayoutObject of the anchor element given by anchor-scroll.
const LayoutObject* AnchorScrollObject(const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned())
    return nullptr;
  const ComputedStyle& style = layout_object->StyleRef();
  const AnchorSpecifierValue* value = style.AnchorScroll();
  if (!value)
    return nullptr;

  const LayoutBox* box = To<LayoutBox>(layout_object);
  const LayoutObject* anchor = nullptr;
  if (value->IsNamed()) {
    anchor = box->FindTargetAnchor(value->GetName());
  } else if (value->IsDefault() && style.AnchorDefault()) {
    anchor = box->FindTargetAnchor(*style.AnchorDefault());
  } else {
    DCHECK(value->IsImplicit() ||
           (value->IsDefault() && !style.AnchorDefault()));
    anchor = box->AcceptableImplicitAnchor();
  }

  return anchor;
}

// Returns the PaintLayer of the scroll container of |anchor|.
const PaintLayer* ContainingScrollContainerForAnchor(
    const LayoutObject* anchor) {
  if (!anchor->HasLayer())
    return anchor->ContainingScrollContainer()->Layer();
  // Normally, |scroller_layer| is the result. There's only one special case
  // where |anchor| is fixed-positioned and |scroller_layer| is the LayoutView,
  // then |anchor| doesn't actually scroll with |scroller_layer|, and null
  // should be returned.
  bool is_fixed_to_view = false;
  const PaintLayer* scroller_layer =
      To<LayoutBoxModelObject>(anchor)->Layer()->ContainingScrollContainerLayer(
          &is_fixed_to_view);
  return is_fixed_to_view ? nullptr : scroller_layer;
}

// Returns the PaintLayer of the scroll container of an anchor-positioned |box|.
const PaintLayer* ContainingScrollContainerLayerForAnchorPositionedBox(
    const LayoutBox* box) {
  // Normally, |scroller_layer| is the result. There's only one special case
  // where |box| is fixed-positioned and |scroller_layer| is the LayoutView,
  // then |box| doesn't actually scroll with |scroller_layer|, and null should
  // be returned.
  bool is_fixed_to_view = false;
  const PaintLayer* scroller_layer =
      box->Layer()->ContainingScrollContainerLayer(&is_fixed_to_view);
  return is_fixed_to_view ? nullptr : scroller_layer;
}

const Vector<PhysicalScrollRange>* GetNonOverflowingScrollRanges(
    const LayoutObject* layout_object) {
  if (!layout_object || !layout_object->IsOutOfFlowPositioned()) {
    return nullptr;
  }
  DCHECK(layout_object->IsBox());
  const auto& layout_results = To<LayoutBox>(layout_object)->GetLayoutResults();
  if (layout_results.empty()) {
    return nullptr;
  }
  // TODO(crbug.com/1309178): Make sure it works when the box is fragmented.
  return layout_results.front()->PositionFallbackNonOverflowingRanges();
}

}  // namespace

AnchorScrollData::AnchorScrollData(Element* element)
    : ScrollSnapshotClient(element->GetDocument().GetFrame()),
      owner_(element) {}

AnchorScrollData::~AnchorScrollData() = default;

bool AnchorScrollData::IsActive() const {
  return owner_->GetAnchorScrollData() == this;
}

AnchorScrollData::SnapshotDiff AnchorScrollData::TakeAndCompareSnapshot(
    bool update) {
  DCHECK(IsActive());

  Vector<CompositorElementId> new_scroll_container_ids;
  gfx::Vector2dF new_accumulated_scroll_offset;
  gfx::Vector2d new_accumulated_scroll_origin;

  if (const LayoutObject* anchor =
          AnchorScrollObject(owner_->GetLayoutObject())) {
    const PaintLayer* starting_layer =
        ContainingScrollContainerForAnchor(anchor);
    const PaintLayer* bounding_layer =
        ContainingScrollContainerLayerForAnchorPositionedBox(
            owner_->GetLayoutBox());
    for (const PaintLayer* layer = starting_layer; layer != bounding_layer;
         layer = layer->ContainingScrollContainerLayer()) {
      // |bounding_layer| must be either null (for fixed-positioned |owner_|) or
      // an ancestor of |starting_layer|, so we'll never have a null layer here.
      DCHECK(layer);
      const PaintLayerScrollableArea* scrollable_area =
          layer->GetScrollableArea();
      new_scroll_container_ids.push_back(scrollable_area->GetScrollElementId());
      new_accumulated_scroll_offset += scrollable_area->GetScrollOffset();
      new_accumulated_scroll_origin +=
          scrollable_area->ScrollOrigin().OffsetFromOrigin();
    }
  }

  SnapshotDiff diff;
  if (scroll_container_ids_ != new_scroll_container_ids) {
    diff = SnapshotDiff::kScrollersOrFallbackPosition;
  } else if (accumulated_scroll_offset_ != new_accumulated_scroll_offset ||
             accumulated_scroll_origin_ != new_accumulated_scroll_origin) {
    diff = IsFallbackPositionValid(new_accumulated_scroll_offset)
               ? SnapshotDiff::kOffsetOnly
               : SnapshotDiff::kScrollersOrFallbackPosition;
  } else {
    diff = SnapshotDiff::kNone;
  }

  if (update && diff != SnapshotDiff::kNone) {
    scroll_container_ids_.swap(new_scroll_container_ids);
    accumulated_scroll_offset_ = new_accumulated_scroll_offset;
    accumulated_scroll_origin_ = new_accumulated_scroll_origin;
  }

  return diff;
}

bool AnchorScrollData::IsFallbackPositionValid(
    const gfx::Vector2dF& new_accumulated_scroll_offset) const {
  const Vector<PhysicalScrollRange>* non_overflowing_scroll_ranges =
      GetNonOverflowingScrollRanges(owner_->GetLayoutObject());
  if (!non_overflowing_scroll_ranges ||
      non_overflowing_scroll_ranges->empty()) {
    return true;
  }

  for (const PhysicalScrollRange& range : *non_overflowing_scroll_ranges) {
    if (range.Contains(accumulated_scroll_offset_) !=
        range.Contains(new_accumulated_scroll_offset)) {
      return false;
    }
  }
  return true;
}

void AnchorScrollData::UpdateSnapshot() {
  if (!IsActive())
    return;

  SnapshotDiff diff = TakeAndCompareSnapshot(true /* update */);
  switch (diff) {
    case SnapshotDiff::kNone:
      return;
    case SnapshotDiff::kOffsetOnly:
      InvalidatePaint();
      return;
    case SnapshotDiff::kScrollersOrFallbackPosition:
      InvalidateLayout();
      return;
  }
}

bool AnchorScrollData::ValidateSnapshot() {
  if (is_snapshot_validated_) {
    return true;
  }
  is_snapshot_validated_ = true;

  // If this AnchorScrollData is detached in the previous style recalc, we no
  // longer need to validate it.
  if (!IsActive())
    return true;

  SnapshotDiff diff = TakeAndCompareSnapshot(true /* update */);
  switch (diff) {
    case SnapshotDiff::kNone:
    case SnapshotDiff::kOffsetOnly:
      // We don't need to rewind to layout recalc for offset-only diff, as this
      // function is called at LayoutClean during lifecycle update, and
      // offset-only diff only needs paint update.
      return true;
    case SnapshotDiff::kScrollersOrFallbackPosition:
      InvalidateLayout();
      return false;
  }
}

bool AnchorScrollData::ShouldScheduleNextService() {
  return IsActive() &&
         TakeAndCompareSnapshot(false /*update*/) != SnapshotDiff::kNone;
}

void AnchorScrollData::InvalidateLayout() {
  DCHECK(IsActive());
  DCHECK(owner_->GetLayoutObject());
  owner_->GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
      layout_invalidation_reason::kAnchorPositioning);
}

void AnchorScrollData::InvalidatePaint() {
  DCHECK(IsActive());
  DCHECK(owner_->GetLayoutObject());
  // TODO(crbug.com/1309178): This causes a main frame commit, which is
  // unnecessary when there's offset-only changes and compositor has already
  // adjusted the element correctly. Try to avoid that. See also
  // crbug.com/1378705 as sticky position has the same issue.
  owner_->GetLayoutObject()->SetNeedsPaintPropertyUpdate();
}

void AnchorScrollData::Trace(Visitor* visitor) const {
  visitor->Trace(owner_);
  ScrollSnapshotClient::Trace(visitor);
  ElementRareDataField::Trace(visitor);
}

}  // namespace blink
