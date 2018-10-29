// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/snap_coordinator.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/scroll/scroll_snap_data.h"

namespace blink {
namespace {
// This is experimentally determined and corresponds to the UA decided
// parameter as mentioned in spec.
constexpr float kProximityRatio = 1.0 / 3.0;
}  // namespace
// TODO(sunyunjia): Move the static functions to an anonymous namespace.

SnapCoordinator::SnapCoordinator() : snap_container_map_() {}

SnapCoordinator::~SnapCoordinator() = default;

SnapCoordinator* SnapCoordinator::Create() {
  return new SnapCoordinator();
}

// Returns the scroll container that can be affected by this snap area.
static LayoutBox* FindSnapContainer(const LayoutBox& snap_area) {
  // According to the new spec
  // https://drafts.csswg.org/css-scroll-snap/#snap-model
  // "Snap positions must only affect the nearest ancestor (on the element’s
  // containing block chain) scroll container".
  Element* viewport_defining_element =
      snap_area.GetDocument().ViewportDefiningElement();
  LayoutBox* box = snap_area.ContainingBlock();
  while (box && !box->HasOverflowClip() && !box->IsLayoutView() &&
         box->GetNode() != viewport_defining_element)
    box = box->ContainingBlock();

  // If we reach to viewportDefiningElement then we dispatch to viewport
  if (box && box->GetNode() == viewport_defining_element)
    return snap_area.GetDocument().GetLayoutView();

  return box;
}

void SnapCoordinator::SnapAreaDidChange(LayoutBox& snap_area,
                                        ScrollSnapAlign scroll_snap_align) {
  LayoutBox* old_container = snap_area.SnapContainer();
  if (scroll_snap_align.alignment_inline == SnapAlignment::kNone &&
      scroll_snap_align.alignment_block == SnapAlignment::kNone) {
    snap_area.SetSnapContainer(nullptr);
    if (old_container)
      UpdateSnapContainerData(*old_container);
    return;
  }

  if (LayoutBox* new_container = FindSnapContainer(snap_area)) {
    snap_area.SetSnapContainer(new_container);
    // TODO(sunyunjia): consider keep the SnapAreas in a map so it is
    // easier to update.
    // TODO(sunyunjia): Only update when the localframe doesn't need layout.
    UpdateSnapContainerData(*new_container);
    if (old_container && old_container != new_container)
      UpdateSnapContainerData(*old_container);
  } else {
    // TODO(majidvp): keep track of snap areas that do not have any
    // container so that we check them again when a new container is
    // added to the page.
  }
}

void SnapCoordinator::UpdateAllSnapContainerData() {
  for (const auto& entry : snap_container_map_) {
    UpdateSnapContainerData(*entry.key);
  }
}

static ScrollableArea* ScrollableAreaForSnapping(const LayoutBox& layout_box) {
  return layout_box.IsLayoutView()
             ? layout_box.GetFrameView()->GetScrollableArea()
             : layout_box.GetScrollableArea();
}

static ScrollSnapType GetPhysicalSnapType(const LayoutBox& snap_container) {
  ScrollSnapType scroll_snap_type = snap_container.Style()->GetScrollSnapType();
  if (scroll_snap_type.axis == SnapAxis::kInline) {
    if (snap_container.Style()->IsHorizontalWritingMode())
      scroll_snap_type.axis = SnapAxis::kX;
    else
      scroll_snap_type.axis = SnapAxis::kY;
  }
  if (scroll_snap_type.axis == SnapAxis::kBlock) {
    if (snap_container.Style()->IsHorizontalWritingMode())
      scroll_snap_type.axis = SnapAxis::kY;
    else
      scroll_snap_type.axis = SnapAxis::kX;
  }
  // Writing mode does not affect the cases where axis kX, kY or kBoth.
  return scroll_snap_type;
}

void SnapCoordinator::UpdateSnapContainerData(const LayoutBox& snap_container) {
  if (snap_container.Style()->GetScrollSnapType().is_none)
    return;

  SnapContainerData snap_container_data(GetPhysicalSnapType(snap_container));

  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(snap_container);
  if (!scrollable_area)
    return;
  FloatPoint max_position = scrollable_area->ScrollOffsetToPosition(
      scrollable_area->MaximumScrollOffset());
  snap_container_data.set_max_position(
      gfx::ScrollOffset(max_position.X(), max_position.Y()));

  // Scroll-padding represents inward offsets from the corresponding edge of the
  // scrollport. https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding
  // Scrollport is the visual viewport of the scroll container (through which
  // the scrollable overflow region can be viewed) coincides with its padding
  // box. https://drafts.csswg.org/css-overflow-3/#scrollport. So we use the
  // LayoutRect of the padding box here. The coordinate is relative to the
  // container's border box.
  LayoutRect container_rect(snap_container.PhysicalPaddingBoxRect());

  const ComputedStyle* container_style = snap_container.Style();
  LayoutRectOutsets container_padding(
      // The percentage of scroll-padding is different from that of normal
      // padding, as scroll-padding resolves the percentage against
      // corresponding dimension of the scrollport[1], while the normal padding
      // resolves that against "width".[2,3]
      // We use MinimumValueForLength here to ensure kAuto is resolved to
      // LayoutUnit() which is the correct behavior for padding.
      // [1] https://drafts.csswg.org/css-scroll-snap-1/#scroll-padding
      //     "relative to the corresponding dimension of the scroll container’s
      //      scrollport"
      // [2] https://drafts.csswg.org/css-box/#padding-props
      // [3] See for example LayoutBoxModelObject::ComputedCSSPadding where it
      //     uses |MinimumValueForLength| but against the "width".
      MinimumValueForLength(container_style->ScrollPaddingTop(),
                            container_rect.Height()),
      MinimumValueForLength(container_style->ScrollPaddingRight(),
                            container_rect.Width()),
      MinimumValueForLength(container_style->ScrollPaddingBottom(),
                            container_rect.Height()),
      MinimumValueForLength(container_style->ScrollPaddingLeft(),
                            container_rect.Width()));
  container_rect.Contract(container_padding);
  snap_container_data.set_rect(FloatRect(container_rect));

  if (snap_container_data.scroll_snap_type().strictness ==
      SnapStrictness::kProximity) {
    LayoutSize size = container_rect.Size();
    size.Scale(kProximityRatio);
    gfx::ScrollOffset range(size.Width().ToFloat(), size.Height().ToFloat());
    snap_container_data.set_proximity_range(range);
  }

  if (SnapAreaSet* snap_areas = snap_container.SnapAreas()) {
    for (const LayoutBox* snap_area : *snap_areas) {
      snap_container_data.AddSnapAreaData(
          CalculateSnapAreaData(*snap_area, snap_container));
    }
  }
  snap_container_map_.Set(&snap_container, snap_container_data);
}

static ScrollSnapAlign GetPhysicalAlignment(
    const ComputedStyle& area_style,
    const ComputedStyle& container_style) {
  ScrollSnapAlign align = area_style.GetScrollSnapAlign();
  if (container_style.IsHorizontalWritingMode())
    return align;

  SnapAlignment tmp = align.alignment_inline;
  align.alignment_inline = align.alignment_block;
  align.alignment_block = tmp;

  if (container_style.IsFlippedBlocksWritingMode()) {
    if (align.alignment_inline == SnapAlignment::kStart) {
      align.alignment_inline = SnapAlignment::kEnd;
    } else if (align.alignment_inline == SnapAlignment::kEnd) {
      align.alignment_inline = SnapAlignment::kStart;
    }
  }
  return align;
}

SnapAreaData SnapCoordinator::CalculateSnapAreaData(
    const LayoutBox& snap_area,
    const LayoutBox& snap_container) {
  const ComputedStyle* container_style = snap_container.Style();
  const ComputedStyle* area_style = snap_area.Style();
  SnapAreaData snap_area_data;

  // We assume that the snap_container is the snap_area's ancestor in layout
  // tree, as the snap_container is found by walking up the layout tree in
  // FindSnapContainer(). Under this assumption,
  // snap_area.LocalToAncestorQuad() returns the snap_area's position relative
  // to its container's border box. And the |area| below represents the
  // snap_area rect in respect to the snap_container.
  LayoutRect area_rect(LayoutPoint(), LayoutSize(snap_area.OffsetWidth(),
                                                 snap_area.OffsetHeight()));
  area_rect = EnclosingLayoutRect(
      snap_area
          .LocalToAncestorQuad(FloatRect(area_rect), &snap_container,
                               kUseTransforms | kTraverseDocumentBoundaries)
          .BoundingBox());
  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(snap_container);
  if (scrollable_area) {
    if (snap_container.IsLayoutView())
      area_rect = snap_container.GetFrameView()->FrameToDocument(area_rect);
    else
      area_rect.MoveBy(LayoutPoint(scrollable_area->ScrollPosition()));
  }

  LayoutRectOutsets area_margin(
      area_style->ScrollMarginTop(), area_style->ScrollMarginRight(),
      area_style->ScrollMarginBottom(), area_style->ScrollMarginLeft());
  area_rect.Expand(area_margin);
  snap_area_data.rect = FloatRect(area_rect);

  ScrollSnapAlign align = GetPhysicalAlignment(*area_style, *container_style);
  snap_area_data.scroll_snap_align = align;

  snap_area_data.must_snap =
      (area_style->ScrollSnapStop() == EScrollSnapStop::kAlways);

  return snap_area_data;
}

base::Optional<FloatPoint> SnapCoordinator::GetSnapPosition(
    const LayoutBox& snap_container,
    const SnapSelectionStrategy& strategy) const {
  auto iter = snap_container_map_.find(&snap_container);
  if (iter == snap_container_map_.end())
    return base::nullopt;

  const SnapContainerData& data = iter->value;
  if (!data.size())
    return base::nullopt;

  gfx::ScrollOffset snap_position;
  if (data.FindSnapPosition(strategy, &snap_position)) {
    FloatPoint snap_point(snap_position.x(), snap_position.y());
    return snap_point;
  }

  return base::nullopt;
}

bool SnapCoordinator::SnapForEndPosition(const LayoutBox& snap_container,
                                         bool scrolled_x,
                                         bool scrolled_y) const {
  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(snap_container);
  if (!scrollable_area)
    return false;
  FloatPoint current_position = scrollable_area->ScrollPosition();
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForEndPosition(
          gfx::ScrollOffset(current_position), scrolled_x, scrolled_y);
  return PerformSnapping(snap_container, *strategy);
}

bool SnapCoordinator::SnapForDirection(const LayoutBox& snap_container,
                                       const ScrollOffset& delta) const {
  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(snap_container);
  if (!scrollable_area)
    return false;
  FloatPoint current_position = scrollable_area->ScrollPosition();
  std::unique_ptr<SnapSelectionStrategy> strategy =
      SnapSelectionStrategy::CreateForDirection(
          gfx::ScrollOffset(current_position),
          gfx::ScrollOffset(delta.Width(), delta.Height()));
  return PerformSnapping(snap_container, *strategy);
}

bool SnapCoordinator::PerformSnapping(
    const LayoutBox& snap_container,
    const SnapSelectionStrategy& strategy) const {
  ScrollableArea* scrollable_area = ScrollableAreaForSnapping(snap_container);
  if (!scrollable_area)
    return false;

  base::Optional<FloatPoint> snap_point =
      GetSnapPosition(snap_container, strategy);
  if (!snap_point.has_value())
    return false;

  scrollable_area->CancelScrollAnimation();
  scrollable_area->CancelProgrammaticScrollAnimation();
  if (gfx::ScrollOffset(snap_point.value()) != strategy.current_position()) {
    scrollable_area->SetScrollOffset(
        scrollable_area->ScrollPositionToOffset(snap_point.value()),
        kProgrammaticScroll, kScrollBehaviorSmooth);
  }
  return true;
}

void SnapCoordinator::SnapContainerDidChange(LayoutBox& snap_container,
                                             ScrollSnapType scroll_snap_type) {
  snap_container.SetNeedsPaintPropertyUpdate();
  if (scroll_snap_type.is_none) {
    snap_container_map_.erase(&snap_container);
    snap_container.ClearSnapAreas();
    return;
  }

  // TODO(sunyunjia): Only update when the localframe doesn't need layout.
  UpdateSnapContainerData(snap_container);

  // TODO(majidvp): Add logic to correctly handle orphaned snap areas here.
  // 1. Removing container: find a new snap container for its orphan snap
  // areas (most likely nearest ancestor of current container) otherwise add
  // them to an orphan list.
  // 2. Adding container: may take over snap areas from nearest ancestor snap
  // container or from existing areas in orphan pool.
}

base::Optional<SnapContainerData> SnapCoordinator::GetSnapContainerData(
    const LayoutBox& snap_container) const {
  auto iter = snap_container_map_.find(&snap_container);
  if (iter != snap_container_map_.end()) {
    return iter->value;
  }
  return base::nullopt;
}

#ifndef NDEBUG

void SnapCoordinator::ShowSnapAreaMap() {
  for (auto* const container : snap_container_map_.Keys())
    ShowSnapAreasFor(container);
}

void SnapCoordinator::ShowSnapAreasFor(const LayoutBox* container) {
  LOG(INFO) << *container->GetNode();
  if (SnapAreaSet* snap_areas = container->SnapAreas()) {
    for (auto* const snap_area : *snap_areas) {
      LOG(INFO) << "    " << *snap_area->GetNode();
    }
  }
}

void SnapCoordinator::ShowSnapDataFor(const LayoutBox* snap_container) {
  auto iter = snap_container_map_.find(snap_container);
  if (iter == snap_container_map_.end())
    return;
  LOG(INFO) << iter->value;
}

#endif

}  // namespace blink
