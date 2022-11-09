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
  if (!layout_object->StyleRef().AnchorScroll())
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

  if (const NGPhysicalFragment* fragment =
          anchor_query->Fragment(*layout_object->StyleRef().AnchorScroll())) {
    return fragment->GetLayoutObject();
  }
  return nullptr;
}

// Returns the PaintLayer of the scroll container of an anchor-positioned |box|.
const PaintLayer* ContainingScrollContainerLayerForAnchorScroll(
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

bool AnchorScrollData::IsActive() const {
  return owner_->GetAnchorScrollData() == this;
}

AnchorScrollData::SnapshotDiff AnchorScrollData::TakeAndCompareSnapshot(
    bool update) {
  DCHECK(IsActive());

  HeapVector<Member<const PaintLayer>> new_scroll_container_layers;
  gfx::Vector2dF new_accumulated_scroll_offset;
  gfx::Vector2d new_accumulated_scroll_origin;

  if (const LayoutObject* anchor =
          AnchorScrollObject(owner_->GetLayoutObject())) {
    const PaintLayer* starting_layer =
        anchor->ContainingScrollContainer()->Layer();
    const PaintLayer* bounding_layer =
        ContainingScrollContainerLayerForAnchorScroll(owner_->GetLayoutBox());
    for (const PaintLayer* layer = starting_layer; layer != bounding_layer;
         layer = layer->ContainingScrollContainerLayer()) {
      // |bounding_layer| must be either null (for fixed-positioned |owner_|) or
      // an ancestor of |starting_layer|, so we'll never have a null layer here.
      DCHECK(layer);
      if (!layer->GetScrollableArea()->HasOverflow())
        continue;
      new_scroll_container_layers.push_back(layer);
      new_accumulated_scroll_offset +=
          layer->GetScrollableArea()->GetScrollOffset();
      new_accumulated_scroll_origin +=
          layer->GetScrollableArea()->ScrollOrigin().OffsetFromOrigin();
    }
  }

  SnapshotDiff diff;
  if (scroll_container_layers_ != new_scroll_container_layers) {
    diff = SnapshotDiff::kScrollers;
  } else if (accumulated_scroll_offset_ != new_accumulated_scroll_offset ||
             accumulated_scroll_origin_ != new_accumulated_scroll_origin) {
    // TODO(crbug.com/1309178): An offset-only change may result in a change in
    // a different fallback position, which needs a re-layout and must be
    // distinguished from a "pure" offset-only change that only needs a repaint.
    // Implement that.
    diff = SnapshotDiff::kOffsetOnly;
  } else {
    diff = SnapshotDiff::kNone;
  }

  if (update && diff != SnapshotDiff::kNone) {
    scroll_container_layers_.swap(new_scroll_container_layers);
    accumulated_scroll_offset_ = new_accumulated_scroll_offset;
    accumulated_scroll_origin_ = new_accumulated_scroll_origin;
  }

  return diff;
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
    case SnapshotDiff::kScrollers:
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
    case SnapshotDiff::kScrollers:
      InvalidateLayout();
      return false;
  }
}

bool AnchorScrollData::ShouldScheduleNextService() {
  return IsActive() &&
         TakeAndCompareSnapshot(false /*update*/) == SnapshotDiff::kNone;
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
  visitor->Trace(scroll_container_layers_);
  ScrollSnapshotClient::Trace(visitor);
}

}  // namespace blink
