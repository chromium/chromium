// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {
namespace {
// This is experimentally determined and corresponds to the UA decided
// parameter as mentioned in spec.
// If changing this, consider modifying
// web_tests/fast/scrolling/area-at-exact-proximity-range-doesnt-crash.html
// accordingly.
constexpr float kProximityRatio = 1.0 / 3.0;

cc::SnapAlignment AdjustForRtlWritingMode(cc::SnapAlignment align) {
  if (align == cc::SnapAlignment::kStart)
    return cc::SnapAlignment::kEnd;

  if (align == cc::SnapAlignment::kEnd)
    return cc::SnapAlignment::kStart;

  return align;
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

}  // namespace
// TODO(sunyunjia): Move the static functions to an anonymous namespace.

// static
bool SnapCoordinator::UpdateSnapContainerData(LayoutBox& snap_container) {
  ScrollableArea* scrollable_area =
      ScrollableArea::GetForScrolling(&snap_container);
  const auto* old_snap_container_data = scrollable_area->GetSnapContainerData();
  auto snap_type = GetPhysicalSnapType(snap_container);

  // Scrollers that don't have any snap areas assigned to them and don't snap
  // require no further processing. These are the most common types and thus
  // returning as early as possible ensures efficiency.
  if (snap_type.is_none) {
    // Clear the old data if needed.
    if (old_snap_container_data) {
      snap_container.SetNeedsPaintPropertyUpdate();
      scrollable_area->SetScrollsnapchangingTargetIds(std::nullopt);
      scrollable_area->SetScrollsnapchangeTargetIds(std::nullopt);
      scrollable_area->SetSnappedQueryTargetIds(std::nullopt);
      if (RuntimeEnabledFeatures::CSSScrollSnapChangeEventEnabled()) {
        scrollable_area->EnqueueScrollSnapChangeEvent();
      }
      scrollable_area->SetSnapContainerData(std::nullopt);
    }
    return false;
  }

  cc::SnapContainerData snap_container_data(snap_type);

  gfx::PointF max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());
  snap_container_data.set_max_position(max_position);
  snap_container_data.set_targeted_area_id(
      scrollable_area->GetTargetedSnapAreaId());

  // Scroll-padding represents inward offsets from the corresponding edge of
  // the scrollport.
  // https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding Scrollport is
  // the visual viewport of the scroll container (through which the scrollable
  // overflow region can be viewed) coincides with its padding box.
  // https://drafts.csswg.org/css-overflow-3/#scrollport. So we use the
  // PhysicalRect of the padding box here. The coordinate is relative to the
  // container's border box.
  PhysicalRect container_rect(
      snap_container.OverflowClipRect(PhysicalOffset()));

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
  snap_container_data.set_has_horizontal_writing_mode(
      container_style->IsHorizontalWritingMode());

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

  for (auto& fragment : snap_container.PhysicalFragments()) {
    if (auto* snap_areas = fragment.SnapAreas()) {
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
            snap_area_data.rect.height() >
                snap_container_data.rect().height()) {
          snap_container.GetDocument().CountUse(
              WebFeature::kScrollSnapCoveringSnapArea);
        }
        snap_container_data.AddSnapAreaData(snap_area_data);
      }
    }
  }

  snap_container_data.SetTargetSnapAreaElementIds(new_target_ids);

  if (!old_snap_container_data ||
      *old_snap_container_data != snap_container_data) {
    snap_container.SetNeedsPaintPropertyUpdate();
    scrollable_area->SetSnapContainerData(snap_container_data);
    return true;
  }
  return false;
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

  // Calculate the bounding box of all fragments generated by `snap_area`,
  // relatively to `snap_container`.
  const MapCoordinatesFlags mapping_mode =
      kTraverseDocumentBoundaries | kIgnoreScrollOffset;
  Vector<gfx::QuadF> quads;
  snap_area.QuadsInAncestor(quads, &snap_container, mapping_mode);
  PhysicalRect area_rect;
  for (const gfx::QuadF& quad : quads) {
    area_rect.UniteIfNonZero(PhysicalRect::EnclosingRect(quad.BoundingBox()));
  }

  PhysicalBoxStrut area_margin(
      area_style->ScrollMarginTop(), area_style->ScrollMarginRight(),
      area_style->ScrollMarginBottom(), area_style->ScrollMarginLeft());
  area_rect.Expand(area_margin);
  snap_area_data.rect = gfx::RectF(area_rect);

  PhysicalRect container_rect = snap_container.PhysicalBorderBoxRect();

  snap_area_data.scroll_snap_align = GetPhysicalAlignment(
      *area_style, *container_style, area_rect, container_rect);

  snap_area_data.must_snap =
      (area_style->ScrollSnapStop() == EScrollSnapStop::kAlways);

  snap_area_data.has_focus_within = snap_area.GetNode()->HasFocusWithin();

  snap_area_data.element_id =
      CompositorElementIdFromDOMNodeId(snap_area.GetNode()->GetDomNodeId());

  return snap_area_data;
}

}  // namespace blink
