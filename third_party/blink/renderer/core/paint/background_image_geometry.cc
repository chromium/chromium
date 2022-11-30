// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/background_image_geometry.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_col.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

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

LayoutUnit ComputeRoundedTileSize(LayoutUnit area_size, LayoutUnit tile_size) {
  int nr_tiles = std::max(1, RoundToInt(area_size / tile_size));
  return area_size / nr_tiles;
}

LayoutUnit ComputeTilePhase(LayoutUnit position, LayoutUnit tile_extent) {
  // Assuming a non-integral number of tiles, find out how much of the
  // partial tile is visible. That is the phase.
  return tile_extent ? tile_extent - IntMod(position, tile_extent)
                     : LayoutUnit();
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
    SetPhaseX(-x_offset);
    SetSpaceSize(
        PhysicalSize(unsnapped_dest_rect_.Width(), SpaceSize().height));
    return;
  }

  if (x_offset > 0) {
    // Move the dest rect if the offset is positive. The image "stays" where
    // it is over the dest rect, so this effectively modifies the phase.
    unsnapped_dest_rect_.Move(PhysicalOffset(x_offset, LayoutUnit()));
    snapped_dest_rect_.SetX(LayoutUnit(unsnapped_dest_rect_.X().Round()));

    // Make the dest as wide as a tile, which will reduce the dest
    // rect if the tile is too small to fill the paint_rect. If not,
    // the dest rect will be clipped when intersected with the paint
    // rect.
    unsnapped_dest_rect_.SetWidth(tile_size_.width);
    snapped_dest_rect_.SetWidth(tile_size_.width);

    SetPhaseX(LayoutUnit());
  } else {
    // Otherwise, if the offset is negative use it to move the image under
    // the dest rect (since we can't paint outside the paint_rect).
    SetPhaseX(-x_offset);

    // Reduce the width of the dest rect to draw only the portion of the
    // tile that remains visible after offsetting the image.
    // TODO(schenney): This might grow the dest rect if the dest rect has
    // been adjusted for opaque borders.
    unsnapped_dest_rect_.SetWidth(tile_size_.width + x_offset);
    snapped_dest_rect_.SetWidth(tile_size_.width + snapped_x_offset);
  }

  // Force the horizontal space to zero, retaining vertical.
  SetSpaceSize(PhysicalSize(LayoutUnit(), SpaceSize().height));
}

void BackgroundImageGeometry::SetNoRepeatY(const FillLayer& fill_layer,
                                           LayoutUnit y_offset,
                                           LayoutUnit snapped_y_offset) {
  if (NeedsFullSizeDestination(fill_layer)) {
    SetPhaseY(-y_offset);
    SetSpaceSize(
        PhysicalSize(SpaceSize().width, unsnapped_dest_rect_.Height()));
    return;
  }

  if (y_offset > 0) {
    // Move the dest rect if the offset is positive. The image "stays" where
    // it is in the paint rect, so this effectively modifies the phase.
    unsnapped_dest_rect_.Move(PhysicalOffset(LayoutUnit(), y_offset));
    snapped_dest_rect_.SetY(LayoutUnit(unsnapped_dest_rect_.Y().Round()));

    // Make the dest as wide as a tile, which will reduce the dest
    // rect if the tile is too small to fill the paint_rect. If not,
    // the dest rect will be clipped when intersected with the paint
    // rect.
    unsnapped_dest_rect_.SetHeight(tile_size_.height);
    snapped_dest_rect_.SetHeight(tile_size_.height);

    SetPhaseY(LayoutUnit());
  } else {
    // Otherwise, if the offset is negative, use it to move the image under
    // the dest rect (since we can't paint outside the paint_rect).
    SetPhaseY(-y_offset);

    // Reduce the height of the dest rect to draw only the portion of the
    // tile that remains visible after offsetting the image.
    // TODO(schenney): This might grow the dest rect if the dest rect has
    // been adjusted for opaque borders.
    unsnapped_dest_rect_.SetHeight(tile_size_.height + y_offset);
    snapped_dest_rect_.SetHeight(tile_size_.height + snapped_y_offset);
  }

  // Force the vertical space to zero, retaining horizontal.
  SetSpaceSize(PhysicalSize(SpaceSize().width, LayoutUnit()));
}

void BackgroundImageGeometry::SetRepeatX(const FillLayer& fill_layer,
                                         LayoutUnit available_width,
                                         LayoutUnit extra_offset) {
  // All values are unsnapped to accurately set phase in the presence of
  // zoom and large values. That is, accurately render the
  // background-position value.
  if (tile_size_.width) {
    // Recompute computed_position because here we need to resolve against
    // unsnapped widths to correctly set the phase.
    LayoutUnit computed_position =
        MinimumValueForLength(fill_layer.PositionX(), available_width) -
        OffsetInBackground(fill_layer).left;
    // Convert from edge-relative form to absolute.
    if (fill_layer.BackgroundXOrigin() == BackgroundEdgeOrigin::kRight)
      computed_position = available_width - computed_position;

    SetPhaseX(
        ComputeTilePhase(computed_position + extra_offset, tile_size_.width));
  } else {
    SetPhaseX(LayoutUnit());
  }
  SetSpaceSize(PhysicalSize(LayoutUnit(), SpaceSize().height));
}

void BackgroundImageGeometry::SetRepeatY(const FillLayer& fill_layer,
                                         LayoutUnit available_height,
                                         LayoutUnit extra_offset) {
  // All values are unsnapped to accurately set phase in the presence of
  // zoom and large values. That is, accurately render the
  // background-position value.
  if (tile_size_.height) {
    // Recompute computed_position because here we need to resolve against
    // unsnapped widths to correctly set the phase.
    LayoutUnit computed_position =
        MinimumValueForLength(fill_layer.PositionY(), available_height) -
        OffsetInBackground(fill_layer).top;
    // Convert from edge-relative form to absolute.
    if (fill_layer.BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom)
      computed_position = available_height - computed_position;

    SetPhaseY(
        ComputeTilePhase(computed_position + extra_offset, tile_size_.height));
  } else {
    SetPhaseY(LayoutUnit());
  }
  SetSpaceSize(PhysicalSize(SpaceSize().width, LayoutUnit()));
}

void BackgroundImageGeometry::SetSpaceX(LayoutUnit space,
                                        LayoutUnit extra_offset) {
  SetSpaceSize(PhysicalSize(space, SpaceSize().height));
  // Modify the phase to start a full tile at the edge of the paint area.
  SetPhaseX(ComputeTilePhase(extra_offset, tile_size_.width + space));
}

void BackgroundImageGeometry::SetSpaceY(LayoutUnit space,
                                        LayoutUnit extra_offset) {
  SetSpaceSize(PhysicalSize(SpaceSize().width, space));
  // Modify the phase to start a full tile at the edge of the paint area.
  SetPhaseY(ComputeTilePhase(extra_offset, tile_size_.height + space));
}

void BackgroundImageGeometry::UseFixedAttachment(
    const PhysicalOffset& attachment_point) {
  DCHECK(has_background_fixed_to_viewport_);
  PhysicalOffset fixed_adjustment =
      attachment_point - unsnapped_dest_rect_.offset;
  fixed_adjustment.ClampNegativeToZero();
  phase_ += fixed_adjustment;
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

PhysicalOffset BackgroundImageGeometry::GetPositioningOffsetForCell(
    const LayoutTableCell& cell,
    const LayoutBox& positioning_box) {
  LayoutUnit h_border_spacing(cell.Table()->HBorderSpacing());
  LayoutUnit v_border_spacing(cell.Table()->VBorderSpacing());
  // TODO(layout-ng): It looks incorrect to use Location() in this function.
  if (positioning_box.IsTableSection()) {
    return PhysicalOffset(cell.Location().X() - h_border_spacing,
                          cell.Location().Y() - v_border_spacing);
  }
  if (positioning_box.IsLegacyTableRow()) {
    return PhysicalOffset(cell.Location().X() - h_border_spacing, LayoutUnit());
  }

  PhysicalRect sections_rect(PhysicalOffset(), cell.Table()->Size());
  cell.Table()->SubtractCaptionRect(sections_rect);
  LayoutUnit height_of_captions =
      cell.Table()->Size().Height() - sections_rect.Height();
  PhysicalOffset offset_in_background = PhysicalOffset(
      LayoutUnit(), (cell.Section()->Location().Y() -
                     cell.Table()->BorderBefore() - height_of_captions) +
                        cell.Location().Y());

  const auto& table_col = To<LayoutTableCol>(positioning_box);
  if (table_col.IsTableColumn()) {
    offset_in_background.top -= v_border_spacing;
    return offset_in_background;
  }

  DCHECK(table_col.IsTableColumnGroup());
  LayoutUnit offset = offset_in_background.left;
  ExpandToTableColumnGroup(cell, table_col, offset, kColumnGroupStart);
  offset_in_background.left += offset;
  offset_in_background.top -= v_border_spacing;
  return offset_in_background;
}

PhysicalSize BackgroundImageGeometry::GetBackgroundObjectDimensions(
    const LayoutTableCell& cell,
    const LayoutBox& positioning_box) {
  PhysicalSize border_spacing(LayoutUnit(cell.Table()->HBorderSpacing()),
                              LayoutUnit(cell.Table()->VBorderSpacing()));
  if (positioning_box.IsTableSection()) {
    return PhysicalSizeToBeNoop(positioning_box.Size()) - border_spacing -
           border_spacing;
  }

  if (positioning_box.IsTableRow()) {
    return PhysicalSizeToBeNoop(positioning_box.Size()) -
           PhysicalSize(border_spacing.width, LayoutUnit()) -
           PhysicalSize(border_spacing.width, LayoutUnit());
  }

  DCHECK(positioning_box.IsLayoutTableCol());
  PhysicalRect sections_rect(PhysicalOffset(), cell.Table()->Size());
  cell.Table()->SubtractCaptionRect(sections_rect);
  LayoutUnit column_height = sections_rect.Height() -
                             cell.Table()->BorderBefore() -
                             border_spacing.height - border_spacing.height;
  const auto& table_col = To<LayoutTableCol>(positioning_box);
  if (table_col.IsTableColumn())
    return PhysicalSize(cell.Size().Width(), column_height);

  DCHECK(table_col.IsTableColumnGroup());
  LayoutUnit width = cell.Size().Width();
  ExpandToTableColumnGroup(cell, table_col, width, kColumnGroupStart);
  ExpandToTableColumnGroup(cell, table_col, width, kColumnGroupEnd);

  return PhysicalSize(width, column_height);
}

bool BackgroundImageGeometry::ShouldUseFixedAttachment(
    const FillLayer& fill_layer) const {
  // Only backgrounds fixed to viewport should be treated as fixed attachment.
  // See comments in the private constructor.
  return has_background_fixed_to_viewport_ &&
         // Solid color background should use default attachment.
         fill_layer.GetImage() &&
         fill_layer.Attachment() == EFillAttachment::kFixed;
}

namespace {

PhysicalRect FixedAttachmentPositioningArea(const PaintInfo& paint_info,
                                            const LayoutBoxModelObject& obj) {
  DCHECK(obj.View());
  gfx::PointF viewport_origin_in_local_space =
      GeometryMapper::SourceToDestinationProjection(
          obj.View()->FirstFragment().LocalBorderBoxProperties().Transform(),
          paint_info.context.GetPaintController()
              .CurrentPaintChunkProperties()
              .Transform())
          .MapPoint(gfx::PointF());
  DCHECK(obj.GetFrameView());
  const ScrollableArea* layout_viewport = obj.GetFrameView()->LayoutViewport();
  DCHECK(layout_viewport);
  return PhysicalRect(
      PhysicalOffset::FromPointFRound(viewport_origin_in_local_space),
      PhysicalSize(layout_viewport->VisibleContentRect().size()));
}

// Computes the stitched table-grid rect relative to the current fragment.
PhysicalRect ComputeStitchedTableGridRect(
    const NGPhysicalBoxFragment& fragment) {
  const auto writing_direction = fragment.Style().GetWritingDirection();
  LogicalRect table_grid_rect;
  LogicalRect fragment_local_grid_rect;
  LayoutUnit stitched_block_size;

  for (const NGPhysicalBoxFragment& walker :
       To<LayoutBox>(fragment.GetLayoutObject())->PhysicalFragments()) {
    LogicalRect local_grid_rect = walker.TableGridRect();
    local_grid_rect.offset.block_offset += stitched_block_size;
    if (table_grid_rect.IsEmpty())
      table_grid_rect = local_grid_rect;
    else
      table_grid_rect.Unite(local_grid_rect);

    if (&walker == &fragment)
      fragment_local_grid_rect = local_grid_rect;

    stitched_block_size += NGFragment(writing_direction, walker).BlockSize();
  }

  // Make the rect relative to the fragment we are currently painting.
  table_grid_rect.offset.block_offset -=
      fragment_local_grid_rect.offset.block_offset;

  WritingModeConverter converter(
      writing_direction, ToPhysicalSize(fragment_local_grid_rect.size,
                                        writing_direction.GetWritingMode()));
  return converter.ToPhysical(table_grid_rect);
}

}  // Anonymous namespace

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutView& view,
    const PhysicalOffset& element_positioning_area_offset)
    : box_(&view), positioning_box_(&view.RootBox()) {
  has_background_fixed_to_viewport_ =
      view.StyleRef().HasFixedAttachmentBackgroundImage();
  painting_view_ = true;
  // The background of the box generated by the root element covers the
  // entire canvas and will be painted by the view object, but the we should
  // still use the root element box for positioning.
  positioning_size_override_ = PhysicalSizeToBeNoop(view.RootBox().Size());
  // The background image should paint from the root element's coordinate space.
  element_positioning_area_offset_ = element_positioning_area_offset;
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutBoxModelObject& obj)
    : BackgroundImageGeometry(&obj, &obj) {}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutTableCell& cell,
    const LayoutObject* background_object)
    : BackgroundImageGeometry(
          &cell,
          background_object && !background_object->IsTableCell()
              ? &To<LayoutBoxModelObject>(*background_object)
              : &cell) {
  painting_table_cell_ = true;
  cell_using_container_background_ =
      background_object && !background_object->IsTableCell();
  if (cell_using_container_background_) {
    element_positioning_area_offset_ =
        GetPositioningOffsetForCell(cell, To<LayoutBox>(*background_object));
    positioning_size_override_ =
        GetBackgroundObjectDimensions(cell, To<LayoutBox>(*background_object));
  }
}

// TablesNG background painting.
BackgroundImageGeometry::BackgroundImageGeometry(const LayoutNGTableCell& cell,
                                                 PhysicalOffset cell_offset,
                                                 const LayoutBox& table_part,
                                                 PhysicalSize table_part_size)
    : BackgroundImageGeometry(&cell, &table_part) {
  painting_table_cell_ = true;
  cell_using_container_background_ = true;
  element_positioning_area_offset_ = cell_offset;
  positioning_size_override_ = table_part_size;
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const NGPhysicalBoxFragment& fragment)
    : BackgroundImageGeometry(
          To<LayoutBoxModelObject>(fragment.GetLayoutObject()),
          To<LayoutBoxModelObject>(fragment.GetLayoutObject())) {
  DCHECK(box_->IsBox());

  if (fragment.IsTableNG()) {
    auto stitched_background_rect = ComputeStitchedTableGridRect(fragment);
    positioning_size_override_ = stitched_background_rect.size;
    element_positioning_area_offset_ = -stitched_background_rect.offset;
    box_has_multiple_fragments_ = !fragment.IsOnlyForNode();
  } else if (!fragment.IsOnlyForNode()) {
    // The element is block-fragmented. We need to calculate the correct
    // background offset within an imaginary box where all the fragments have
    // been stitched together.
    element_positioning_area_offset_ =
        OffsetInStitchedFragments(fragment, &positioning_size_override_);
    box_has_multiple_fragments_ = true;
  }
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutBoxModelObject* box,
    const LayoutBoxModelObject* positioning_box)
    : box_(box), positioning_box_(positioning_box) {
  // Specialized constructor should be used for LayoutView.
  DCHECK(!IsA<LayoutView>(box));
  DCHECK(box);
  DCHECK(positioning_box);
  if (positioning_box->StyleRef().HasFixedAttachmentBackgroundImage()) {
    has_background_fixed_to_viewport_ = true;
    // https://www.w3.org/TR/css-transforms-1/#transform-rendering
    // Fixed backgrounds on the root element are affected by any transform
    // specified for that element. For all other elements that are effected
    // by a transform, a value of fixed for the background-attachment property
    // is treated as if it had a value of scroll.
    for (const PaintLayer* layer = box->EnclosingLayer();
         layer && !layer->IsRootLayer(); layer = layer->Parent()) {
      // Check LayoutObject::HasTransformRelatedProperty() first to exclude
      // non-applicable transforms and will-change: transform.
      LayoutObject& object = layer->GetLayoutObject();
      if (object.HasTransformRelatedProperty() &&
          (layer->Transform() ||
           object.StyleRef().HasWillChangeHintForAnyTransformProperty())) {
        has_background_fixed_to_viewport_ = false;
        break;
      }
    }
  }
}

void BackgroundImageGeometry::ComputeDestRectAdjustments(
    const FillLayer& fill_layer,
    const PhysicalRect& unsnapped_positioning_area,
    bool disallow_border_derived_adjustment,
    LayoutRectOutsets& unsnapped_dest_adjust,
    LayoutRectOutsets& snapped_dest_adjust) const {
  switch (fill_layer.Clip()) {
    case EFillBox::kContent:
      // If the PaddingOutsets are zero then this is equivalent to
      // kPadding and we should apply the snapping logic.
      unsnapped_dest_adjust = positioning_box_->PaddingOutsets();
      if (!unsnapped_dest_adjust.IsZero()) {
        unsnapped_dest_adjust += positioning_box_->BorderBoxOutsets();

        // We're not trying to match a border position, so don't snap.
        snapped_dest_adjust = unsnapped_dest_adjust;
        return;
      }
      [[fallthrough]];
    case EFillBox::kPadding:
      unsnapped_dest_adjust = positioning_box_->BorderBoxOutsets();
      if (disallow_border_derived_adjustment) {
        // Nothing to drive snapping behavior, so don't snap.
        snapped_dest_adjust = unsnapped_dest_adjust;
      } else {
        // Force the snapped dest rect to match the inner border to
        // avoid gaps between the background and border.
        // TODO(schenney) The LayoutUnit(float) constructor always
        // rounds down. We should FromFloatFloor or FromFloatCeil to
        // move toward the border.
        gfx::RectF inner_border_rect =
            RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
                positioning_box_->StyleRef(), unsnapped_positioning_area)
                .Rect();
        snapped_dest_adjust.SetLeft(LayoutUnit(inner_border_rect.x()) -
                                    unsnapped_dest_rect_.X());
        snapped_dest_adjust.SetTop(LayoutUnit(inner_border_rect.y()) -
                                   unsnapped_dest_rect_.Y());
        snapped_dest_adjust.SetRight(unsnapped_dest_rect_.Right() -
                                     LayoutUnit(inner_border_rect.right()));
        snapped_dest_adjust.SetBottom(unsnapped_dest_rect_.Bottom() -
                                      LayoutUnit(inner_border_rect.bottom()));
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
      positioning_box_->StyleRef().GetBorderEdgeInfo(edges);
      gfx::RectF inner_border_rect =
          RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
              positioning_box_->StyleRef(), unsnapped_positioning_area)
              .Rect();
      LayoutRectOutsets box_outsets = positioning_box_->BorderBoxOutsets();
      if (edges[static_cast<unsigned>(BoxSide::kTop)].ObscuresBackground()) {
        snapped_dest_adjust.SetTop(LayoutUnit(inner_border_rect.y()) -
                                   unsnapped_dest_rect_.Y());
        unsnapped_dest_adjust.SetTop(box_outsets.Top());
      }
      if (edges[static_cast<unsigned>(BoxSide::kRight)].ObscuresBackground()) {
        snapped_dest_adjust.SetRight(unsnapped_dest_rect_.Right() -
                                     LayoutUnit(inner_border_rect.right()));
        unsnapped_dest_adjust.SetRight(box_outsets.Right());
      }
      if (edges[static_cast<unsigned>(BoxSide::kBottom)].ObscuresBackground()) {
        snapped_dest_adjust.SetBottom(unsnapped_dest_rect_.Bottom() -
                                      LayoutUnit(inner_border_rect.bottom()));
        unsnapped_dest_adjust.SetBottom(box_outsets.Bottom());
      }
      if (edges[static_cast<unsigned>(BoxSide::kLeft)].ObscuresBackground()) {
        snapped_dest_adjust.SetLeft(LayoutUnit(inner_border_rect.x()) -
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
    const PhysicalRect& unsnapped_positioning_area,
    bool disallow_border_derived_adjustment,
    LayoutRectOutsets& unsnapped_box_outset,
    LayoutRectOutsets& snapped_box_outset) const {
  switch (fill_layer.Origin()) {
    case EFillBox::kContent:
      // If the PaddingOutsets are zero then this is equivalent to
      // kPadding and we should apply the snapping logic.
      unsnapped_box_outset = positioning_box_->PaddingOutsets();
      if (!unsnapped_box_outset.IsZero()) {
        unsnapped_box_outset += positioning_box_->BorderBoxOutsets();

        // We're not trying to match a border position, so don't snap.
        snapped_box_outset = unsnapped_box_outset;
        return;
      }
      [[fallthrough]];
    case EFillBox::kPadding:
      unsnapped_box_outset = positioning_box_->BorderBoxOutsets();
      if (disallow_border_derived_adjustment) {
        snapped_box_outset = unsnapped_box_outset;
      } else {
        // Force the snapped positioning area to fill to the borders.
        // Note that the snapped adjustments do not have the same effect as
        // pixel snapping the unsnapped rectangle. Border snapping snaps both
        // the size and position of the borders, sometimes adjusting the inner
        // border by more than a pixel when done (particularly under magnifying
        // zoom).
        gfx::RectF inner_border_rect =
            RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
                positioning_box_->StyleRef(), unsnapped_positioning_area)
                .Rect();
        snapped_box_outset.SetLeft(LayoutUnit(inner_border_rect.x()) -
                                   unsnapped_positioning_area.X());
        snapped_box_outset.SetTop(LayoutUnit(inner_border_rect.y()) -
                                  unsnapped_positioning_area.Y());
        snapped_box_outset.SetRight(unsnapped_positioning_area.Right() -
                                    LayoutUnit(inner_border_rect.right()));
        snapped_box_outset.SetBottom(unsnapped_positioning_area.Bottom() -
                                     LayoutUnit(inner_border_rect.bottom()));
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
    const PaintInfo& paint_info,
    const FillLayer& fill_layer,
    const PhysicalRect& paint_rect,
    PhysicalRect& unsnapped_positioning_area,
    PhysicalRect& snapped_positioning_area,
    PhysicalOffset& unsnapped_box_offset,
    PhysicalOffset& snapped_box_offset) {
  if (ShouldUseFixedAttachment(fill_layer)) {
    // No snapping for fixed attachment.
    unsnapped_positioning_area =
        FixedAttachmentPositioningArea(paint_info, *box_);
    unsnapped_dest_rect_ = snapped_dest_rect_ = snapped_positioning_area =
        unsnapped_positioning_area;
  } else {
    unsnapped_dest_rect_ = paint_rect;

    if (painting_view_ || cell_using_container_background_ ||
        box_has_multiple_fragments_)
      unsnapped_positioning_area.size = positioning_size_override_;
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
    // * We are painting a block-fragmented box.
    // * There is a border image, because it may not be opaque or may be outset.
    bool disallow_border_derived_adjustment =
        !ShouldPaintSelfBlockBackground(paint_info.phase) ||
        fill_layer.Composite() != CompositeOperator::kCompositeSourceOver ||
        painting_view_ || painting_table_cell_ || box_has_multiple_fragments_ ||
        positioning_box_->StyleRef().BorderImage().GetImage() ||
        positioning_box_->StyleRef().BorderCollapse() ==
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
    snapped_dest_rect_ = PhysicalRect(ToPixelSnappedRect(snapped_dest_rect_));
    snapped_dest_rect_.size.ClampNegativeToZero();
    unsnapped_dest_rect_.Contract(unsnapped_dest_adjust);
    unsnapped_dest_rect_.size.ClampNegativeToZero();
    snapped_positioning_area = unsnapped_positioning_area;
    snapped_positioning_area.Contract(snapped_box_outset);
    snapped_positioning_area =
        PhysicalRect(ToPixelSnappedRect(snapped_positioning_area));
    snapped_positioning_area.size.ClampNegativeToZero();
    unsnapped_positioning_area.Contract(unsnapped_box_outset);
    unsnapped_positioning_area.size.ClampNegativeToZero();

    // Offset of the positioning area from the corner of the
    // positioning_box_->
    // TODO(schenney): Could we enable dest adjust for collapsed
    // borders if we computed this based on the actual offset between
    // the rects?
    unsnapped_box_offset = PhysicalOffset(
        unsnapped_box_outset.Left() - unsnapped_dest_adjust.Left(),
        unsnapped_box_outset.Top() - unsnapped_dest_adjust.Top());
    snapped_box_offset =
        PhysicalOffset(snapped_box_outset.Left() - snapped_dest_adjust.Left(),
                       snapped_box_outset.Top() - snapped_dest_adjust.Top());
  }
}

void BackgroundImageGeometry::CalculateFillTileSize(
    const FillLayer& fill_layer,
    const PhysicalSize& unsnapped_positioning_area_size,
    const PhysicalSize& snapped_positioning_area_size) {
  StyleImage* image = fill_layer.GetImage();
  EFillSizeType type = fill_layer.SizeType();

  // Tile size is snapped for images without intrinsic dimensions (typically
  // generated content) and unsnapped for content that has intrinsic
  // dimensions. Once we choose here we stop tracking whether the tile size is
  // snapped or unsnapped.
  PhysicalSize positioning_area_size = !image->HasIntrinsicSize()
                                           ? snapped_positioning_area_size
                                           : unsnapped_positioning_area_size;
  PhysicalSize image_intrinsic_size = PhysicalSize::FromSizeFFloor(
      image->ImageSize(positioning_box_->StyleRef().EffectiveZoom(),
                       gfx::SizeF(positioning_area_size),
                       LayoutObject::ShouldRespectImageOrientation(box_)));
  switch (type) {
    case EFillSizeType::kSizeLength: {
      tile_size_ = positioning_area_size;

      const Length& layer_width = fill_layer.SizeLength().Width();
      const Length& layer_height = fill_layer.SizeLength().Height();

      if (layer_width.IsFixed()) {
        tile_size_.width = LayoutUnit(layer_width.Value());
      } else if (layer_width.IsPercentOrCalc()) {
        tile_size_.width =
            ValueForLength(layer_width, positioning_area_size.width);
      }

      if (layer_height.IsFixed()) {
        tile_size_.height = LayoutUnit(layer_height.Value());
      } else if (layer_height.IsPercentOrCalc()) {
        tile_size_.height =
            ValueForLength(layer_height, positioning_area_size.height);
      }

      // If one of the values is auto we have to use the appropriate
      // scale to maintain our aspect ratio.
      if (layer_width.IsAuto() && !layer_height.IsAuto()) {
        if (!image->HasIntrinsicSize()) {
          // Spec says that auto should be 100% in the absence of
          // an intrinsic ratio or size.
          tile_size_.width = positioning_area_size.width;
        } else if (image_intrinsic_size.height) {
          LayoutUnit adjusted_width = tile_size_.height.MulDiv(
              image_intrinsic_size.width, image_intrinsic_size.height);
          if (image_intrinsic_size.width >= 1 && adjusted_width < 1)
            adjusted_width = LayoutUnit(1);
          tile_size_.width = adjusted_width;
        }
      } else if (!layer_width.IsAuto() && layer_height.IsAuto()) {
        if (!image->HasIntrinsicSize()) {
          // Spec says that auto should be 100% in the absence of
          // an intrinsic ratio or size.
          tile_size_.height = positioning_area_size.height;
        } else if (image_intrinsic_size.width) {
          LayoutUnit adjusted_height = tile_size_.width.MulDiv(
              image_intrinsic_size.height, image_intrinsic_size.width);
          if (image_intrinsic_size.height >= 1 && adjusted_height < 1)
            adjusted_height = LayoutUnit(1);
          tile_size_.height = adjusted_height;
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
      if (image_intrinsic_size.IsEmpty()) {
        tile_size_ = snapped_positioning_area_size;
        return;
      }
      // Always use the snapped positioning area size for this computation,
      // so that we resize the image to completely fill the actual painted
      // area.
      // Force the dimension that determines the size to exactly match the
      // positioning_area_size in that dimension.
      tile_size_ = snapped_positioning_area_size.FitToAspectRatio(
          image_intrinsic_size, type == EFillSizeType::kCover
                                    ? kAspectRatioFitGrow
                                    : kAspectRatioFitShrink);
      // Snap the dependent dimension to avoid bleeding/blending artifacts
      // at the edge of the image when we paint it.
      if (type == EFillSizeType::kContain) {
        if (tile_size_.width != snapped_positioning_area_size.width)
          tile_size_.width = LayoutUnit(std::max(1, tile_size_.width.Round()));
        if (tile_size_.height != snapped_positioning_area_size.height) {
          tile_size_.height =
              LayoutUnit(std::max(1, tile_size_.height.Round()));
        }
      } else {
        if (tile_size_.width != snapped_positioning_area_size.width)
          tile_size_.width = std::max(LayoutUnit(1), tile_size_.width);
        if (tile_size_.height != snapped_positioning_area_size.height)
          tile_size_.height = std::max(LayoutUnit(1), tile_size_.height);
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

void BackgroundImageGeometry::Calculate(const PaintInfo& paint_info,
                                        const FillLayer& fill_layer,
                                        const PhysicalRect& paint_rect) {
  DCHECK_GE(box_->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  // Unsnapped positioning area is used to derive quantities
  // that reference source image maps and define non-integer values, such
  // as phase and position.
  PhysicalRect unsnapped_positioning_area;

  // Snapped positioning area is used for sizing images based on the
  // background area (like cover and contain), and for setting the repeat
  // spacing.
  PhysicalRect snapped_positioning_area;

  // Additional offset from the corner of the positioning_box_
  PhysicalOffset unsnapped_box_offset;
  PhysicalOffset snapped_box_offset;

  // This method also sets the destination rects.
  ComputePositioningArea(paint_info, fill_layer, paint_rect,
                         unsnapped_positioning_area, snapped_positioning_area,
                         unsnapped_box_offset, snapped_box_offset);

  // Sets the tile_size_.
  CalculateFillTileSize(fill_layer, unsnapped_positioning_area.size,
                        snapped_positioning_area.size);

  EFillRepeat background_repeat_x = fill_layer.RepeatX();
  EFillRepeat background_repeat_y = fill_layer.RepeatY();

  // Maintain both snapped and unsnapped available widths and heights.
  // Unsnapped values are used for most thing, but snapped are used
  // to computed sizes that must fill the area, such as round and space.
  LayoutUnit unsnapped_available_width =
      unsnapped_positioning_area.Width() - tile_size_.width;
  LayoutUnit unsnapped_available_height =
      unsnapped_positioning_area.Height() - tile_size_.height;
  LayoutUnit snapped_available_width =
      snapped_positioning_area.Width() - tile_size_.width;
  LayoutUnit snapped_available_height =
      snapped_positioning_area.Height() - tile_size_.height;
  PhysicalSize snapped_positioning_area_size = snapped_positioning_area.size;

  // Computed position is for placing things within the destination, so use
  // snapped values.
  LayoutUnit computed_x_position =
      MinimumValueForLength(fill_layer.PositionX(), snapped_available_width) -
      OffsetInBackground(fill_layer).left;
  LayoutUnit computed_y_position =
      MinimumValueForLength(fill_layer.PositionY(), snapped_available_height) -
      OffsetInBackground(fill_layer).top;

  if (background_repeat_x == EFillRepeat::kRoundFill &&
      snapped_positioning_area_size.width > LayoutUnit() &&
      tile_size_.width > LayoutUnit()) {
    LayoutUnit rounded_width = ComputeRoundedTileSize(
        snapped_positioning_area_size.width, tile_size_.width);
    // Maintain aspect ratio if background-size: auto is set
    if (fill_layer.SizeLength().Height().IsAuto() &&
        background_repeat_y != EFillRepeat::kRoundFill) {
      tile_size_.height =
          rounded_width.MulDiv(tile_size_.height, tile_size_.width);
    }
    tile_size_.width = rounded_width;

    // Force the first tile to line up with the edge of the positioning area.
    SetPhaseX(ComputeTilePhase(computed_x_position + unsnapped_box_offset.left,
                               tile_size_.width));
    SetSpaceSize(PhysicalSize());
  }

  if (background_repeat_y == EFillRepeat::kRoundFill &&
      snapped_positioning_area_size.height > LayoutUnit() &&
      tile_size_.height > LayoutUnit()) {
    LayoutUnit rounded_height = ComputeRoundedTileSize(
        snapped_positioning_area_size.height, tile_size_.height);
    // Maintain aspect ratio if background-size: auto is set
    if (fill_layer.SizeLength().Width().IsAuto() &&
        background_repeat_x != EFillRepeat::kRoundFill) {
      tile_size_.width =
          rounded_height.MulDiv(tile_size_.width, tile_size_.height);
    }
    tile_size_.height = rounded_height;

    // Force the first tile to line up with the edge of the positioning area.
    SetPhaseY(ComputeTilePhase(computed_y_position + unsnapped_box_offset.top,
                               tile_size_.height));
    SetSpaceSize(PhysicalSize());
  }

  if (background_repeat_x == EFillRepeat::kRepeatFill) {
    // Repeat must set the phase accurately, so use unsnapped values.
    SetRepeatX(fill_layer, unsnapped_available_width,
               unsnapped_box_offset.left);
  } else if (background_repeat_x == EFillRepeat::kSpaceFill &&
             tile_size_.width > LayoutUnit()) {
    // SpaceFill uses snapped values to fill the painted area.
    LayoutUnit space = GetSpaceBetweenImageTiles(
        snapped_positioning_area_size.width, tile_size_.width);
    if (space >= LayoutUnit())
      SetSpaceX(space, snapped_box_offset.left);
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
    SetNoRepeatX(fill_layer, unsnapped_box_offset.left + x_offset,
                 snapped_box_offset.left + snapped_x_offset);
  }

  if (background_repeat_y == EFillRepeat::kRepeatFill) {
    // Repeat must set the phase accurately, so use unsnapped values.
    SetRepeatY(fill_layer, unsnapped_available_height,
               unsnapped_box_offset.top);
  } else if (background_repeat_y == EFillRepeat::kSpaceFill &&
             tile_size_.height > LayoutUnit()) {
    // SpaceFill uses snapped values to fill the painted area.
    LayoutUnit space = GetSpaceBetweenImageTiles(
        snapped_positioning_area_size.height, tile_size_.height);
    if (space >= LayoutUnit())
      SetSpaceY(space, snapped_box_offset.top);
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
    SetNoRepeatY(fill_layer, unsnapped_box_offset.top + y_offset,
                 snapped_box_offset.top + snapped_y_offset);
  }

  if (ShouldUseFixedAttachment(fill_layer))
    UseFixedAttachment(paint_rect.offset);

  // Clip the final output rect to the paint rect.
  unsnapped_dest_rect_.Intersect(paint_rect);

  // Clip the snapped rect, and re-snap the dest rect as we may have
  // adjusted it with unsnapped values.
  snapped_dest_rect_.Intersect(paint_rect);
  snapped_dest_rect_ = PhysicalRect(ToPixelSnappedRect(snapped_dest_rect_));
}

const ImageResourceObserver& BackgroundImageGeometry::ImageClient() const {
  return *(painting_view_ ? box_ : positioning_box_);
}

const Document& BackgroundImageGeometry::ImageDocument() const {
  return box_->GetDocument();
}

const ComputedStyle& BackgroundImageGeometry::ImageStyle(
    const ComputedStyle& fragment_style) const {
  if (painting_view_ || cell_using_container_background_)
    return positioning_box_->StyleRef();
  return fragment_style;
}

InterpolationQuality BackgroundImageGeometry::ImageInterpolationQuality()
    const {
  return box_->StyleRef().GetInterpolationQuality();
}

PhysicalOffset BackgroundImageGeometry::OffsetInBackground(
    const FillLayer& fill_layer) const {
  if (ShouldUseFixedAttachment(fill_layer))
    return PhysicalOffset();
  return element_positioning_area_offset_;
}

PhysicalOffset BackgroundImageGeometry::ComputeDestPhase() const {
  // Given the size that the whole image should draw at, and the input phase
  // requested by the content, and the space between repeated tiles, compute a
  // phase that is no more than one size + space in magnitude.
  const PhysicalSize step_per_tile = tile_size_ + repeat_spacing_;
  const PhysicalOffset phase = {IntMod(-phase_.left, step_per_tile.width),
                                IntMod(-phase_.top, step_per_tile.height)};
  return snapped_dest_rect_.offset + phase;
}

}  // namespace blink
