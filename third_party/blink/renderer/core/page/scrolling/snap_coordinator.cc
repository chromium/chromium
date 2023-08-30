// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {
namespace {
// This is experimentally determined and corresponds to the UA decided
// parameter as mentioned in spec.
constexpr float kProximityRatio = 1.0 / 3.0;

cc::SnapAlignment AdjustForRtlWritingMode(cc::SnapAlignment align) {
  if (align == cc::SnapAlignment::kStart)
    return cc::SnapAlignment::kEnd;

  if (align == cc::SnapAlignment::kEnd)
    return cc::SnapAlignment::kStart;

  return align;
}

}  // namespace
// TODO(sunyunjia): Move the static functions to an anonymous namespace.

SnapCoordinator::SnapCoordinator() : snap_containers_() {}

SnapCoordinator::~SnapCoordinator() = default;

void SnapCoordinator::Trace(Visitor* visitor) const {
  visitor->Trace(snap_containers_);
}

// Returns the layout box's next ancestor that can be a snap container.
// The origin may be either a snap area or a snap container.
LayoutBox* FindSnapContainer(const LayoutBox& origin_box) {
  // According to the new spec
  // https://drafts.csswg.org/css-scroll-snap/#snap-model
  // "Snap positions must only affect the nearest ancestor (on the element’s
  // containing block chain) scroll container".
  Element* document_element = origin_box.GetDocument().documentElement();
  LayoutBox* box = origin_box.ContainingBlock();
  while (box && !box->IsScrollContainer() && !IsA<LayoutView>(box) &&
         box->GetNode() != document_element) {
    box = box->ContainingBlock();
  }

  // If we reach to document element then we dispatch to layout view.
  if (box && box->GetNode() == document_element)
    return origin_box.GetDocument().GetLayoutView();

  return box;
}

// Snap types are categorized according to the spec
// https://drafts.csswg.org/css-scroll-snap-1/#snap-axis
cc::ScrollSnapType GetPhysicalSnapType(const LayoutBox& snap_container) {
  cc::ScrollSnapType scroll_snap_type =
      snap_container.Style()->GetScrollSnapType();
  if (scroll_snap_type.axis == cc::SnapAxis::kInline) {
    if (snap_container.Style()->IsHorizontalWritingMode())
      scroll_snap_type.axis = cc::SnapAxis::kX;
    else
      scroll_snap_type.axis = cc::SnapAxis::kY;
  }
  if (scroll_snap_type.axis == cc::SnapAxis::kBlock) {
    if (snap_container.Style()->IsHorizontalWritingMode())
      scroll_snap_type.axis = cc::SnapAxis::kY;
    else
      scroll_snap_type.axis = cc::SnapAxis::kX;
  }
  // Writing mode does not affect the cases where axis is kX, kY or kBoth.
  return scroll_snap_type;
}

// Adding a snap container means that the element's descendant snap areas need
// to be reassigned to it. First we find the snap container ancestor of the new
// snap container, then check its snap areas to see if their closest ancestor is
// changed to the new snap container.
// E.g., if A1 and A2's ancestor, C2, is C1's descendant and becomes scrollable
// then C2 becomes a snap container then the snap area assignments change:
//  Before            After adding C2
//       C1              C1
//       |               |
//   +-------+         +-----+
//   |  |    |         C2    |
//   A1 A2   A3      +---+   A3
//                   |   |
//                   A1  A2
void SnapCoordinator::AddSnapContainer(LayoutBox& snap_container) {
  DCHECK(!RuntimeEnabledFeatures::LayoutNewSnapLogicEnabled());

  snap_containers_.insert(&snap_container);

  LayoutBox* ancestor_snap_container = FindSnapContainer(snap_container);
  // If an ancestor doesn't exist then this means that the element is being
  // attached now; this means that it won't have any descendants that are
  // assigned to an ancestor snap container.
  if (!ancestor_snap_container) {
    DCHECK(!snap_container.Parent());
    return;
  }
  SnapAreaSet* snap_areas = ancestor_snap_container->SnapAreas();
  if (!snap_areas)
    return;
  HeapVector<Member<LayoutBox>> snap_areas_to_reassign;
  for (const auto& snap_area : *snap_areas) {
    if (FindSnapContainer(*snap_area) == &snap_container)
      snap_areas_to_reassign.push_back(snap_area);
  }
  for (const auto& snap_area : snap_areas_to_reassign)
    snap_area->SetSnapContainer(&snap_container);

  // The new snap container will not have attached its ScrollableArea yet, so we
  // cannot invalidate the snap container data at this point. However, the snap
  // container data is set to needing an update by default, so we only need to
  // update the flag for the ancestor.
  if (snap_areas_to_reassign.size()) {
    ancestor_snap_container->GetScrollableArea()
        ->SetSnapContainerDataNeedsUpdate(true);
  }
}

void SnapCoordinator::RemoveSnapContainer(LayoutBox& snap_container) {
  DCHECK(!RuntimeEnabledFeatures::LayoutNewSnapLogicEnabled());

  LayoutBox* ancestor_snap_container = FindSnapContainer(snap_container);

  // We remove the snap container if it is no longer scrollable, or if the
  // element is detached.
  // - If it is no longer scrollable, then we reassign its snap areas to the
  // next ancestor snap container.
  // - If it is detached, then we simply clear its snap areas since they will be
  // detached as well.
  if (ancestor_snap_container) {
    ancestor_snap_container->GetScrollableArea()
        ->SetSnapContainerDataNeedsUpdate(true);
    snap_container.ReassignSnapAreas(*ancestor_snap_container);
  } else {
    DCHECK(!snap_container.Parent());
    snap_container.ClearSnapAreas();
  }
  // We don't need to update the old snap container's data since the
  // corresponding ScrollableArea is being removed, and thus the snap container
  // data is removed too.
  snap_container.SetNeedsPaintPropertyUpdate();
  snap_containers_.erase(&snap_container);
}

void SnapCoordinator::SnapContainerDidChange(LayoutBox& snap_container) {
  DCHECK(!RuntimeEnabledFeatures::LayoutNewSnapLogicEnabled());

  // Scroll snap properties have no effect on the document element instead they
  // are propagated to (See StyleResolver::PropagateStyleToViewport) and handled
  // by the LayoutView.
  if (snap_container.GetNode() ==
      snap_container.GetDocument().documentElement())
    return;

  PaintLayerScrollableArea* scrollable_area =
      snap_container.GetScrollableArea();
  // Per specification snap positions only affect *scroll containers* [1]. So if
  // the layout box is not a scroll container we ignore it here even if it has
  // non-none scroll-snap-type. Note that in blink, existence of scrollable area
  // directly maps to being a scroll container in the specification. [1]
  // https://drafts.csswg.org/css-scroll-snap/#overview
  if (!scrollable_area) {
    DCHECK(!snap_containers_.Contains(&snap_container));
    return;
  }

  // Note that even if scroll snap type is 'none' we continue to maintain its
  // snap container entry as long as the element is a scroller. This is because
  // while the scroller does not snap, it still captures the snap areas in its
  // subtree for whom it is the nearest ancestor scroll container per spec [1].
  //
  // [1] https://drafts.csswg.org/css-scroll-snap/#overview
  scrollable_area->SetSnapContainerDataNeedsUpdate(true);
}

void SnapCoordinator::SnapAreaDidChange(LayoutBox& snap_area,
                                        cc::ScrollSnapAlign scroll_snap_align) {
  DCHECK(!RuntimeEnabledFeatures::LayoutNewSnapLogicEnabled());

  LayoutBox* old_container = snap_area.SnapContainer();
  if (scroll_snap_align.alignment_inline == cc::SnapAlignment::kNone &&
      scroll_snap_align.alignment_block == cc::SnapAlignment::kNone) {
    snap_area.SetSnapContainer(nullptr);
    if (old_container)
      old_container->GetScrollableArea()->SetSnapContainerDataNeedsUpdate(true);
    return;
  }

  // If there is no ancestor snap container then this means that this snap
  // area is being detached. In the worst case, the layout view is the
  // ancestor snap container, which should exist as long as the document is
  // not destroyed.
  if (LayoutBox* new_container = FindSnapContainer(snap_area)) {
    snap_area.SetSnapContainer(new_container);
    // TODO(sunyunjia): consider keep the SnapAreas in a map so it is
    // easier to update.
    new_container->GetScrollableArea()->SetSnapContainerDataNeedsUpdate(true);
    if (old_container && old_container != new_container)
      old_container->GetScrollableArea()->SetSnapContainerDataNeedsUpdate(true);
  }
}

void SnapCoordinator::UpdateAllSnapContainerDataIfNeeded() {
  for (const auto& container : snap_containers_) {
    if (container->GetScrollableArea()->SnapContainerDataNeedsUpdate())
      UpdateSnapContainerData(*container);
  }
  SetAnySnapContainerDataNeedsUpdate(false);
}

// static
void SnapCoordinator::UpdateSnapContainerData(LayoutBox& snap_container) {
  ScrollableArea* scrollable_area =
      ScrollableArea::GetForScrolling(&snap_container);
  const auto* old_snap_container_data = scrollable_area->GetSnapContainerData();
  auto snap_type = GetPhysicalSnapType(snap_container);

  if (!RuntimeEnabledFeatures::LayoutNewSnapLogicEnabled()) {
    scrollable_area->SetSnapContainerDataNeedsUpdate(false);
  }

  // Scrollers that don't have any snap areas assigned to them and don't snap
  // require no further processing. These are the most common types and thus
  // returning as early as possible ensures efficiency.
  if (snap_type.is_none) {
    // Clear the old data if needed.
    if (old_snap_container_data) {
      snap_container.SetNeedsPaintPropertyUpdate();
      scrollable_area->SetSnapContainerData(absl::nullopt);
    }
    return;
  }

  cc::SnapContainerData snap_container_data(snap_type);

  gfx::PointF max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());
  snap_container_data.set_max_position(max_position);

  // Scroll-padding represents inward offsets from the corresponding edge of
  // the scrollport.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding Scrollport is
  // the visual viewport of the scroll container (through which the scrollable
  // overflow region can be viewed) coincides with its padding box.
  // https://drafts.csswg.org/css-overflow-3/#scrollport. So we use the
  // LayoutRect of the padding box here. The coordinate is relative to the
  // container's border box.
  PhysicalRect container_rect(snap_container.PhysicalPaddingBoxRect());

  const ComputedStyle* container_style = snap_container.Style();
  // The percentage of scroll-padding is different from that of normal
  // padding, as scroll-padding resolves the percentage against corresponding
  // dimension of the scrollport[1], while the normal padding resolves that
  // against "width".[2,3] We use MinimumValueForLength here to ensure kAuto
  // is resolved to LayoutUnit() which is the correct behavior for padding.
  //
  // [1] https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding
  //     "relative to the corresponding dimension of the scroll container’s
  //      scrollport"
  // [2] https://drafts.csswg.org/css-box/#padding-props
  // [3] See for example LayoutBoxModelObject::ComputedCSSPadding where it
  //     uses |MinimumValueForLength| but against the "width".
  container_rect.ContractEdges(
      MinimumValueForLength(container_style->ScrollPaddingTop(),
                            container_rect.Height()),
      MinimumValueForLength(container_style->ScrollPaddingRight(),
                            container_rect.Width()),
      MinimumValueForLength(container_style->ScrollPaddingBottom(),
                            container_rect.Height()),
      MinimumValueForLength(container_style->ScrollPaddingLeft(),
                            container_rect.Width()));
  snap_container_data.set_rect(gfx::RectF(container_rect));

  if (snap_container_data.scroll_snap_type().strictness ==
      cc::SnapStrictness::kProximity) {
    PhysicalSize size = container_rect.size;
    size.Scale(kProximityRatio);
    gfx::PointF range(size.width.ToFloat(), size.height.ToFloat());
    snap_container_data.set_proximity_range(range);
  }

  cc::TargetSnapAreaElementIds new_target_ids;
  const cc::TargetSnapAreaElementIds old_target_ids =
      old_snap_container_data
          ? old_snap_container_data->GetTargetSnapAreaElementIds()
          : cc::TargetSnapAreaElementIds();

  const HeapHashSet<Member<LayoutBox>>* snap_areas;
  if (RuntimeEnabledFeatures::LayoutNewSnapLogicEnabled()) {
    for (auto& fragment : snap_container.PhysicalFragments()) {
      snap_areas = fragment.SnapAreas();
      if (snap_areas) {
        break;
      }
    }
  } else {
    snap_areas = snap_container.SnapAreas();
  }

  if (snap_areas) {
    for (const LayoutBox* snap_area : *snap_areas) {
      cc::SnapAreaData snap_area_data =
          CalculateSnapAreaData(*snap_area, snap_container);
      // The target snap elements should be preserved in the new container
      // only if the respective snap areas are still present.
      if (old_target_ids.x == snap_area_data.element_id) {
        new_target_ids.x = old_target_ids.x;
      }
      if (old_target_ids.y == snap_area_data.element_id) {
        new_target_ids.y = old_target_ids.y;
      }

      if (snap_area_data.rect.width() > snap_container_data.rect().width() ||
          snap_area_data.rect.height() > snap_container_data.rect().height()) {
        snap_container.GetDocument().CountUse(
            WebFeature::kScrollSnapCoveringSnapArea);
      }
      snap_container_data.AddSnapAreaData(snap_area_data);
    }
  }
  snap_container_data.SetTargetSnapAreaElementIds(new_target_ids);

  if (!old_snap_container_data ||
      *old_snap_container_data != snap_container_data) {
    snap_container.SetNeedsPaintPropertyUpdate();
    scrollable_area->SetSnapContainerData(snap_container_data);
    scrollable_area->SnapAfterLayout();
  }
}

// https://drafts.csswg.org/css-scroll-snap-1/#scroll-snap-align
// After normalization:
//   * inline corresponds to x, and block corresponds to y
//   * start corresponds to left or top
//   * end corresponds to right or bottom
// In other words, the adjusted logical properties map to a physical layout
// as if the writing mode were horizontal left to right and top to bottom.
static cc::ScrollSnapAlign GetPhysicalAlignment(
    const ComputedStyle& area_style,
    const ComputedStyle& container_style,
    const PhysicalRect& area_rect,
    const PhysicalRect& container_rect) {
  cc::ScrollSnapAlign align = area_style.GetScrollSnapAlign();
  cc::ScrollSnapAlign adjusted_alignment;
  // Start and end alignments are resolved with respect to the writing mode of
  // the snap container unless the scroll snap area is larger than the snapport,
  // in which case they are resolved with respect to the writing mode of the box
  // itself. (This allows items in a container to have consistent snap alignment
  // in general, while ensuring that start always aligns the item to allow
  // reading its contents from the beginning.)
  WritingDirectionMode writing_direction =
      container_style.GetWritingDirection();
  WritingDirectionMode area_writing_direction =
      area_style.GetWritingDirection();
  if (area_writing_direction.IsHorizontal()) {
    if (area_rect.Width() > container_rect.Width())
      writing_direction = area_writing_direction;
  } else {
    if (area_rect.Height() > container_rect.Height())
      writing_direction = area_writing_direction;
  }

  bool rtl = (writing_direction.IsRtl());
  if (writing_direction.IsHorizontal()) {
    adjusted_alignment.alignment_inline =
        rtl ? AdjustForRtlWritingMode(align.alignment_inline)
            : align.alignment_inline;
    adjusted_alignment.alignment_block = align.alignment_block;
  } else {
    bool flipped = writing_direction.IsFlippedBlocks();
    adjusted_alignment.alignment_inline =
        flipped ? AdjustForRtlWritingMode(align.alignment_block)
                : align.alignment_block;
    adjusted_alignment.alignment_block =
        rtl ? AdjustForRtlWritingMode(align.alignment_inline)
            : align.alignment_inline;
  }
  return adjusted_alignment;
}

// static
cc::SnapAreaData SnapCoordinator::CalculateSnapAreaData(
    const LayoutBox& snap_area,
    const LayoutBox& snap_container) {
  const ComputedStyle* container_style = snap_container.Style();
  const ComputedStyle* area_style = snap_area.Style();
  cc::SnapAreaData snap_area_data;

  // We assume that the snap_container is the snap_area's ancestor in layout
  // tree, as the snap_container is found by walking up the layout tree in
  // FindSnapContainer(). Under this assumption,
  // snap_area.LocalToAncestorRect() returns the snap_area's position relative
  // to the snap container's border box, while ignoring scroll offset.
  PhysicalRect area_rect = snap_area.PhysicalBorderBoxRect();
  area_rect = snap_area.LocalToAncestorRect(
      area_rect, &snap_container,
      kTraverseDocumentBoundaries | kIgnoreScrollOffset);

  NGPhysicalBoxStrut area_margin(
      area_style->ScrollMarginTop(), area_style->ScrollMarginRight(),
      area_style->ScrollMarginBottom(), area_style->ScrollMarginLeft());
  area_rect.Expand(area_margin);
  snap_area_data.rect = gfx::RectF(area_rect);

  PhysicalRect container_rect = snap_container.PhysicalBorderBoxRect();

  snap_area_data.scroll_snap_align = GetPhysicalAlignment(
      *area_style, *container_style, area_rect, container_rect);

  snap_area_data.must_snap =
      (area_style->ScrollSnapStop() == EScrollSnapStop::kAlways);

  snap_area_data.element_id = CompositorElementIdFromDOMNodeId(
      DOMNodeIds::IdForNode(snap_area.GetNode()));

  return snap_area_data;
}

#ifndef NDEBUG

void SnapCoordinator::ShowSnapAreaMap() {
  for (const auto& container : snap_containers_)
    ShowSnapAreasFor(container);
}

void SnapCoordinator::ShowSnapAreasFor(const LayoutBox* container) {
  LOG(INFO) << *container->GetNode();
  if (SnapAreaSet* snap_areas = container->SnapAreas()) {
    for (const auto& snap_area : *snap_areas) {
      LOG(INFO) << "    " << *snap_area->GetNode();
    }
  }
}

void SnapCoordinator::ShowSnapDataFor(const LayoutBox* snap_container) {
  if (!snap_container)
    return;
  ScrollableArea* scrollable_area =
      ScrollableArea::GetForScrolling(snap_container);
  const auto* optional_data =
      scrollable_area ? scrollable_area->GetSnapContainerData() : nullptr;
  if (optional_data)
    LOG(INFO) << *optional_data;
}

#endif

}  // namespace blink
