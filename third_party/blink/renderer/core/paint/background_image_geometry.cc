// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/background_image_geometry.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

namespace {

// Return the amount of space to leave between image tiles for the
// background-repeat: space property.
inline LayoutUnit GetSpaceBetweenImageTiles(LayoutUnit area_size,
                                            LayoutUnit tile_size) {
  int number_of_tiles = (area_size / tile_size).ToInt();
  LayoutUnit space(-1);
  if (number_of_tiles > 1) {
    // Spec doesn't specify rounding, so use the same method as for
    // background-repeat: round.
    space = (area_size - number_of_tiles * tile_size) / (number_of_tiles - 1);
  }

  return space;
}

bool FixedBackgroundPaintsInLocalCoordinates(
    const LayoutObject& obj,
    const GlobalPaintFlags global_paint_flags) {
  if (!obj.IsLayoutView())
    return false;

  const LayoutView& view = ToLayoutView(obj);

  // TODO(wangxianzhu): For CAP, inline this function into
  // FixedBackgroundPaintsInLocalCoordinates().
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    return view.GetBackgroundPaintLocation() !=
           kBackgroundPaintInScrollingContents;
  }

  if (global_paint_flags & kGlobalPaintFlattenCompositingLayers)
    return false;

  PaintLayer* root_layer = view.Layer();
  if (!root_layer || root_layer->GetCompositingState() == kNotComposited)
    return false;

  CompositedLayerMapping* mapping = root_layer->GetCompositedLayerMapping();
  return !mapping->BackgroundPaintsOntoScrollingContentsLayer();
}

LayoutPoint AccumulatedScrollOffsetForFixedBackground(
    const LayoutBoxModelObject& object,
    const LayoutBoxModelObject* container) {
  LayoutPoint result;
  if (&object == container)
    return result;

  LayoutObject::AncestorSkipInfo skip_info(container);
  for (const LayoutBlock* block = object.ContainingBlock(&skip_info);
       block && !skip_info.AncestorSkipped();
       block = block->ContainingBlock(&skip_info)) {
    if (block->HasOverflowClip())
      result += block->ScrolledContentOffset();
    if (block == container)
      break;
  }
  return result;
}

}  // anonymous namespace

bool NeedsFullSizeDestination(const FillLayer& fill_layer) {
  // When dealing with a mask, the dest rect needs to maintain the full size
  // and the mask should be expanded to fill it out. This allows the mask to
  // correctly mask the entire area it is meant to. This is unnecessary on the
  // last layer, so the normal background path is taken for efficiency when
  // creating the paint shader later on.
  return fill_layer.GetType() == EFillLayerType::kMask && fill_layer.Next() &&
         fill_layer.Composite() != kCompositeSourceOver;
}

void BackgroundImageGeometry::SetNoRepeatX(const FillLayer& fill_layer,
                                           LayoutUnit x_offset,
                                           LayoutUnit snapped_x_offset) {
  if (NeedsFullSizeDestination(fill_layer)) {
    SetPhaseX(-x_offset.ToFloat());
    SetSpaceSize(
        LayoutSize(unsnapped_dest_rect_.Width(), SpaceSize().Height()));
    return;
  }

  // The snapped offset may not yet be snapped, so make sure it is an integer.
  snapped_x_offset = LayoutUnit(RoundToInt(snapped_x_offset));

  if (x_offset > 0) {
    DCHECK(snapped_x_offset >= LayoutUnit());
    // Move the dest rect if the offset is positive. The image "stays" where
    // it is over the dest rect, so this effectively modifies the phase.
    unsnapped_dest_rect_.Move(x_offset, LayoutUnit());
    snapped_dest_rect_.Move(snapped_x_offset, LayoutUnit());

    // Make the dest as wide as a tile, which will reduce the dest
    // rect if the tile is too small to fill the paint_rect. If not,
    // the dest rect will be clipped when intersected with the paint
    // rect.
    unsnapped_dest_rect_.SetWidth(tile_size_.Width());
    snapped_dest_rect_.SetWidth(tile_size_.Width());

    SetPhaseX(0);
  } else {
    // Otherwise, if the offset is negative use it to move the image under
    // the dest rect (since we can't paint outside the paint_rect).
    SetPhaseX(-x_offset.ToFloat());

    // Reduce the width of the dest rect to draw only the portion of the
    // tile that remains visible after offsetting the image.
    // TODO(schenney): This might grow the dest rect if the dest rect has
    // been adjusted for opaque borders.
    unsnapped_dest_rect_.SetWidth(tile_size_.Width() + x_offset);
    snapped_dest_rect_.SetWidth(tile_size_.Width() + snapped_x_offset);
  }

  // Force the horizontal space to zero, retaining vertical.
  SetSpaceSize(LayoutSize(LayoutUnit(), SpaceSize().Height()));
}

void BackgroundImageGeometry::SetNoRepeatY(const FillLayer& fill_layer,
                                           LayoutUnit y_offset,
                                           LayoutUnit snapped_y_offset) {
  if (NeedsFullSizeDestination(fill_layer)) {
    SetPhaseY(-y_offset.ToFloat());
    SetSpaceSize(
        LayoutSize(SpaceSize().Width(), unsnapped_dest_rect_.Height()));
    return;
  }

  // The snapped offset may not yet be snapped, so make sure it is an integer.
  snapped_y_offset = LayoutUnit(RoundToInt(snapped_y_offset));

  if (y_offset > 0) {
    DCHECK(snapped_y_offset >= LayoutUnit());
    // Move the dest rect if the offset is positive. The image "stays" where
    // it is in the paint rect, so this effectively modifies the phase.
    unsnapped_dest_rect_.Move(LayoutUnit(), y_offset);
    snapped_dest_rect_.Move(LayoutUnit(), snapped_y_offset);

    // Make the dest as wide as a tile, which will reduce the dest
    // rect if the tile is too small to fill the paint_rect. If not,
    // the dest rect will be clipped when intersected with the paint
    // rect.
    unsnapped_dest_rect_.SetHeight(tile_size_.Height());
    snapped_dest_rect_.SetHeight(tile_size_.Height());

    SetPhaseY(0);
  } else {
    // Otherwise, if the offset is negative, use it to move the image under
    // the dest rect (since we can't paint outside the paint_rect).
    SetPhaseY(-y_offset.ToFloat());

    // Reduce the height of the dest rect to draw only the portion of the
    // tile that remains visible after offsetting the image.
    // TODO(schenney): This might grow the dest rect if the dest rect has
    // been adjusted for opaque borders.
    unsnapped_dest_rect_.SetHeight(tile_size_.Height() + y_offset);
    snapped_dest_rect_.SetHeight(tile_size_.Height() + snapped_y_offset);
  }

  // Force the vertical space to zero, retaining horizontal.
  SetSpaceSize(LayoutSize(SpaceSize().Width(), LayoutUnit()));
}

void BackgroundImageGeometry::SetRepeatX(const FillLayer& fill_layer,
                                         LayoutUnit available_width,
                                         LayoutUnit extra_offset) {
  // All values are unsnapped to accurately set phase in the presence of
  // zoom and large values. That is, accurately render the
  // background-position value.
  if (tile_size_.Width()) {
    // Recompute computed_position because here we need to resolve against
    // unsnapped widths to correctly set the phase.
    LayoutUnit computed_position =
        MinimumValueForLength(fill_layer.PositionX(), available_width) -
        offset_in_background_.X();

    // Identify the number of tiles that fit within the computed
    // position in the direction we should be moving.
    float number_of_tiles_in_position;
    if (fill_layer.BackgroundXOrigin() == BackgroundEdgeOrigin::kRight) {
      number_of_tiles_in_position =
          (available_width - computed_position + extra_offset).ToFloat() /
          tile_size_.Width().ToFloat();
    } else {
      number_of_tiles_in_position =
          (computed_position + extra_offset).ToFloat() /
          tile_size_.Width().ToFloat();
    }
    // Assuming a non-integral number of tiles, find out how much of the
    // partial tile is visible. That is the phase.
    float fractional_position_within_tile =
        1.0f -
        (number_of_tiles_in_position - truncf(number_of_tiles_in_position));
    SetPhaseX(fractional_position_within_tile * tile_size_.Width());
  } else {
    SetPhaseX(0);
  }
  SetSpaceSize(LayoutSize(LayoutUnit(), SpaceSize().Height()));
}

void BackgroundImageGeometry::SetRepeatY(const FillLayer& fill_layer,
                                         LayoutUnit available_height,
                                         LayoutUnit extra_offset) {
  // All values are unsnapped to accurately set phase in the presence of
  // zoom and large values. That is, accurately render the
  // background-position value.
  if (tile_size_.Height()) {
    // Recompute computed_position because here we need to resolve against
    // unsnapped widths to correctly set the phase.
    LayoutUnit computed_position =
        MinimumValueForLength(fill_layer.PositionY(), available_height) -
        offset_in_background_.Y();

    // Identify the number of tiles that fit within the computed
    // position in the direction we should be moving.
    float number_of_tiles_in_position;
    if (fill_layer.BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom) {
      number_of_tiles_in_position =
          (available_height - computed_position + extra_offset).ToFloat() /
          tile_size_.Height().ToFloat();
    } else {
      number_of_tiles_in_position =
          (computed_position + extra_offset).ToFloat() /
          tile_size_.Height().ToFloat();
    }
    // Assuming a non-integral number of tiles, find out how much of the
    // partial tile is visible. That is the phase.
    float fractional_position_within_tile =
        1.0f -
        (number_of_tiles_in_position - truncf(number_of_tiles_in_position));
    SetPhaseY(fractional_position_within_tile * tile_size_.Height());
  } else {
    SetPhaseY(0);
  }
  SetSpaceSize(LayoutSize(SpaceSize().Width(), LayoutUnit()));
}

void BackgroundImageGeometry::SetSpaceX(LayoutUnit space,
                                        LayoutUnit extra_offset) {
  SetSpaceSize(LayoutSize(space, SpaceSize().Height()));
  // Modify the phase to start a full tile at the edge of the paint area
  LayoutUnit actual_width = tile_size_.Width() + space;
  SetPhaseX(actual_width ? actual_width - fmodf(extra_offset, actual_width)
                         : 0);
}

void BackgroundImageGeometry::SetSpaceY(LayoutUnit space,
                                        LayoutUnit extra_offset) {
  SetSpaceSize(LayoutSize(SpaceSize().Width(), space));
  // Modify the phase to start a full tile at the edge of the paint area
  LayoutUnit actual_height = tile_size_.Height() + space;
  SetPhaseY(actual_height ? actual_height - fmodf(extra_offset, actual_height)
                          : 0);
}

void BackgroundImageGeometry::UseFixedAttachment(
    const LayoutPoint& attachment_point) {
  LayoutPoint aligned_point = attachment_point;
  phase_.Move(
      std::max((aligned_point.X() - unsnapped_dest_rect_.X()).ToFloat(), 0.f),
      std::max((aligned_point.Y() - unsnapped_dest_rect_.Y()).ToFloat(), 0.f));
}

enum ColumnGroupDirection { kColumnGroupStart, kColumnGroupEnd };

static void ExpandToTableColumnGroup(const LayoutTableCell& cell,
                                     const LayoutTableCol& column_group,
                                     LayoutUnit& value,
                                     ColumnGroupDirection column_direction) {
  auto sibling_cell = column_direction == kColumnGroupStart
                          ? &LayoutTableCell::PreviousCell
                          : &LayoutTableCell::NextCell;
  for (const auto* sibling = (cell.*sibling_cell)(); sibling;
       sibling = (sibling->*sibling_cell)()) {
    LayoutTableCol* innermost_col =
        cell.Table()
            ->ColElementAtAbsoluteColumn(sibling->AbsoluteColumnIndex())
            .InnermostColOrColGroup();
    if (!innermost_col || innermost_col->EnclosingColumnGroup() != column_group)
      break;
    value += sibling->Size().Width();
  }
}

LayoutPoint BackgroundImageGeometry::GetOffsetForCell(
    const LayoutTableCell& cell,
    const LayoutBox& positioning_box) {
  LayoutSize border_spacing = LayoutSize(cell.Table()->HBorderSpacing(),
                                         cell.Table()->VBorderSpacing());
  if (positioning_box.IsTableSection())
    return cell.Location() - border_spacing;
  if (positioning_box.IsTableRow()) {
    return LayoutPoint(cell.Location().X(), LayoutUnit()) -
           LayoutSize(border_spacing.Width(), LayoutUnit());
  }

  PhysicalRect sections_rect(PhysicalOffset(), cell.Table()->Size());
  cell.Table()->SubtractCaptionRect(sections_rect);
  LayoutUnit height_of_captions =
      cell.Table()->Size().Height() - sections_rect.Height();
  LayoutPoint offset_in_background = LayoutPoint(
      LayoutUnit(), (cell.Section()->Location().Y() -
                     cell.Table()->BorderBefore() - height_of_captions) +
                        cell.Location().Y());

  DCHECK(positioning_box.IsLayoutTableCol());
  if (ToLayoutTableCol(positioning_box).IsTableColumn()) {
    return offset_in_background -
           LayoutSize(LayoutUnit(), border_spacing.Height());
  }

  DCHECK(ToLayoutTableCol(positioning_box).IsTableColumnGroup());
  LayoutUnit offset = offset_in_background.X();
  ExpandToTableColumnGroup(cell, ToLayoutTableCol(positioning_box), offset,
                           kColumnGroupStart);
  offset_in_background.Move(offset, LayoutUnit());
  return offset_in_background -
         LayoutSize(LayoutUnit(), border_spacing.Height());
}

LayoutSize BackgroundImageGeometry::GetBackgroundObjectDimensions(
    const LayoutTableCell& cell,
    const LayoutBox& positioning_box) {
  LayoutSize border_spacing = LayoutSize(cell.Table()->HBorderSpacing(),
                                         cell.Table()->VBorderSpacing());
  if (positioning_box.IsTableSection())
    return positioning_box.Size() - border_spacing - border_spacing;

  if (positioning_box.IsTableRow()) {
    return positioning_box.Size() -
           LayoutSize(border_spacing.Width(), LayoutUnit()) -
           LayoutSize(border_spacing.Width(), LayoutUnit());
  }

  DCHECK(positioning_box.IsLayoutTableCol());
  PhysicalRect sections_rect(PhysicalOffset(), cell.Table()->Size());
  cell.Table()->SubtractCaptionRect(sections_rect);
  LayoutUnit column_height = sections_rect.Height() -
                             cell.Table()->BorderBefore() -
                             border_spacing.Height() - border_spacing.Height();
  if (ToLayoutTableCol(positioning_box).IsTableColumn())
    return LayoutSize(cell.Size().Width(), column_height);

  DCHECK(ToLayoutTableCol(positioning_box).IsTableColumnGroup());
  LayoutUnit width = cell.Size().Width();
  ExpandToTableColumnGroup(cell, ToLayoutTableCol(positioning_box), width,
                           kColumnGroupStart);
  ExpandToTableColumnGroup(cell, ToLayoutTableCol(positioning_box), width,
                           kColumnGroupEnd);

  return LayoutSize(width, column_height);
}

bool BackgroundImageGeometry::ShouldUseFixedAttachment(
    const FillLayer& fill_layer) {
  // Solid color background should use default attachment.
  return fill_layer.GetImage() &&
         fill_layer.Attachment() == EFillAttachment::kFixed;
}

namespace {

LayoutRect FixedAttachmentPositioningArea(const LayoutBoxModelObject& obj,
                                          const LayoutBoxModelObject* container,
                                          const GlobalPaintFlags flags) {
  // TODO(crbug.com/966142): We should consider ancestor with transform as the
  // fixed background container, instead of always the viewport.
  LocalFrameView* frame_view = obj.View()->GetFrameView();
  if (!frame_view)
    return LayoutRect();

  ScrollableArea* layout_viewport = frame_view->LayoutViewport();
  DCHECK(layout_viewport);

  LayoutRect rect = LayoutRect(
      LayoutPoint(), LayoutSize(layout_viewport->VisibleContentRect().Size()));

  if (FixedBackgroundPaintsInLocalCoordinates(obj, flags))
    return rect;

  // The LayoutView is the only object that can paint a fixed background into
  // its scrolling contents layer, so it gets a special adjustment here.
  if (obj.IsLayoutView()) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      DCHECK_EQ(obj.GetBackgroundPaintLocation(),
                kBackgroundPaintInScrollingContents);
      rect.SetLocation(LayoutPoint(ToLayoutView(obj).ScrolledContentOffset()));
    } else if (auto* mapping = obj.Layer()->GetCompositedLayerMapping()) {
      if (mapping->BackgroundPaintsOntoScrollingContentsLayer()) {
        rect.SetLocation(
            LayoutPoint(ToLayoutView(obj).ScrolledContentOffset()));
      }
    }
  }

  rect.MoveBy(AccumulatedScrollOffsetForFixedBackground(obj, container));

  if (container) {
    rect.MoveBy(
        -container->LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms)
             .ToLayoutPoint());
  }

  // By now we have converted the viewport rect to the border box space of
  // |container|, however |container| does not necessarily create a paint
  // offset translation node, thus its paint offset must be added to convert
  // the rect to the space of the transform node.
  // TODO(trchen): This function does only one simple thing -- mapping the
  // viewport rect from frame space to whatever space the current paint
  // context uses. However we can't always invoke geometry mapper because
  // there are at least one caller uses this before PrePaint phase.
  if (container) {
    DCHECK_GE(container->GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kPrePaintClean);
    rect.MoveBy(container->FirstFragment().PaintOffset().ToLayoutPoint());
  }

  return rect;
}

}  // Anonymous namespace

BackgroundImageGeometry::BackgroundImageGeometry(const LayoutView& view)
    : box_(view),
      positioning_box_(view.RootBox()),
      has_non_local_geometry_(false),
      painting_view_(true),
      painting_table_cell_(false),
      cell_using_container_background_(false) {
  // The background of the box generated by the root element covers the
  // entire canvas and will be painted by the view object, but the we should
  // still use the root element box for positioning.
  positioning_size_override_ = view.RootBox().Size();
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutBoxModelObject& obj)
    : box_(obj),
      positioning_box_(obj),
      has_non_local_geometry_(false),
      painting_view_(false),
      painting_table_cell_(false),
      cell_using_container_background_(false) {
  // Specialized constructor should be used for LayoutView.
  DCHECK(!obj.IsLayoutView());
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutTableCell& cell,
    const LayoutObject* background_object)
    : box_(cell),
      positioning_box_(background_object && !background_object->IsTableCell()
                           ? ToLayoutBoxModelObject(*background_object)
                           : cell),
      has_non_local_geometry_(false),
      painting_view_(false),
      painting_table_cell_(true) {
  cell_using_container_background_ =
      background_object && !background_object->IsTableCell();
  if (cell_using_container_background_) {
    offset_in_background_ =
        GetOffsetForCell(cell, ToLayoutBox(*background_object));
    positioning_size_override_ =
        GetBackgroundObjectDimensions(cell, ToLayoutBox(*background_object));
  }
}

void BackgroundImageGeometry::ComputeDestRectAdjustments(
    const FillLayer& fill_layer,
    const LayoutRect& unsnapped_positioning_area,
    bool disallow_border_derived_adjustment,
    LayoutRectOutsets& unsnapped_dest_adjust,
    LayoutRectOutsets& snapped_dest_adjust) const {
  switch (fill_layer.Clip()) {
    case EFillBox::kContent:
      // If the PaddingOutsets are zero then this is equivalent to
      // kPadding and we should apply the snapping logic.
      unsnapped_dest_adjust = positioning_box_.PaddingOutsets();
      if (!unsnapped_dest_adjust.IsZero()) {
        unsnapped_dest_adjust += positioning_box_.BorderBoxOutsets();

        // We're not trying to match a border position, so don't snap.
        snapped_dest_adjust = unsnapped_dest_adjust;
        return;
      }
      FALLTHROUGH;
    case EFillBox::kPadding:
      unsnapped_dest_adjust = positioning_box_.BorderBoxOutsets();
      if (disallow_border_derived_adjustment) {
        // Nothing to drive snapping behavior, so don't snap.
        snapped_dest_adjust = unsnapped_dest_adjust;
      } else {
        // Force the snapped dest rect to match the inner border to
        // avoid gaps between the background and border.
        // TODO(schenney) The LayoutUnit(float) constructor always
        // rounds down. We should FromFloatFloor or FromFloatCeil to
        // move toward the border.
        FloatRect inner_border_rect =
            positioning_box_.StyleRef()
                .GetRoundedInnerBorderFor(unsnapped_positioning_area)
                .Rect();
        snapped_dest_adjust.SetLeft(LayoutUnit(inner_border_rect.X()) -
                                    unsnapped_dest_rect_.X());
        snapped_dest_adjust.SetTop(LayoutUnit(inner_border_rect.Y()) -
                                   unsnapped_dest_rect_.Y());
        snapped_dest_adjust.SetRight(unsnapped_dest_rect_.MaxX() -
                                     LayoutUnit(inner_border_rect.MaxX()));
        snapped_dest_adjust.SetBottom(unsnapped_dest_rect_.MaxY() -
                                      LayoutUnit(inner_border_rect.MaxY()));
      }
      return;
    case EFillBox::kBorder: {
      if (disallow_border_derived_adjustment) {
        // All adjustments remain 0.
        return;
      }

      // The dest rects can be adjusted. The snapped dest rect is forced
      // to match the inner border to avoid gaps between the background and
      // border, while the unsnapped dest moves according to the
      // border box outsets. This leaves the unsnapped dest accurately
      // conveying the content creator's intent when used for determining
      // the pixels to use from sprite maps and other size and positioning
      // properties.
      // Note that the snapped adjustments do not have the same effect as
      // pixel snapping the unsnapped rectangle. Border snapping snaps both
      // the size and position of the borders, sometimes adjusting the inner
      // border by more than a pixel when done (particularly under magnifying
      // zoom).
      // TODO(schenney) The LayoutUnit(float) constructor always
      // rounds down. We should FromFloatFloor or FromFloatCeil to
      // move toward the border.
      BorderEdge edges[4];
      positioning_box_.StyleRef().GetBorderEdgeInfo(edges);
      FloatRect inner_border_rect =
          positioning_box_.StyleRef()
              .GetRoundedInnerBorderFor(unsnapped_positioning_area)
              .Rect();
      LayoutRectOutsets box_outsets = positioning_box_.BorderBoxOutsets();
      if (edges[static_cast<unsigned>(BoxSide::kTop)].ObscuresBackground()) {
        snapped_dest_adjust.SetTop(LayoutUnit(inner_border_rect.Y()) -
                                   unsnapped_dest_rect_.Y());
        unsnapped_dest_adjust.SetTop(box_outsets.Top());
      }
      if (edges[static_cast<unsigned>(BoxSide::kRight)].ObscuresBackground()) {
        snapped_dest_adjust.SetRight(unsnapped_dest_rect_.MaxX() -
                                     LayoutUnit(inner_border_rect.MaxX()));
        unsnapped_dest_adjust.SetRight(box_outsets.Right());
      }
      if (edges[static_cast<unsigned>(BoxSide::kBottom)].ObscuresBackground()) {
        snapped_dest_adjust.SetBottom(unsnapped_dest_rect_.MaxY() -
                                      LayoutUnit(inner_border_rect.MaxY()));
        unsnapped_dest_adjust.SetBottom(box_outsets.Bottom());
      }
      if (edges[static_cast<unsigned>(BoxSide::kLeft)].ObscuresBackground()) {
        snapped_dest_adjust.SetLeft(LayoutUnit(inner_border_rect.X()) -
                                    unsnapped_dest_rect_.X());
        unsnapped_dest_adjust.SetLeft(box_outsets.Left());
      }
    }
      return;
    case EFillBox::kText:
      return;
  }
}

void BackgroundImageGeometry::ComputePositioningAreaAdjustments(
    const FillLayer& fill_layer,
    const LayoutRect& unsnapped_positioning_area,
    bool disallow_border_derived_adjustment,
    LayoutRectOutsets& unsnapped_box_outset,
    LayoutRectOutsets& snapped_box_outset) const {
  switch (fill_layer.Origin()) {
    case EFillBox::kContent:
      // If the PaddingOutsets are zero then this is equivalent to
      // kPadding and we should apply the snapping logic.
      unsnapped_box_outset = positioning_box_.PaddingOutsets();
      if (!unsnapped_box_outset.IsZero()) {
        unsnapped_box_outset += positioning_box_.BorderBoxOutsets();

        // We're not trying to match a border position, so don't snap.
        snapped_box_outset = unsnapped_box_outset;
        return;
      }
      FALLTHROUGH;
    case EFillBox::kPadding:
      unsnapped_box_outset = positioning_box_.BorderBoxOutsets();
      if (disallow_border_derived_adjustment) {
        snapped_box_outset = unsnapped_box_outset;
      } else {
        // Force the snapped positioning area to fill to the borders.
        // Note that the snapped adjustments do not have the same effect as
        // pixel snapping the unsnapped rectangle. Border snapping snaps both
        // the size and position of the borders, sometimes adjusting the inner
        // border by more than a pixel when done (particularly under magnifying
        // zoom).
        FloatRect inner_border_rect =
            positioning_box_.StyleRef()
                .GetRoundedInnerBorderFor(unsnapped_positioning_area)
                .Rect();
        snapped_box_outset.SetLeft(LayoutUnit(inner_border_rect.X()) -
                                   unsnapped_positioning_area.X());
        snapped_box_outset.SetTop(LayoutUnit(inner_border_rect.Y()) -
                                  unsnapped_positioning_area.Y());
        snapped_box_outset.SetRight(unsnapped_positioning_area.MaxX() -
                                    LayoutUnit(inner_border_rect.MaxX()));
        snapped_box_outset.SetBottom(unsnapped_positioning_area.MaxY() -
                                     LayoutUnit(inner_border_rect.MaxY()));
      }
      return;
    case EFillBox::kBorder:
      // All adjustments remain 0.
      snapped_box_outset = unsnapped_box_outset = LayoutRectOutsets(0, 0, 0, 0);
      return;
    case EFillBox::kText:
      return;
  }
}

void BackgroundImageGeometry::ComputePositioningArea(
    const LayoutBoxModelObject* container,
    PaintPhase paint_phase,
    GlobalPaintFlags flags,
    const FillLayer& fill_layer,
    const LayoutRect& paint_rect,
    LayoutRect& unsnapped_positioning_area,
    LayoutRect& snapped_positioning_area,
    LayoutPoint& unsnapped_box_offset,
    LayoutPoint& snapped_box_offset) {
  if (ShouldUseFixedAttachment(fill_layer)) {
    // No snapping for fixed attachment.
    SetHasNonLocalGeometry();
    offset_in_background_ = LayoutPoint();
    unsnapped_positioning_area =
        FixedAttachmentPositioningArea(box_, container, flags);
    unsnapped_dest_rect_ = snapped_dest_rect_ = snapped_positioning_area =
        unsnapped_positioning_area;
  } else {
    unsnapped_dest_rect_ = paint_rect;

    if (painting_view_ || cell_using_container_background_)
      unsnapped_positioning_area.SetSize(positioning_size_override_);
    else
      unsnapped_positioning_area = unsnapped_dest_rect_;

    // Attempt to shrink the destination rect if possible while also ensuring
    // that it paints to the border:
    //
    //   * for background-clip content-box/padding-box, we can restrict to the
    //     respective box, but for padding-box we also try to force alignment
    //     with the inner border.
    //
    //   * for border-box, we can modify individual edges iff the border fully
    //     obscures the background.
    //
    // It is unsafe to derive dest from border information when any of the
    // following is true:
    // * the layer is not painted as part of a regular background phase
    //  (e.g.paint_phase == kMask)
    // * non-SrcOver compositing is active
    // * painting_view_ is set, meaning we're dealing with a
    //   LayoutView - for which dest rect is overflowing (expanded to cover
    //   the whole canvas).
    // * We are painting table cells using the table background, or the table
    //   has collapsed borders
    // * There is a border image, because it may not be opaque or may be outset.
    bool disallow_border_derived_adjustment =
        !ShouldPaintSelfBlockBackground(paint_phase) ||
        fill_layer.Composite() != CompositeOperator::kCompositeSourceOver ||
        painting_view_ || painting_table_cell_ ||
        positioning_box_.StyleRef().BorderImage().GetImage() ||
        positioning_box_.StyleRef().BorderCollapse() ==
            EBorderCollapse::kCollapse;

    // Compute all the outsets we need to apply to the rectangles. These
    // outsets also include the snapping behavior.
    LayoutRectOutsets unsnapped_dest_adjust;
    LayoutRectOutsets snapped_dest_adjust;
    ComputeDestRectAdjustments(fill_layer, unsnapped_positioning_area,
                               disallow_border_derived_adjustment,
                               unsnapped_dest_adjust, snapped_dest_adjust);
    LayoutRectOutsets unsnapped_box_outset;
    LayoutRectOutsets snapped_box_outset;
    ComputePositioningAreaAdjustments(fill_layer, unsnapped_positioning_area,
                                      disallow_border_derived_adjustment,
                                      unsnapped_box_outset, snapped_box_outset);

    // Apply the adjustments.
    snapped_dest_rect_ = unsnapped_dest_rect_;
    snapped_dest_rect_.Contract(snapped_dest_adjust);
    snapped_dest_rect_ = LayoutRect(PixelSnappedIntRect(snapped_dest_rect_));
    unsnapped_dest_rect_.Contract(unsnapped_dest_adjust);
    snapped_positioning_area = unsnapped_positioning_area;
    snapped_positioning_area.Contract(snapped_box_outset);
    snapped_positioning_area =
        LayoutRect(PixelSnappedIntRect(snapped_positioning_area));
    unsnapped_positioning_area.Contract(unsnapped_box_outset);

    // Offset of the positioning area from the corner of the
    // positioning_box_.
    // TODO(schenney): Could we enable dest adjust for collapsed
    // borders if we computed this based on the actual offset between
    // the rects?
    unsnapped_box_offset =
        LayoutPoint(unsnapped_box_outset.Left() - unsnapped_dest_adjust.Left(),
                    unsnapped_box_outset.Top() - unsnapped_dest_adjust.Top());
    snapped_box_offset =
        LayoutPoint(snapped_box_outset.Left() - snapped_dest_adjust.Left(),
                    snapped_box_outset.Top() - snapped_dest_adjust.Top());
    // For view backgrounds, the input paint rect is specified in root element
    // local coordinate (i.e. a transform is applied on the context for
    // painting), and is expanded to cover the whole canvas. Since left/top is
    // relative to the paint rect, we need to offset them back.
    if (painting_view_) {
      unsnapped_box_offset -= paint_rect.Location();
      snapped_box_offset -= paint_rect.Location();
    }
  }
}

void BackgroundImageGeometry::CalculateFillTileSize(
    const FillLayer& fill_layer,
    const LayoutSize& unsnapped_positioning_area_size,
    const LayoutSize& snapped_positioning_area_size) {
  StyleImage* image = fill_layer.GetImage();
  EFillSizeType type = fill_layer.SizeType();

  // Tile size is snapped for images without intrinsic dimensions (typically
  // generated content) and unsnapped for content that has intrinsic
  // dimensions. Once we choose here we stop tracking whether the tile size is
  // snapped or unsnapped.
  LayoutSize positioning_area_size = !image->HasIntrinsicSize()
                                         ? snapped_positioning_area_size
                                         : unsnapped_positioning_area_size;
  LayoutSize image_intrinsic_size(image->ImageSize(
      positioning_box_.GetDocument(),
      positioning_box_.StyleRef().EffectiveZoom(), positioning_area_size));
  switch (type) {
    case EFillSizeType::kSizeLength: {
      tile_size_ = positioning_area_size;

      const Length& layer_width = fill_layer.SizeLength().Width();
      const Length& layer_height = fill_layer.SizeLength().Height();

      if (layer_width.IsFixed()) {
        tile_size_.SetWidth(LayoutUnit(layer_width.Value()));
      } else if (layer_width.IsPercentOrCalc()) {
        tile_size_.SetWidth(
            ValueForLength(layer_width, positioning_area_size.Width()));
      }

      if (layer_height.IsFixed()) {
        tile_size_.SetHeight(LayoutUnit(layer_height.Value()));
      } else if (layer_height.IsPercentOrCalc()) {
        tile_size_.SetHeight(
            ValueForLength(layer_height, positioning_area_size.Height()));
      }

      // If one of the values is auto we have to use the appropriate
      // scale to maintain our aspect ratio.
      if (layer_width.IsAuto() && !layer_height.IsAuto()) {
        if (!image->HasIntrinsicSize()) {
          // Spec says that auto should be 100% in the absence of
          // an intrinsic ratio or size.
          tile_size_.SetWidth(positioning_area_size.Width());
        } else if (image_intrinsic_size.Height()) {
          float adjusted_width = image_intrinsic_size.Width().ToFloat() /
                                 image_intrinsic_size.Height().ToFloat() *
                                 tile_size_.Height().ToFloat();
          if (image_intrinsic_size.Width() >= 1 && adjusted_width < 1)
            adjusted_width = 1;
          tile_size_.SetWidth(LayoutUnit(adjusted_width));
        }
      } else if (!layer_width.IsAuto() && layer_height.IsAuto()) {
        if (!image->HasIntrinsicSize()) {
          // Spec says that auto should be 100% in the absence of
          // an intrinsic ratio or size.
          tile_size_.SetHeight(positioning_area_size.Height());
        } else if (image_intrinsic_size.Width()) {
          float adjusted_height = image_intrinsic_size.Height().ToFloat() /
                                  image_intrinsic_size.Width().ToFloat() *
                                  tile_size_.Width().ToFloat();
          if (image_intrinsic_size.Height() >= 1 && adjusted_height < 1)
            adjusted_height = 1;
          tile_size_.SetHeight(LayoutUnit(adjusted_height));
        }
      } else if (layer_width.IsAuto() && layer_height.IsAuto()) {
        // If both width and height are auto, use the image's intrinsic size.
        tile_size_ = image_intrinsic_size;
      }

      tile_size_.ClampNegativeToZero();
      return;
    }
    case EFillSizeType::kContain:
    case EFillSizeType::kCover: {
      // Always use the snapped positioning area size for this computation,
      // so that we resize the image to completely fill the actual painted
      // area.
      float horizontal_scale_factor =
          image_intrinsic_size.Width()
              ? snapped_positioning_area_size.Width().ToFloat() /
                    image_intrinsic_size.Width()
              : 1.0f;
      float vertical_scale_factor =
          image_intrinsic_size.Height()
              ? snapped_positioning_area_size.Height().ToFloat() /
                    image_intrinsic_size.Height()
              : 1.0f;
      // Force the dimension that determines the size to exactly match the
      // positioning_area_size in that dimension, so that rounding of floating
      // point approximation to LayoutUnit do not shrink the image to smaller
      // than the positioning_area_size.
      if (type == EFillSizeType::kContain) {
        // Snap the dependent dimension to avoid bleeding/blending artifacts
        // at the edge of the image when we paint it.
        if (horizontal_scale_factor < vertical_scale_factor) {
          tile_size_ = LayoutSize(
              snapped_positioning_area_size.Width(),
              LayoutUnit(std::max(1.0f, roundf(image_intrinsic_size.Height() *
                                               horizontal_scale_factor))));
        } else {
          tile_size_ = LayoutSize(
              LayoutUnit(std::max(1.0f, roundf(image_intrinsic_size.Width() *
                                               vertical_scale_factor))),
              snapped_positioning_area_size.Height());
        }
        return;
      }
      if (horizontal_scale_factor > vertical_scale_factor) {
        tile_size_ =
            LayoutSize(snapped_positioning_area_size.Width(),
                       LayoutUnit(std::max(1.0f, image_intrinsic_size.Height() *
                                                     horizontal_scale_factor)));
      } else {
        tile_size_ =
            LayoutSize(LayoutUnit(std::max(1.0f, image_intrinsic_size.Width() *
                                                     vertical_scale_factor)),
                       snapped_positioning_area_size.Height());
      }
      return;
    }
    case EFillSizeType::kSizeNone:
      // This value should only be used while resolving style.
      NOTREACHED();
  }

  NOTREACHED();
  return;
}

void BackgroundImageGeometry::Calculate(const LayoutBoxModelObject* container,
                                        PaintPhase paint_phase,
                                        GlobalPaintFlags flags,
                                        const FillLayer& fill_layer,
                                        const PhysicalRect& paint_rect_arg) {
  LayoutRect paint_rect = paint_rect_arg.ToLayoutRect();
  // Unsnapped positioning area is used to derive quantities
  // that reference source image maps and define non-integer values, such
  // as phase and position.
  LayoutRect unsnapped_positioning_area;

  // Snapped positioning area is used for sizing images based on the
  // background area (like cover and contain), and for setting the repeat
  // spacing.
  LayoutRect snapped_positioning_area;

  // Additional offset from the corner of the positioning_box_
  LayoutPoint unsnapped_box_offset;
  LayoutPoint snapped_box_offset;

  // This method also sets the destination rects.
  ComputePositioningArea(container, paint_phase, flags, fill_layer, paint_rect,
                         unsnapped_positioning_area, snapped_positioning_area,
                         unsnapped_box_offset, snapped_box_offset);

  // Sets the tile_size_.
  CalculateFillTileSize(fill_layer, unsnapped_positioning_area.Size(),
                        snapped_positioning_area.Size());

  EFillRepeat background_repeat_x = fill_layer.RepeatX();
  EFillRepeat background_repeat_y = fill_layer.RepeatY();

  // Maintain both snapped and unsnapped available widths and heights.
  // Unsnapped values are used for most thing, but snapped are used
  // to computed sizes that must fill the area, such as round and space.
  LayoutUnit unsnapped_available_width =
      unsnapped_positioning_area.Width() - tile_size_.Width();
  LayoutUnit unsnapped_available_height =
      unsnapped_positioning_area.Height() - tile_size_.Height();
  LayoutUnit snapped_available_width =
      snapped_positioning_area.Width() - tile_size_.Width();
  LayoutUnit snapped_available_height =
      snapped_positioning_area.Height() - tile_size_.Height();
  LayoutSize snapped_positioning_area_size = snapped_positioning_area.Size();

  // Computed position is for placing things within the destination, so use
  // snapped values.
  LayoutUnit computed_x_position =
      MinimumValueForLength(fill_layer.PositionX(), snapped_available_width) -
      offset_in_background_.X();
  LayoutUnit computed_y_position =
      MinimumValueForLength(fill_layer.PositionY(), snapped_available_height) -
      offset_in_background_.Y();

  if (background_repeat_x == EFillRepeat::kRoundFill &&
      snapped_positioning_area_size.Width() > LayoutUnit() &&
      tile_size_.Width() > LayoutUnit()) {
    int nr_tiles = std::max(
        1,
        RoundToInt(snapped_positioning_area_size.Width() / tile_size_.Width()));
    LayoutUnit rounded_width = snapped_positioning_area_size.Width() / nr_tiles;

    // Maintain aspect ratio if background-size: auto is set
    if (fill_layer.SizeLength().Height().IsAuto() &&
        background_repeat_y != EFillRepeat::kRoundFill) {
      tile_size_.SetHeight(tile_size_.Height() * rounded_width /
                           tile_size_.Width());
    }
    tile_size_.SetWidth(rounded_width);

    // Force the first tile to line up with the edge of the positioning area.
    SetPhaseX(tile_size_.Width()
                  ? tile_size_.Width() -
                        fmodf(computed_x_position + unsnapped_box_offset.X(),
                              tile_size_.Width())
                  : 0);
    SetSpaceSize(LayoutSize());
  }

  if (background_repeat_y == EFillRepeat::kRoundFill &&
      snapped_positioning_area_size.Height() > LayoutUnit() &&
      tile_size_.Height() > LayoutUnit()) {
    int nr_tiles =
        std::max(1, RoundToInt(snapped_positioning_area_size.Height() /
                               tile_size_.Height()));
    LayoutUnit rounded_height =
        snapped_positioning_area_size.Height() / nr_tiles;
    // Maintain aspect ratio if background-size: auto is set
    if (fill_layer.SizeLength().Width().IsAuto() &&
        background_repeat_x != EFillRepeat::kRoundFill) {
      tile_size_.SetWidth(tile_size_.Width() * rounded_height /
                          tile_size_.Height());
    }
    tile_size_.SetHeight(rounded_height);

    // Force the first tile to line up with the edge of the positioning area.
    SetPhaseY(tile_size_.Height()
                  ? tile_size_.Height() -
                        fmodf(computed_y_position + unsnapped_box_offset.Y(),
                              tile_size_.Height())
                  : 0);
    SetSpaceSize(LayoutSize());
  }

  if (background_repeat_x == EFillRepeat::kRepeatFill) {
    // Repeat must set the phase accurately, so use unsnapped values.
    SetRepeatX(fill_layer, unsnapped_available_width, unsnapped_box_offset.X());
  } else if (background_repeat_x == EFillRepeat::kSpaceFill &&
             tile_size_.Width() > LayoutUnit()) {
    // SpaceFill uses snapped values to fill the painted area.
    LayoutUnit space = GetSpaceBetweenImageTiles(
        snapped_positioning_area_size.Width(), tile_size_.Width());
    if (space >= LayoutUnit())
      SetSpaceX(space, snapped_box_offset.X());
    else
      background_repeat_x = EFillRepeat::kNoRepeatFill;
  }
  if (background_repeat_x == EFillRepeat::kNoRepeatFill) {
    // NoRepeat moves the dest rects, so needs both snapped and
    // unsnapped parameters.
    LayoutUnit x_offset =
        fill_layer.BackgroundXOrigin() == BackgroundEdgeOrigin::kRight
            ? unsnapped_available_width - computed_x_position
            : computed_x_position;
    LayoutUnit snapped_x_offset =
        fill_layer.BackgroundXOrigin() == BackgroundEdgeOrigin::kRight
            ? snapped_available_width - computed_x_position
            : computed_x_position;
    SetNoRepeatX(fill_layer, unsnapped_box_offset.X() + x_offset,
                 snapped_box_offset.X() + snapped_x_offset);
    if (offset_in_background_.X() > tile_size_.Width())
      unsnapped_dest_rect_ = snapped_dest_rect_ = LayoutRect();
  }

  if (background_repeat_y == EFillRepeat::kRepeatFill) {
    // Repeat must set the phase accurately, so use unsnapped values.
    SetRepeatY(fill_layer, unsnapped_available_height,
               unsnapped_box_offset.Y());
  } else if (background_repeat_y == EFillRepeat::kSpaceFill &&
             tile_size_.Height() > LayoutUnit()) {
    // SpaceFill uses snapped values to fill the painted area.
    LayoutUnit space = GetSpaceBetweenImageTiles(
        snapped_positioning_area_size.Height(), tile_size_.Height());
    if (space >= LayoutUnit())
      SetSpaceY(space, snapped_box_offset.Y());
    else
      background_repeat_y = EFillRepeat::kNoRepeatFill;
  }
  if (background_repeat_y == EFillRepeat::kNoRepeatFill) {
    // NoRepeat moves the dest rects, so needs both snapped and
    // unsnapped parameters.
    LayoutUnit y_offset =
        fill_layer.BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom
            ? unsnapped_available_height - computed_y_position
            : computed_y_position;
    LayoutUnit snapped_y_offset =
        fill_layer.BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom
            ? snapped_available_height - computed_y_position
            : computed_y_position;
    SetNoRepeatY(fill_layer, unsnapped_box_offset.Y() + y_offset,
                 snapped_box_offset.Y() + snapped_y_offset);
    if (offset_in_background_.Y() > tile_size_.Height())
      unsnapped_dest_rect_ = snapped_dest_rect_ = LayoutRect();
  }

  if (ShouldUseFixedAttachment(fill_layer))
    UseFixedAttachment(paint_rect.Location());

  // Clip the final output rect to the paint rect, maintaining snapping.
  unsnapped_dest_rect_.Intersect(paint_rect);
  snapped_dest_rect_.Intersect(LayoutRect(PixelSnappedIntRect(paint_rect)));
}

const ImageResourceObserver& BackgroundImageGeometry::ImageClient() const {
  return painting_view_ ? box_ : positioning_box_;
}

const Document& BackgroundImageGeometry::ImageDocument() const {
  return box_.GetDocument();
}

const ComputedStyle& BackgroundImageGeometry::ImageStyle() const {
  const bool use_style_from_positioning_box =
      painting_view_ || cell_using_container_background_;
  return (use_style_from_positioning_box ? positioning_box_ : box_).StyleRef();
}

InterpolationQuality BackgroundImageGeometry::ImageInterpolationQuality()
    const {
  return box_.StyleRef().GetInterpolationQuality();
}

}  // namespace blink
