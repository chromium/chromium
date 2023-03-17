// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/anchor_scroll_data.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
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

  LayoutBox::NGPhysicalFragmentList containing_block_fragments =
      layout_object->ContainingBlock()->PhysicalFragments();
  if (containing_block_fragments.IsEmpty())
    return nullptr;

  // TODO(crbug.com/1309178): Fix it when the containing block is fragmented or
  // an inline box.
  const NGPhysicalAnchorQuery* anchor_query =
      containing_block_fragments.front().AnchorQuery();
  if (!anchor_query)
    return nullptr;

  bool can_use_invalid_anchors = layout_object->IsInTopOrViewTransitionLayer();

  const NGPhysicalFragment* fragment = nullptr;
  if (value->IsNamed()) {
    fragment =
        anchor_query->Fragment(&value->GetName(), can_use_invalid_anchors);
  } else if (value->IsDefault() && style.AnchorDefault()) {
    fragment =
        anchor_query->Fragment(style.AnchorDefault(), can_use_invalid_anchors);
  } else {
    DCHECK(value->IsImplicit() ||
           (value->IsDefault() && !style.AnchorDefault()));
    Element* element = DynamicTo<Element>(layout_object->GetNode());
    Element* anchor = element ? element->ImplicitAnchorElement() : nullptr;
    LayoutObject* anchor_layout_object =
        anchor ? anchor->GetLayoutObject() : nullptr;
    if (anchor_layout_object) {
      fragment =
          anchor_query->Fragment(anchor_layout_object, can_use_invalid_anchors);
    }
  }

  // |can_use_invalid_anchors| allows NGPhysicalAnchorQuery to return elements
  // that are rendered after, and hence, can't be used as anchors for
  // |layout_object|.
  if (can_use_invalid_anchors && fragment &&
      layout_object->IsBeforeInPreOrder(*fragment->GetLayoutObject())) {
    return nullptr;
  }

  return fragment ? fragment->GetLayoutObject() : nullptr;
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
  if (!non_overflowing_scroll_ranges_.size()) {
    return true;
  }

  for (const PhysicalScrollRange& range : non_overflowing_scroll_ranges_) {
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
