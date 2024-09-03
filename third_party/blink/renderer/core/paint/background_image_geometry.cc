// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/background_image_geometry.h"

#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/svg_background_paint_context.h"

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

LayoutUnit ResolveXPosition(const FillLayer& fill_layer,
                            LayoutUnit available_width,
                            LayoutUnit offset) {
  const LayoutUnit edge_relative_position =
      MinimumValueForLength(fill_layer.PositionX(), available_width);
  // Convert from edge-relative form to absolute.
  const LayoutUnit absolute_position =
      fill_layer.BackgroundXOrigin() == BackgroundEdgeOrigin::kRight
          ? available_width - edge_relative_position
          : edge_relative_position;
  return absolute_position - offset;
}

LayoutUnit ResolveYPosition(const FillLayer& fill_layer,
                            LayoutUnit available_height,
                            LayoutUnit offset) {
  const LayoutUnit edge_relative_position =
      MinimumValueForLength(fill_layer.PositionY(), available_height);
  // Convert from edge-relative form to absolute.
  const LayoutUnit absolute_position =
      fill_layer.BackgroundYOrigin() == BackgroundEdgeOrigin::kBottom
          ? available_height - edge_relative_position
          : edge_relative_position;
  return absolute_position - offset;
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

void BackgroundImageGeometry::SetRepeatX(LayoutUnit x_offset) {
  // All values are unsnapped to accurately set phase in the presence of
  // zoom and large values. That is, accurately render the
  // background-position value.
  SetPhaseX(ComputeTilePhase(x_offset, tile_size_.width));
  SetSpaceSize(PhysicalSize(LayoutUnit(), SpaceSize().height));
}

void BackgroundImageGeometry::SetRepeatY(LayoutUnit y_offset) {
  // All values are unsnapped to accurately set phase in the presence of
  // zoom and large values. That is, accurately render the
  // background-position value.
  SetPhaseY(ComputeTilePhase(y_offset, tile_size_.height));
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

SnappedAndUnsnappedOutsets BackgroundImageGeometry::ComputeDestRectAdjustments(
    const FillLayer& fill_layer,
    const BoxBackgroundPaintContext& paint_context,
    const PhysicalRect& unsnapped_positioning_area,
    bool disallow_border_derived_adjustment) const {
  SnappedAndUnsnappedOutsets dest_adjust;
  switch (paint_context.EffectiveClip(fill_layer)) {
    case EFillBox::kNoClip:
      dest_adjust.unsnapped = paint_context.VisualOverflowOutsets();
      dest_adjust.snapped = dest_adjust.unsnapped;
      break;
    case EFillBox::kFillBox:
    // Spec: For elements with associated CSS layout box, the used values for
    // fill-box compute to content-box.
    // https://drafts.fxtf.org/css-masking/#the-mask-clip
    case EFillBox::kContent:
      // If the PaddingOutsets are zero then this is equivalent to
      // kPadding and we should apply the snapping logic.
      dest_adjust.unsnapped = paint_context.PaddingOutsets();
      if (!dest_adjust.unsnapped.IsZero()) {
        dest_adjust.unsnapped += paint_context.BorderOutsets();
        // We're not trying to match a border position, so don't snap.
        dest_adjust.snapped = dest_adjust.unsnapped;
        break;
      }
      [[fallthrough]];
    case EFillBox::kPadding:
      dest_adjust.unsnapped = paint_context.BorderOutsets();
      if (disallow_border_derived_adjustment) {
        // Nothing to drive snapping behavior, so don't snap.
        dest_adjust.snapped = dest_adjust.unsnapped;
      } else {
        // Force the snapped dest rect to match the inner border to
        // avoid gaps between the background and border.
        dest_adjust.snapped = paint_context.InnerBorderOutsets(
            unsnapped_dest_rect_, unsnapped_positioning_area);
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
      dest_adjust = paint_context.ObscuredBorderOutsets(
          unsnapped_dest_rect_, unsnapped_positioning_area);
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
    const BoxBackgroundPaintContext& paint_context,
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
      box_outset.unsnapped = paint_context.PaddingOutsets();
      if (!box_outset.unsnapped.IsZero()) {
        box_outset.unsnapped += paint_context.BorderOutsets();
        // We're not trying to match a border position, so don't snap.
        box_outset.snapped = box_outset.unsnapped;
        break;
      }
      [[fallthrough]];
    case EFillBox::kPadding:
      box_outset.unsnapped = paint_context.BorderOutsets();
      if (disallow_border_derived_adjustment) {
        box_outset.snapped = box_outset.unsnapped;
      } else {
        // Force the snapped positioning area to fill to the borders.
        // Note that the snapped adjustments do not have the same effect as
        // pixel snapping the unsnapped rectangle. Border snapping snaps both
        // the size and position of the borders, sometimes adjusting the inner
        // border by more than a pixel when done (particularly under magnifying
        // zoom).
        box_outset.snapped = paint_context.InnerBorderOutsets(
            unsnapped_positioning_area, unsnapped_positioning_area);
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
      NOTREACHED_IN_MIGRATION();
  }
  return box_outset;
}

void BackgroundImageGeometry::AdjustPositioningArea(
    const FillLayer& fill_layer,
    const BoxBackgroundPaintContext& paint_context,
    const PaintInfo& paint_info,
    PhysicalRect& unsnapped_positioning_area,
    PhysicalRect& snapped_positioning_area,
    PhysicalOffset& unsnapped_box_offset,
    PhysicalOffset& snapped_box_offset) {
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
      paint_context.DisallowBorderDerivedAdjustment();

  // Compute all the outsets we need to apply to the rectangles. These
  // outsets also include the snapping behavior.
  const SnappedAndUnsnappedOutsets dest_adjust = ComputeDestRectAdjustments(
      fill_layer, paint_context, unsnapped_positioning_area,
      disallow_border_derived_adjustment);
  const SnappedAndUnsnappedOutsets box_outset =
      ComputePositioningAreaAdjustments(fill_layer, paint_context,
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

void BackgroundImageGeometry::CalculateFillTileSize(
    const FillLayer& fill_layer,
    const ComputedStyle& style,
    const PhysicalSize& unsnapped_positioning_area_size,
    const PhysicalSize& snapped_positioning_area_size) {
  StyleImage* image = fill_layer.GetImage();
  EFillSizeType type = fill_layer.SizeType();

  // Tile size is snapped for images without intrinsic dimensions (typically
  // generated content) and unsnapped for content that has intrinsic
  // dimensions. Once we choose here we stop tracking whether the tile size is
  // snapped or unsnapped.
  IntrinsicSizingInfo sizing_info = image->GetNaturalSizingInfo(
      style.EffectiveZoom(), style.ImageOrientation());
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
      } else if (layer_width.IsPercent() || layer_width.IsCalculated()) {
        tile_size_.width =
            ValueForLength(layer_width, positioning_area_size.width);
      }

      if (layer_height.IsFixed()) {
        tile_size_.height = LayoutUnit(layer_height.Value());
      } else if (layer_height.IsPercent() || layer_height.IsCalculated()) {
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
        PhysicalSize concrete_image_size =
            PhysicalSize::FromSizeFFloor(image->ImageSize(
                style.EffectiveZoom(), gfx::SizeF(positioning_area_size),
                style.ImageOrientation()));
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
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED_IN_MIGRATION();
  return;
}

void BackgroundImageGeometry::CalculateRepeatAndPosition(
    const FillLayer& fill_layer,
    const PhysicalOffset& offset_in_background,
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
    const LayoutUnit x_offset = ResolveXPosition(
        fill_layer, snapped_available_width, offset_in_background.left);
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
    const LayoutUnit y_offset = ResolveYPosition(
        fill_layer, snapped_available_height, offset_in_background.top);
    SetPhaseY(ComputeTilePhase(y_offset + unsnapped_box_offset.top,
                               tile_size_.height));
    SetSpaceSize(PhysicalSize());
  }

  if (background_repeat_x == EFillRepeat::kRepeatFill) {
    // Repeat must set the phase accurately, so use unsnapped values.
    // Recompute computed position because here we need to resolve against
    // unsnapped widths to correctly set the phase.
    const LayoutUnit x_offset = ResolveXPosition(
        fill_layer, unsnapped_available_width, offset_in_background.left);
    SetRepeatX(unsnapped_box_offset.left + x_offset);
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
    const LayoutUnit x_offset = ResolveXPosition(
        fill_layer, unsnapped_available_width, offset_in_background.left);
    const LayoutUnit snapped_x_offset = ResolveXPosition(
        fill_layer, snapped_available_width, offset_in_background.left);
    SetNoRepeatX(fill_layer, unsnapped_box_offset.left + x_offset,
                 snapped_box_offset.left + snapped_x_offset);
  }

  if (background_repeat_y == EFillRepeat::kRepeatFill) {
    // Repeat must set the phase accurately, so use unsnapped values.
    // Recompute computed position because here we need to resolve against
    // unsnapped widths to correctly set the phase.
    const LayoutUnit y_offset = ResolveYPosition(
        fill_layer, unsnapped_available_height, offset_in_background.top);
    SetRepeatY(unsnapped_box_offset.top + y_offset);
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
    const LayoutUnit y_offset = ResolveYPosition(
        fill_layer, unsnapped_available_height, offset_in_background.top);
    const LayoutUnit snapped_y_offset = ResolveYPosition(
        fill_layer, snapped_available_height, offset_in_background.top);
    SetNoRepeatY(fill_layer, unsnapped_box_offset.top + y_offset,
                 snapped_box_offset.top + snapped_y_offset);
  }
}

void BackgroundImageGeometry::Calculate(
    const FillLayer& fill_layer,
    const BoxBackgroundPaintContext& paint_context,
    const PhysicalRect& paint_rect,
    const PaintInfo& paint_info) {
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

  if (paint_context.ShouldUseFixedAttachment(fill_layer)) {
    unsnapped_positioning_area =
        paint_context.FixedAttachmentPositioningArea(paint_info);
    unsnapped_dest_rect_ = snapped_dest_rect_ = snapped_positioning_area =
        unsnapped_positioning_area;
  } else {
    unsnapped_positioning_area =
        paint_context.NormalPositioningArea(paint_rect);
    unsnapped_dest_rect_ = paint_rect;

    // This method adjusts `unsnapped_dest_rect_` and sets
    // `snapped_dest_rect_`.
    AdjustPositioningArea(fill_layer, paint_context, paint_info,
                          unsnapped_positioning_area, snapped_positioning_area,
                          unsnapped_box_offset, snapped_box_offset);
  }

  // Sets the tile_size_.
  CalculateFillTileSize(fill_layer, paint_context.Style(),
                        unsnapped_positioning_area.size,
                        snapped_positioning_area.size);

  // Applies *-repeat and *-position.
  const PhysicalOffset offset_in_background =
      paint_context.OffsetInBackground(fill_layer);
  CalculateRepeatAndPosition(
      fill_layer, offset_in_background, unsnapped_positioning_area.size,
      snapped_positioning_area.size, unsnapped_box_offset, snapped_box_offset);

  if (paint_context.ShouldUseFixedAttachment(fill_layer)) {
    PhysicalOffset fixed_adjustment =
        paint_rect.offset - unsnapped_dest_rect_.offset;
    fixed_adjustment.ClampNegativeToZero();
    phase_ += fixed_adjustment;
  }

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

gfx::RectF BackgroundImageGeometry::ComputePositioningArea(
    const FillLayer& layer,
    const SVGBackgroundPaintContext& paint_context) const {
  switch (layer.Origin()) {
    case EFillBox::kNoClip:
    case EFillBox::kText:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case EFillBox::kBorder:
    case EFillBox::kContent:
    case EFillBox::kFillBox:
    case EFillBox::kPadding:
      return paint_context.ReferenceBox(GeometryBox::kFillBox);
    case EFillBox::kStrokeBox:
      return paint_context.ReferenceBox(GeometryBox::kStrokeBox);
    case EFillBox::kViewBox:
      return paint_context.ReferenceBox(GeometryBox::kViewBox);
  }
}

gfx::RectF BackgroundImageGeometry::ComputePaintingArea(
    const FillLayer& layer,
    const SVGBackgroundPaintContext& paint_context,
    const gfx::RectF& positioning_area) const {
  switch (layer.Clip()) {
    case EFillBox::kText:
    case EFillBox::kNoClip:
      return paint_context.VisualOverflowRect();
    case EFillBox::kContent:
    case EFillBox::kFillBox:
    case EFillBox::kPadding:
      return positioning_area;
    case EFillBox::kStrokeBox:
    case EFillBox::kBorder:
      return paint_context.ReferenceBox(GeometryBox::kStrokeBox);
    case EFillBox::kViewBox:
      return paint_context.ReferenceBox(GeometryBox::kViewBox);
  }
}

void BackgroundImageGeometry::Calculate(
    const FillLayer& fill_layer,
    const SVGBackgroundPaintContext& paint_context) {
  const gfx::RectF positioning_area =
      ComputePositioningArea(fill_layer, paint_context);
  const gfx::RectF painting_area =
      ComputePaintingArea(fill_layer, paint_context, positioning_area);
  // Unsnapped positioning area is used to derive quantities
  // that reference source image maps and define non-integer values, such
  // as phase and position.
  PhysicalRect unsnapped_positioning_area =
      PhysicalRect::EnclosingRect(positioning_area);
  unsnapped_dest_rect_ = PhysicalRect::EnclosingRect(painting_area);

  // Additional offset from the corner of the positioning_box_
  PhysicalOffset unsnapped_box_offset =
      unsnapped_positioning_area.offset - unsnapped_dest_rect_.offset;

  snapped_dest_rect_ = unsnapped_dest_rect_;

  // Sets the tile_size_.
  CalculateFillTileSize(fill_layer, paint_context.Style(),
                        unsnapped_positioning_area.size,
                        unsnapped_positioning_area.size);

  // Applies *-repeat and *-position.
  CalculateRepeatAndPosition(fill_layer, PhysicalOffset(),
                             unsnapped_positioning_area.size,
                             unsnapped_positioning_area.size,
                             unsnapped_box_offset, unsnapped_box_offset);
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
