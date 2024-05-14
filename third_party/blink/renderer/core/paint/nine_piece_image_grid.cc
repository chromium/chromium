// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/nine_piece_image_grid.h"

#include "third_party/blink/renderer/core/layout/geometry/box_strut.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "ui/gfx/geometry/outsets.h"

namespace blink {

namespace {

LayoutUnit ComputeEdgeWidth(const BorderImageLength& border_slice,
                            int border_side,
                            float image_side,
                            int box_extent) {
  if (border_slice.IsNumber())
    return LayoutUnit(border_slice.Number() * border_side);
  if (border_slice.length().IsAuto())
    return LayoutUnit(image_side);
  return ValueForLength(border_slice.length(), LayoutUnit(box_extent));
}

float ComputeEdgeSlice(const Length& slice, float slice_scale, float maximum) {
  float resolved;
  // If the slice is a <number> (stored as a fixed Length), scale it by the
  // slice scale to get to the same space as the image.
  if (slice.IsFixed()) {
    resolved = slice.Value() * slice_scale;
  } else {
    DCHECK(slice.IsPercent());
    resolved = FloatValueForLength(slice, maximum);
  }
  resolved = std::min(maximum, resolved);
  // Round-trip via LayoutUnit to flush out any "excess" precision.
  return LayoutUnit::FromFloatRound(resolved).ToFloat();
}

// "Round" the edge widths, adhering to the following restrictions:
//
//  1) Perform rounding in the same way as for borders, thus preferring
//     symmetry.
//
//  2) If edges are abutting, then distribute the space (i.e the single pixel)
//     to the edge with the highest coverage - giving the starting edge
//     precedence if tied.
//
gfx::Outsets SnapEdgeWidths(const PhysicalBoxStrut& edge_widths,
                            const gfx::Size& snapped_box_size) {
  gfx::Outsets snapped;
  // Allow a small deviation when checking if the the edges are abutting.
  constexpr LayoutUnit kAbuttingEpsilon(LayoutUnit::Epsilon());
  if (snapped_box_size.width() - edge_widths.HorizontalSum() <=
      kAbuttingEpsilon) {
    snapped.set_left(edge_widths.left.Round());
    snapped.set_right(snapped_box_size.width() - snapped.left());
  } else {
    snapped.set_left(edge_widths.left.Floor());
    snapped.set_right(edge_widths.right.Floor());
  }
  DCHECK_LE(snapped.left() + snapped.right(), snapped_box_size.width());

  if (snapped_box_size.height() - edge_widths.VerticalSum() <=
      kAbuttingEpsilon) {
    snapped.set_top(edge_widths.top.Round());
    snapped.set_bottom(snapped_box_size.height() - snapped.top());
  } else {
    snapped.set_top(edge_widths.top.Floor());
    snapped.set_bottom(edge_widths.bottom.Floor());
  }
  DCHECK_LE(snapped.top() + snapped.bottom(), snapped_box_size.height());
  return snapped;
}

}  // namespace

NinePieceImageGrid::NinePieceImageGrid(const NinePieceImage& nine_piece_image,
                                       const gfx::SizeF& image_size,
                                       const gfx::Vector2dF& slice_scale,
                                       float zoom,
                                       const gfx::Rect& border_image_area,
                                       const gfx::Outsets& border_widths,
                                       PhysicalBoxSides sides_to_include)
    : border_image_area_(border_image_area),
      image_size_(image_size),
      horizontal_tile_rule_(nine_piece_image.HorizontalRule()),
      vertical_tile_rule_(nine_piece_image.VerticalRule()),
      zoom_(zoom),
      fill_(nine_piece_image.Fill()) {
  const LengthBox& image_slices = nine_piece_image.ImageSlices();
  top_.slice = ComputeEdgeSlice(image_slices.Top(), slice_scale.y(),
                                image_size.height());
  right_.slice = ComputeEdgeSlice(image_slices.Right(), slice_scale.x(),
                                  image_size.width());
  bottom_.slice = ComputeEdgeSlice(image_slices.Bottom(), slice_scale.y(),
                                   image_size.height());
  left_.slice = ComputeEdgeSlice(image_slices.Left(), slice_scale.x(),
                                 image_size.width());

  // |Edge::slice| is in image-local units (physical pixels for raster images),
  // but when using it to resolve 'auto' for border-image-widths we want it to
  // be in zoomed CSS pixels, so divide by |slice_scale| and multiply by zoom.
  const gfx::Vector2dF auto_slice_adjustment(zoom / slice_scale.x(),
                                             zoom / slice_scale.y());
  const BorderImageLengthBox& border_slices = nine_piece_image.BorderSlices();
  PhysicalBoxStrut resolved_widths;
  if (sides_to_include.top) {
    resolved_widths.top = ComputeEdgeWidth(
        border_slices.Top(), border_widths.top(),
        top_.slice * auto_slice_adjustment.y(), border_image_area.height());
  }
  if (sides_to_include.right) {
    resolved_widths.right = ComputeEdgeWidth(
        border_slices.Right(), border_widths.right(),
        right_.slice * auto_slice_adjustment.x(), border_image_area.width());
  }
  if (sides_to_include.bottom) {
    resolved_widths.bottom = ComputeEdgeWidth(
        border_slices.Bottom(), border_widths.bottom(),
        bottom_.slice * auto_slice_adjustment.y(), border_image_area.height());
  }
  if (sides_to_include.left) {
    resolved_widths.left = ComputeEdgeWidth(
        border_slices.Left(), border_widths.left(),
        left_.slice * auto_slice_adjustment.x(), border_image_area.width());
  }

  // The spec says: Given Lwidth as the width of the border image area, Lheight
  // as its height, and Wside as the border image width offset for the side, let
  // f = min(Lwidth/(Wleft+Wright), Lheight/(Wtop+Wbottom)). If f < 1, then all
  // W are reduced by multiplying them by f.
  const LayoutUnit border_side_width = resolved_widths.HorizontalSum();
  const LayoutUnit border_side_height = resolved_widths.VerticalSum();
  const float border_side_scale_factor = std::min(
      static_cast<float>(border_image_area.width()) / border_side_width,
      static_cast<float>(border_image_area.height()) / border_side_height);
  if (border_side_scale_factor < 1) {
    resolved_widths.top =
        LayoutUnit(resolved_widths.top * border_side_scale_factor);
    resolved_widths.right =
        LayoutUnit(resolved_widths.right * border_side_scale_factor);
    resolved_widths.bottom =
        LayoutUnit(resolved_widths.bottom * border_side_scale_factor);
    resolved_widths.left =
        LayoutUnit(resolved_widths.left * border_side_scale_factor);
  }

  const gfx::Outsets snapped_widths =
      SnapEdgeWidths(resolved_widths, border_image_area.size());

  top_.width = snapped_widths.top();
  right_.width = snapped_widths.right();
  bottom_.width = snapped_widths.bottom();
  left_.width = snapped_widths.left();
}

// Given a rectangle, construct a subrectangle using offset, width and height.
// Negative offsets are relative to the extent of the given rectangle.
static gfx::RectF Subrect(const gfx::RectF& rect,
                          float offset_x,
                          float offset_y,
                          float width,
                          float height) {
  float base_x = rect.x();
  if (offset_x < 0)
    base_x = rect.right();

  float base_y = rect.y();
  if (offset_y < 0)
    base_y = rect.bottom();

  return gfx::RectF(base_x + offset_x, base_y + offset_y, width, height);
}

static gfx::RectF Subrect(const gfx::Rect& rect,
                          float offset_x,
                          float offset_y,
                          float width,
                          float height) {
  return Subrect(gfx::RectF(rect), offset_x, offset_y, width, height);
}

static gfx::RectF Subrect(const gfx::SizeF& size,
                          float offset_x,
                          float offset_y,
                          float width,
                          float height) {
  return Subrect(gfx::RectF(size), offset_x, offset_y, width, height);
}

static inline void SetCornerPiece(
    NinePieceImageGrid::NinePieceDrawInfo& draw_info,
    bool is_drawable,
    const gfx::RectF& source,
    const gfx::RectF& destination) {
  draw_info.is_drawable = is_drawable;
  if (draw_info.is_drawable) {
    draw_info.source = source;
    draw_info.destination = destination;
  }
}

void NinePieceImageGrid::SetDrawInfoCorner(NinePieceDrawInfo& draw_info,
                                           NinePiece piece) const {
  switch (piece) {
    case kTopLeftPiece:
      SetCornerPiece(
          draw_info, top_.IsDrawable() && left_.IsDrawable(),
          Subrect(image_size_, 0, 0, left_.slice, top_.slice),
          Subrect(border_image_area_, 0, 0, left_.width, top_.width));
      break;
    case kBottomLeftPiece:
      SetCornerPiece(
          draw_info, bottom_.IsDrawable() && left_.IsDrawable(),
          Subrect(image_size_, 0, -bottom_.slice, left_.slice, bottom_.slice),
          Subrect(border_image_area_, 0, -bottom_.width, left_.width,
                  bottom_.width));
      break;
    case kTopRightPiece:
      SetCornerPiece(
          draw_info, top_.IsDrawable() && right_.IsDrawable(),
          Subrect(image_size_, -right_.slice, 0, right_.slice, top_.slice),
          Subrect(border_image_area_, -right_.width, 0, right_.width,
                  top_.width));
      break;
    case kBottomRightPiece:
      SetCornerPiece(draw_info, bottom_.IsDrawable() && right_.IsDrawable(),
                     Subrect(image_size_, -right_.slice, -bottom_.slice,
                             right_.slice, bottom_.slice),
                     Subrect(border_image_area_, -right_.width, -bottom_.width,
                             right_.width, bottom_.width));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

static inline void SetHorizontalEdge(
    NinePieceImageGrid::NinePieceDrawInfo& draw_info,
    const NinePieceImageGrid::Edge& edge,
    const gfx::RectF& source,
    const gfx::RectF& destination,
    ENinePieceImageRule tile_rule) {
  draw_info.is_drawable =
      edge.IsDrawable() && source.width() > 0 && destination.width() > 0;
  if (draw_info.is_drawable) {
    draw_info.source = source;
    draw_info.destination = destination;
    draw_info.tile_scale = gfx::Vector2dF(edge.Scale(), edge.Scale());
    draw_info.tile_rule = {tile_rule, kStretchImageRule};
  }
}

static inline void SetVerticalEdge(
    NinePieceImageGrid::NinePieceDrawInfo& draw_info,
    const NinePieceImageGrid::Edge& edge,
    const gfx::RectF& source,
    const gfx::RectF& destination,
    ENinePieceImageRule tile_rule) {
  draw_info.is_drawable =
      edge.IsDrawable() && source.height() > 0 && destination.height() > 0;
  if (draw_info.is_drawable) {
    draw_info.source = source;
    draw_info.destination = destination;
    draw_info.tile_scale = gfx::Vector2dF(edge.Scale(), edge.Scale());
    draw_info.tile_rule = {kStretchImageRule, tile_rule};
  }
}

void NinePieceImageGrid::SetDrawInfoEdge(NinePieceDrawInfo& draw_info,
                                         NinePiece piece) const {
  gfx::SizeF edge_source_size =
      image_size_ -
      gfx::SizeF(left_.slice + right_.slice, top_.slice + bottom_.slice);
  gfx::Size edge_destination_size =
      border_image_area_.size() -
      gfx::Size(left_.width + right_.width, top_.width + bottom_.width);

  switch (piece) {
    case kLeftPiece:
      SetVerticalEdge(draw_info, left_,
                      Subrect(image_size_, 0, top_.slice, left_.slice,
                              edge_source_size.height()),
                      Subrect(border_image_area_, 0, top_.width, left_.width,
                              edge_destination_size.height()),
                      vertical_tile_rule_);
      break;
    case kRightPiece:
      SetVerticalEdge(draw_info, right_,
                      Subrect(image_size_, -right_.slice, top_.slice,
                              right_.slice, edge_source_size.height()),
                      Subrect(border_image_area_, -right_.width, top_.width,
                              right_.width, edge_destination_size.height()),
                      vertical_tile_rule_);
      break;
    case kTopPiece:
      SetHorizontalEdge(draw_info, top_,
                        Subrect(image_size_, left_.slice, 0,
                                edge_source_size.width(), top_.slice),
                        Subrect(border_image_area_, left_.width, 0,
                                edge_destination_size.width(), top_.width),
                        horizontal_tile_rule_);
      break;
    case kBottomPiece:
      SetHorizontalEdge(draw_info, bottom_,
                        Subrect(image_size_, left_.slice, -bottom_.slice,
                                edge_source_size.width(), bottom_.slice),
                        Subrect(border_image_area_, left_.width, -bottom_.width,
                                edge_destination_size.width(), bottom_.width),
                        horizontal_tile_rule_);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void NinePieceImageGrid::SetDrawInfoMiddle(NinePieceDrawInfo& draw_info) const {
  gfx::SizeF source_size = image_size_ - gfx::SizeF(left_.slice + right_.slice,
                                                    top_.slice + bottom_.slice);
  gfx::Size destination_size =
      border_image_area_.size() -
      gfx::Size(left_.width + right_.width, top_.width + bottom_.width);

  draw_info.is_drawable =
      fill_ && !source_size.IsEmpty() && !destination_size.IsEmpty();
  if (!draw_info.is_drawable)
    return;

  draw_info.source = Subrect(image_size_, left_.slice, top_.slice,
                             source_size.width(), source_size.height());
  draw_info.destination =
      Subrect(border_image_area_, left_.width, top_.width,
              destination_size.width(), destination_size.height());

  gfx::Vector2dF middle_scale_factor(zoom_, zoom_);

  if (top_.IsDrawable())
    middle_scale_factor.set_x(top_.Scale());
  else if (bottom_.IsDrawable())
    middle_scale_factor.set_x(bottom_.Scale());

  if (left_.IsDrawable())
    middle_scale_factor.set_y(left_.Scale());
  else if (right_.IsDrawable())
    middle_scale_factor.set_y(right_.Scale());

  if (!source_size.IsEmpty()) {
    // For "stretch" rules, just override the scale factor and replace. We only
    // have to do this for the center tile, since sides don't even use the scale
    // factor unless they have a rule other than "stretch". The middle however
    // can have "stretch" specified in one axis but not the other, so we have to
    // correct the scale here.
    if (horizontal_tile_rule_ == kStretchImageRule) {
      middle_scale_factor.set_x(destination_size.width() / source_size.width());
    }
    if (vertical_tile_rule_ == kStretchImageRule) {
      middle_scale_factor.set_y(destination_size.height() /
                                source_size.height());
    }
  }

  draw_info.tile_scale = middle_scale_factor;
  draw_info.tile_rule = {horizontal_tile_rule_, vertical_tile_rule_};
}

NinePieceImageGrid::NinePieceDrawInfo NinePieceImageGrid::GetNinePieceDrawInfo(
    NinePiece piece) const {
  NinePieceDrawInfo draw_info;
  draw_info.is_corner_piece =
      piece == kTopLeftPiece || piece == kTopRightPiece ||
      piece == kBottomLeftPiece || piece == kBottomRightPiece;

  if (draw_info.is_corner_piece)
    SetDrawInfoCorner(draw_info, piece);
  else if (piece != kMiddlePiece)
    SetDrawInfoEdge(draw_info, piece);
  else
    SetDrawInfoMiddle(draw_info);

  return draw_info;
}

}  // namespace blink
