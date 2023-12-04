// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/background_image_geometry.h"

#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_cell.h"
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

LayoutUnit ResolveWidthForRatio(LayoutUnit height,
                                const PhysicalSize& natural_ratio) {
  LayoutUnit resolved_width =
      height.MulDiv(natural_ratio.width, natural_ratio.height);
  if (natural_ratio.width >= 1 && resolved_width < 1) {
    return LayoutUnit(1);
  }
  return resolved_width;
}

LayoutUnit ResolveHeightForRatio(LayoutUnit width,
                                 const PhysicalSize& natural_ratio) {
  LayoutUnit resolved_height =
      width.MulDiv(natural_ratio.height, natural_ratio.width);
  if (natural_ratio.height >= 1 && resolved_height < 1) {
    return LayoutUnit(1);
  }
  return resolved_height;
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

bool BackgroundImageGeometry::ShouldUseFixedAttachment(
    const FillLayer& fill_layer) const {
  // Only backgrounds fixed to viewport should be treated as fixed attachment.
  // See comments in the private constructor.
  return has_background_fixed_to_viewport_ &&
         // Solid color background should use default attachment.
         fill_layer.GetImage() &&
         fill_layer.Attachment() == EFillAttachment::kFixed;
}

bool BackgroundImageGeometry::CanCompositeBackgroundAttachmentFixed() const {
  return !painting_view_ && has_background_fixed_to_viewport_ &&
         positioning_box_->CanCompositeBackgroundAttachmentFixed();
}

PhysicalRect BackgroundImageGeometry::FixedAttachmentPositioningArea(
    const PaintInfo& paint_info) const {
  const ScrollableArea* layout_viewport =
      box_->GetFrameView()->LayoutViewport();
  DCHECK(layout_viewport);
  PhysicalSize size(layout_viewport->VisibleContentRect().size());
  if (CanCompositeBackgroundAttachmentFixed()) {
    // The caller should have adjusted paint chunk properties to be in the
    // viewport space.
    return PhysicalRect(PhysicalOffset(), size);
  }
  gfx::PointF viewport_origin_in_local_space =
      GeometryMapper::SourceToDestinationProjection(
          box_->View()->FirstFragment().LocalBorderBoxProperties().Transform(),
          paint_info.context.GetPaintController()
              .CurrentPaintChunkProperties()
              .Transform())
          .MapPoint(gfx::PointF());
  return PhysicalRect(
      PhysicalOffset::FromPointFRound(viewport_origin_in_local_space),
      PhysicalSize(layout_viewport->VisibleContentRect().size()));
}

namespace {

// Computes the stitched table-grid rect relative to the current fragment.
PhysicalRect ComputeStitchedTableGridRect(const PhysicalBoxFragment& fragment) {
  const auto writing_direction = fragment.Style().GetWritingDirection();
  LogicalRect table_grid_rect;
  LogicalRect fragment_local_grid_rect;
  LayoutUnit stitched_block_size;

  for (const PhysicalBoxFragment& walker :
       To<LayoutBox>(fragment.GetLayoutObject())->PhysicalFragments()) {
    LogicalRect local_grid_rect = walker.TableGridRect();
    local_grid_rect.offset.block_offset += stitched_block_size;
    if (table_grid_rect.IsEmpty())
      table_grid_rect = local_grid_rect;
    else
      table_grid_rect.Unite(local_grid_rect);

    if (&walker == &fragment)
      fragment_local_grid_rect = local_grid_rect;

    stitched_block_size +=
        LogicalFragment(writing_direction, walker).BlockSize();
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
  has_background_fixed_to_viewport_ = view.IsBackgroundAttachmentFixedObject();
  painting_view_ = true;
  // The background of the box generated by the root element covers the
  // entire canvas and will be painted by the view object, but the we should
  // still use the root element box for positioning.
  positioning_size_override_ = view.RootBox().Size();
  // The background image should paint from the root element's coordinate space.
  element_positioning_area_offset_ = element_positioning_area_offset;
}

BackgroundImageGeometry::BackgroundImageGeometry(
    const LayoutBoxModelObject& obj)
    : BackgroundImageGeometry(&obj, &obj) {}

// TablesNG background painting.
BackgroundImageGeometry::BackgroundImageGeometry(const LayoutTableCell& cell,
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
    const PhysicalBoxFragment& fragment)
    : BackgroundImageGeometry(
          To<LayoutBoxModelObject>(fragment.GetLayoutObject()),
          To<LayoutBoxModelObject>(fragment.GetLayoutObject())) {
  DCHECK(box_->IsBox());

  if (fragment.IsTable()) {
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
    : box_(box),
      positioning_box_(positioning_box),
      has_background_fixed_to_viewport_(
          HasBackgroundFixedToViewport(*positioning_box)) {
  // Specialized constructor should be used for LayoutView.
  DCHECK(!IsA<LayoutView>(box));
  DCHECK(box);
  DCHECK(positioning_box);
}

PhysicalBoxStrut BackgroundImageGeometry::VisualOverflowOutsets() const {
  PhysicalRect border_box;
  if (positioning_box_->IsBox()) {
    border_box = To<LayoutBox>(positioning_box_)->PhysicalBorderBoxRect();
  } else {
    border_box = To<LayoutInline>(positioning_box_)->PhysicalLinesBoundingBox();
  }
  PhysicalRect visual_overflow =
      positioning_box_->Layer()
          ->LocalBoundingBoxIncludingSelfPaintingDescendants();
  return PhysicalBoxStrut(visual_overflow.Y() - border_box.Y(),
                          border_box.Right() - visual_overflow.Right(),
                          border_box.Bottom() - visual_overflow.Bottom(),
                          visual_overflow.X() - border_box.X());
}

PhysicalBoxStrut BackgroundImageGeometry::InnerBorderOutsets(
    const PhysicalRect& dest_rect,
    const PhysicalRect& positioning_area) const {
  gfx::RectF inner_border_rect =
      RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
          positioning_box_->StyleRef(), positioning_area)
          .Rect();
  PhysicalBoxStrut outset;
  // TODO(rendering-core) The LayoutUnit(float) constructor always rounds
  // down. We should FromFloatFloor or FromFloatCeil to move toward the border.
  outset.left = LayoutUnit(inner_border_rect.x()) - dest_rect.X();
  outset.top = LayoutUnit(inner_border_rect.y()) - dest_rect.Y();
  outset.right = dest_rect.Right() - LayoutUnit(inner_border_rect.right());
  outset.bottom = dest_rect.Bottom() - LayoutUnit(inner_border_rect.bottom());
  return outset;
}

SnappedAndUnsnappedOutsets BackgroundImageGeometry::ObscuredBorderOutsets(
    const PhysicalRect& dest_rect,
    const PhysicalRect& positioning_area) const {
  const ComputedStyle& style = positioning_box_->StyleRef();
  gfx::RectF inner_border_rect =
      RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(style,
                                                            positioning_area)
          .Rect();

  BorderEdge edges[4];
  style.GetBorderEdgeInfo(edges);
  const PhysicalBoxStrut box_outsets = positioning_box_->BorderOutsets();
  SnappedAndUnsnappedOutsets adjust;
  if (edges[static_cast<unsigned>(BoxSide::kTop)].ObscuresBackground()) {
    adjust.snapped.top = LayoutUnit(inner_border_rect.y()) - dest_rect.Y();
    adjust.unsnapped.top = box_outsets.top;
  }
  if (edges[static_cast<unsigned>(BoxSide::kRight)].ObscuresBackground()) {
    adjust.snapped.right =
        dest_rect.Right() - LayoutUnit(inner_border_rect.right());
    adjust.unsnapped.right = box_outsets.right;
  }
  if (edges[static_cast<unsigned>(BoxSide::kBottom)].ObscuresBackground()) {
    adjust.snapped.bottom =
        dest_rect.Bottom() - LayoutUnit(inner_border_rect.bottom());
    adjust.unsnapped.bottom = box_outsets.bottom;
  }
  if (edges[static_cast<unsigned>(BoxSide::kLeft)].ObscuresBackground()) {
    adjust.snapped.left = LayoutUnit(inner_border_rect.x()) - dest_rect.X();
    adjust.unsnapped.left = box_outsets.left;
  }
  return adjust;
}

bool BackgroundImageGeometry::HasBackgroundFixedToViewport(
    const LayoutBoxModelObject& object) {
  if (!object.IsBackgroundAttachmentFixedObject()) {
    return false;
  }
  // https://www.w3.org/TR/css-transforms-1/#transform-rendering
  // Fixed backgrounds on the root element are affected by any transform
  // specified for that element. For all other elements that are effected
  // by a transform, a value of fixed for the background-attachment property
  // is treated as if it had a value of scroll.
  for (const PaintLayer* layer = object.EnclosingLayer();
       layer && !layer->IsRootLayer(); layer = layer->Parent()) {
    // Check LayoutObject::HasTransformRelatedProperty() first to exclude
    // non-applicable transforms and will-change: transform.
    LayoutObject& ancestor = layer->GetLayoutObject();
    if (ancestor.HasTransformRelatedProperty() &&
        (layer->Transform() ||
         ancestor.StyleRef().HasWillChangeHintForAnyTransformProperty())) {
      return false;
    }
  }
  return true;
}

SnappedAndUnsnappedOutsets BackgroundImageGeometry::ComputeDestRectAdjustments(
    const FillLayer& fill_layer,
    const PhysicalRect& unsnapped_positioning_area,
    bool disallow_border_derived_adjustment) const {
  SnappedAndUnsnappedOutsets dest_adjust;
  switch (fill_layer.Clip()) {
    case EFillBox::kNoClip:
      dest_adjust.unsnapped = VisualOverflowOutsets();
      dest_adjust.snapped = dest_adjust.unsnapped;
      break;
    case EFillBox::kFillBox:
    // Spec: For elements with associated CSS layout box, the used values for
    // fill-box compute to content-box.
    // https://drafts.fxtf.org/css-masking/#the-mask-clip
    case EFillBox::kContent:
      // If the PaddingOutsets are zero then this is equivalent to
      // kPadding and we should apply the snapping logic.
      dest_adjust.unsnapped = positioning_box_->PaddingOutsets();
      if (!dest_adjust.unsnapped.IsZero()) {
        dest_adjust.unsnapped += positioning_box_->BorderOutsets();
        // We're not trying to match a border position, so don't snap.
        dest_adjust.snapped = dest_adjust.unsnapped;
        break;
      }
      [[fallthrough]];
    case EFillBox::kPadding:
      dest_adjust.unsnapped = positioning_box_->BorderOutsets();
      if (disallow_border_derived_adjustment) {
        // Nothing to drive snapping behavior, so don't snap.
        dest_adjust.snapped = dest_adjust.unsnapped;
      } else {
        // Force the snapped dest rect to match the inner border to
        // avoid gaps between the background and border.
        dest_adjust.snapped = InnerBorderOutsets(unsnapped_dest_rect_,
                                                 unsnapped_positioning_area);
      }
      break;
    case EFillBox::kStrokeBox:
    case EFillBox::kViewBox:
    // Spec: For elements with associated CSS layout box, ... stroke-box and
    // view-box compute to border-box.
    // https://drafts.fxtf.org/css-masking/#the-mask-clip
    case EFillBox::kBorder: {
      if (disallow_border_derived_adjustment) {
        // All adjustments remain 0.
        break;
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
      dest_adjust = ObscuredBorderOutsets(unsnapped_dest_rect_,
                                          unsnapped_positioning_area);
      break;
    }
    case EFillBox::kText:
      break;
  }
  return dest_adjust;
}

SnappedAndUnsnappedOutsets
BackgroundImageGeometry::ComputePositioningAreaAdjustments(
    const FillLayer& fill_layer,
    const PhysicalRect& unsnapped_positioning_area,
    bool disallow_border_derived_adjustment) const {
  SnappedAndUnsnappedOutsets box_outset;
  switch (fill_layer.Origin()) {
    case EFillBox::kFillBox:
    // Spec: For elements with associated CSS layout box, the used values for
    // fill-box compute to content-box.
    // https://drafts.fxtf.org/css-masking/#the-mask-clip
    case EFillBox::kContent:
      // If the PaddingOutsets are zero then this is equivalent to
      // kPadding and we should apply the snapping logic.
      box_outset.unsnapped = positioning_box_->PaddingOutsets();
      if (!box_outset.unsnapped.IsZero()) {
        box_outset.unsnapped += positioning_box_->BorderOutsets();
        // We're not trying to match a border position, so don't snap.
        box_outset.snapped = box_outset.unsnapped;
        break;
      }
      [[fallthrough]];
    case EFillBox::kPadding:
      box_outset.unsnapped = positioning_box_->BorderOutsets();
      if (disallow_border_derived_adjustment) {
        box_outset.snapped = box_outset.unsnapped;
      } else {
        // Force the snapped positioning area to fill to the borders.
        // Note that the snapped adjustments do not have the same effect as
        // pixel snapping the unsnapped rectangle. Border snapping snaps both
        // the size and position of the borders, sometimes adjusting the inner
        // border by more than a pixel when done (particularly under magnifying
        // zoom).
        box_outset.snapped = InnerBorderOutsets(unsnapped_positioning_area,
                                                unsnapped_positioning_area);
      }
      break;
    case EFillBox::kStrokeBox:
    case EFillBox::kViewBox:
    // Spec: For elements with associated CSS layout box, ... stroke-box and
    // view-box compute to border-box.
    // https://drafts.fxtf.org/css-masking/#the-mask-clip
    case EFillBox::kBorder:
      // All adjustments remain 0.
      break;
    case EFillBox::kNoClip:
    case EFillBox::kText:
      // These are not supported mask-origin values.
      NOTREACHED();
  }
  return box_outset;
}

PhysicalRect BackgroundImageGeometry::ComputePositioningArea(
    const PaintInfo& paint_info,
    const FillLayer& fill_layer,
    const PhysicalRect& paint_rect) const {
  if (ShouldUseFixedAttachment(fill_layer)) {
    return FixedAttachmentPositioningArea(paint_info);
  }
  if (painting_view_ || cell_using_container_background_ ||
      box_has_multiple_fragments_) {
    return {PhysicalOffset(), positioning_size_override_};
  }
  return paint_rect;
}

void BackgroundImageGeometry::AdjustPositioningArea(
    const PaintInfo& paint_info,
    const FillLayer& fill_layer,
    const PhysicalRect& paint_rect,
    PhysicalRect& unsnapped_positioning_area,
    PhysicalRect& snapped_positioning_area,
    PhysicalOffset& unsnapped_box_offset,
    PhysicalOffset& snapped_box_offset) {
  if (ShouldUseFixedAttachment(fill_layer)) {
    unsnapped_dest_rect_ = snapped_dest_rect_ = snapped_positioning_area =
        unsnapped_positioning_area;
  } else {
    unsnapped_dest_rect_ = paint_rect;

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
    const SnappedAndUnsnappedOutsets dest_adjust =
        ComputeDestRectAdjustments(fill_layer, unsnapped_positioning_area,
                                   disallow_border_derived_adjustment);
    const SnappedAndUnsnappedOutsets box_outset =
        ComputePositioningAreaAdjustments(fill_layer,
                                          unsnapped_positioning_area,
                                          disallow_border_derived_adjustment);

    // Offset of the positioning area from the corner of positioning_box_.
    unsnapped_box_offset =
        box_outset.unsnapped.Offset() - dest_adjust.unsnapped.Offset();
    snapped_box_offset =
        box_outset.snapped.Offset() - dest_adjust.snapped.Offset();

    // Apply the adjustments.
    snapped_dest_rect_ = unsnapped_dest_rect_;
    snapped_dest_rect_.Contract(dest_adjust.snapped);
    snapped_dest_rect_ = PhysicalRect(ToPixelSnappedRect(snapped_dest_rect_));
    snapped_dest_rect_.size.ClampNegativeToZero();
    unsnapped_dest_rect_.Contract(dest_adjust.unsnapped);
    unsnapped_dest_rect_.size.ClampNegativeToZero();
    snapped_positioning_area = unsnapped_positioning_area;
    snapped_positioning_area.Contract(box_outset.snapped);
    snapped_positioning_area =
        PhysicalRect(ToPixelSnappedRect(snapped_positioning_area));
    snapped_positioning_area.size.ClampNegativeToZero();
    unsnapped_positioning_area.Contract(box_outset.unsnapped);
    unsnapped_positioning_area.size.ClampNegativeToZero();
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
  IntrinsicSizingInfo sizing_info =
      image->GetNaturalSizingInfo(positioning_box_->StyleRef().EffectiveZoom(),
                                  box_->StyleRef().ImageOrientation());
  PhysicalSize image_aspect_ratio =
      PhysicalSize::FromSizeFFloor(sizing_info.aspect_ratio);
  PhysicalSize positioning_area_size = !image->HasIntrinsicSize()
                                           ? snapped_positioning_area_size
                                           : unsnapped_positioning_area_size;
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

      // An auto value for one dimension is resolved by using the image's
      // natural aspect ratio and the size of the other dimension, or failing
      // that, using the image's natural size, or failing that, treating it as
      // 100%.
      // If both values are auto then the natural width and/or height of the
      // image should be used, if any, the missing dimension (if any)
      // behaving as auto as described above. If the image has neither
      // natural size, its size is determined as for contain.
      if (layer_width.IsAuto() && !layer_height.IsAuto()) {
        if (!image_aspect_ratio.IsEmpty()) {
          tile_size_.width =
              ResolveWidthForRatio(tile_size_.height, image_aspect_ratio);
        } else if (sizing_info.has_width) {
          tile_size_.width =
              LayoutUnit::FromFloatFloor(sizing_info.size.width());
        } else {
          tile_size_.width = positioning_area_size.width;
        }
      } else if (!layer_width.IsAuto() && layer_height.IsAuto()) {
        if (!image_aspect_ratio.IsEmpty()) {
          tile_size_.height =
              ResolveHeightForRatio(tile_size_.width, image_aspect_ratio);
        } else if (sizing_info.has_height) {
          tile_size_.height =
              LayoutUnit::FromFloatFloor(sizing_info.size.height());
        } else {
          tile_size_.height = positioning_area_size.height;
        }
      } else if (layer_width.IsAuto() && layer_height.IsAuto()) {
        PhysicalSize concrete_image_size = PhysicalSize::FromSizeFFloor(
            image->ImageSize(positioning_box_->StyleRef().EffectiveZoom(),
                             gfx::SizeF(positioning_area_size),
                             box_->StyleRef().ImageOrientation()));
        tile_size_ = concrete_image_size;
      }

      tile_size_.ClampNegativeToZero();
      return;
    }
    case EFillSizeType::kContain:
    case EFillSizeType::kCover: {
      if (image_aspect_ratio.IsEmpty()) {
        tile_size_ = snapped_positioning_area_size;
        return;
      }
      // Always use the snapped positioning area size for this computation,
      // so that we resize the image to completely fill the actual painted
      // area.
      // Force the dimension that determines the size to exactly match the
      // positioning_area_size in that dimension.
      tile_size_ = snapped_positioning_area_size.FitToAspectRatio(
          image_aspect_ratio, type == EFillSizeType::kCover
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

void BackgroundImageGeometry::CalculateRepeatAndPosition(
    const FillLayer& fill_layer,
    const PhysicalSize& unsnapped_positioning_area_size,
    const PhysicalSize& snapped_positioning_area_size,
    const PhysicalOffset& unsnapped_box_offset,
    const PhysicalOffset& snapped_box_offset) {
  EFillRepeat background_repeat_x = fill_layer.Repeat().x;
  EFillRepeat background_repeat_y = fill_layer.Repeat().y;

  // Maintain both snapped and unsnapped available widths and heights.
  // Unsnapped values are used for most thing, but snapped are used
  // to computed sizes that must fill the area, such as round and space.
  const LayoutUnit unsnapped_available_width =
      unsnapped_positioning_area_size.width - tile_size_.width;
  const LayoutUnit unsnapped_available_height =
      unsnapped_positioning_area_size.height - tile_size_.height;
  const LayoutUnit snapped_available_width =
      snapped_positioning_area_size.width - tile_size_.width;
  const LayoutUnit snapped_available_height =
      snapped_positioning_area_size.height - tile_size_.height;

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
      tile_size_.height = ResolveHeightForRatio(rounded_width, tile_size_);
    }
    tile_size_.width = rounded_width;

    // Force the first tile to line up with the edge of the positioning area.
    const LayoutUnit x_offset =
        fill_layer.BackgroundXOrigin() == BackgroundEdgeOrigin::kRight
            ? snapped_available_width - computed_x_position
            : computed_x_position;
    SetPhaseX(ComputeTilePhase(x_offset + unsnapped_box_offset.left,
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
      tile_size_.width = ResolveWidthForRatio(rounded_height, tile_size_);
    }
    tile_size_.height = rounded_height;

    // Force the first tile to line up with the edge of the positioning area.
    const LayoutUnit y_offset =
        fill_layer.BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom
            ? snapped_available_height - computed_y_position
            : computed_y_position;
    SetPhaseY(ComputeTilePhase(y_offset + unsnapped_box_offset.top,
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
}

void BackgroundImageGeometry::Calculate(const PaintInfo& paint_info,
                                        const FillLayer& fill_layer,
                                        const PhysicalRect& paint_rect) {
  DCHECK_GE(box_->GetDocument().Lifecycle().GetState(),
            DocumentLifecycle::kPrePaintClean);

  // Unsnapped positioning area is used to derive quantities
  // that reference source image maps and define non-integer values, such
  // as phase and position.
  PhysicalRect unsnapped_positioning_area =
      ComputePositioningArea(paint_info, fill_layer, paint_rect);

  // Snapped positioning area is used for sizing images based on the
  // background area (like cover and contain), and for setting the repeat
  // spacing.
  PhysicalRect snapped_positioning_area;

  // Additional offset from the corner of the positioning_box_
  PhysicalOffset unsnapped_box_offset;
  PhysicalOffset snapped_box_offset;

  // This method also sets the destination rects.
  AdjustPositioningArea(paint_info, fill_layer, paint_rect,
                        unsnapped_positioning_area, snapped_positioning_area,
                        unsnapped_box_offset, snapped_box_offset);

  // Sets the tile_size_.
  CalculateFillTileSize(fill_layer, unsnapped_positioning_area.size,
                        snapped_positioning_area.size);

  // Applies *-repeat and *-position.
  CalculateRepeatAndPosition(fill_layer, unsnapped_positioning_area.size,
                             snapped_positioning_area.size,
                             unsnapped_box_offset, snapped_box_offset);

  if (ShouldUseFixedAttachment(fill_layer))
    UseFixedAttachment(paint_rect.offset);

  // The actual painting area can be bigger than the provided background
  // geometry (`paint_rect`) for `mask-clip: no-clip`, so avoid clipping.
  if (fill_layer.Clip() != EFillBox::kNoClip) {
    // Clip the final output rect to the paint rect.
    unsnapped_dest_rect_.Intersect(paint_rect);
    snapped_dest_rect_.Intersect(paint_rect);
  }
  // Re-snap the dest rect as we may have adjusted it with unsnapped values.
  snapped_dest_rect_ = PhysicalRect(ToPixelSnappedRect(snapped_dest_rect_));
}

const ImageResourceObserver& BackgroundImageGeometry::ImageClient() const {
  return *(painting_view_ ? box_ : positioning_box_);
}

const ComputedStyle& BackgroundImageGeometry::ImageStyle(
    const ComputedStyle& fragment_style) const {
  if (painting_view_ || cell_using_container_background_)
    return positioning_box_->StyleRef();
  return fragment_style;
}

PhysicalOffset BackgroundImageGeometry::OffsetInBackground(
    const FillLayer& fill_layer) const {
  if (ShouldUseFixedAttachment(fill_layer))
    return PhysicalOffset();
  return element_positioning_area_offset_;
}

PhysicalOffset BackgroundImageGeometry::ComputePhase() const {
  // Given the size that the whole image should draw at, and the input phase
  // requested by the content, and the space between repeated tiles, compute a
  // phase that is no more than one size + space in magnitude.
  const PhysicalSize step_per_tile = tile_size_ + repeat_spacing_;
  const PhysicalOffset phase = {IntMod(-phase_.left, step_per_tile.width),
                                IntMod(-phase_.top, step_per_tile.height)};
  return phase;
}

}  // namespace blink
