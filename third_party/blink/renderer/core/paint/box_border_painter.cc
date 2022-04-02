// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_border_painter.h"

#include <algorithm>

#include "third_party/blink/renderer/core/paint/box_painter.h"
#include "third_party/blink/renderer/core/paint/object_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

enum BorderEdgeFlag {
  kTopBorderEdge = 1 << static_cast<unsigned>(BoxSide::kTop),
  kRightBorderEdge = 1 << static_cast<unsigned>(BoxSide::kRight),
  kBottomBorderEdge = 1 << static_cast<unsigned>(BoxSide::kBottom),
  kLeftBorderEdge = 1 << static_cast<unsigned>(BoxSide::kLeft),
  kAllBorderEdges =
      kTopBorderEdge | kBottomBorderEdge | kLeftBorderEdge | kRightBorderEdge
};

inline BorderEdgeFlag EdgeFlagForSide(BoxSide side) {
  return static_cast<BorderEdgeFlag>(1 << static_cast<unsigned>(side));
}

inline bool IncludesEdge(BorderEdgeFlags flags, BoxSide side) {
  return flags & EdgeFlagForSide(side);
}

inline bool IncludesAdjacentEdges(BorderEdgeFlags flags) {
  // The set includes adjacent edges iff it contains at least one horizontal and
  // one vertical edge.
  return (flags & (kTopBorderEdge | kBottomBorderEdge)) &&
         (flags & (kLeftBorderEdge | kRightBorderEdge));
}

inline bool StyleRequiresClipPolygon(EBorderStyle style) {
  // These are drawn with a stroke, so we have to clip to get corner miters.
  return style == EBorderStyle::kDotted || style == EBorderStyle::kDashed;
}

inline bool BorderStyleFillsBorderArea(EBorderStyle style) {
  return !(style == EBorderStyle::kDotted || style == EBorderStyle::kDashed ||
           style == EBorderStyle::kDouble);
}

inline bool BorderStyleHasInnerDetail(EBorderStyle style) {
  return style == EBorderStyle::kGroove || style == EBorderStyle::kRidge ||
         style == EBorderStyle::kDouble;
}

inline bool BorderStyleIsDottedOrDashed(EBorderStyle style) {
  return style == EBorderStyle::kDotted || style == EBorderStyle::kDashed;
}

// BorderStyleOutset darkens the bottom and right (and maybe lightens the top
// and left) BorderStyleInset darkens the top and left (and maybe lightens the
// bottom and right).
inline bool BorderStyleHasUnmatchedColorsAtCorner(EBorderStyle style,
                                                  BoxSide side,
                                                  BoxSide adjacent_side) {
  // These styles match at the top/left and bottom/right.
  if (style == EBorderStyle::kInset || style == EBorderStyle::kGroove ||
      style == EBorderStyle::kRidge || style == EBorderStyle::kOutset) {
    const BorderEdgeFlags top_right_flags =
        EdgeFlagForSide(BoxSide::kTop) | EdgeFlagForSide(BoxSide::kRight);
    const BorderEdgeFlags bottom_left_flags =
        EdgeFlagForSide(BoxSide::kBottom) | EdgeFlagForSide(BoxSide::kLeft);

    BorderEdgeFlags flags =
        EdgeFlagForSide(side) | EdgeFlagForSide(adjacent_side);
    return flags == top_right_flags || flags == bottom_left_flags;
  }
  return false;
}

inline bool BorderWillArcInnerEdge(const gfx::SizeF& first_radius,
                                   const gfx::SizeF& second_radius) {
  return !first_radius.IsZero() || !second_radius.IsZero();
}

inline bool WillOverdraw(BoxSide side,
                         EBorderStyle style,
                         BorderEdgeFlags completed_edges) {
  // If we're done with this side, it will obviously not overdraw any portion of
  // the current edge.
  if (IncludesEdge(completed_edges, side))
    return false;

  // The side is still to be drawn. It overdraws the current edge iff it has a
  // solid fill style.
  return BorderStyleFillsBorderArea(style);
}

inline bool BorderStylesRequireMiter(BoxSide side,
                                     BoxSide adjacent_side,
                                     EBorderStyle style,
                                     EBorderStyle adjacent_style) {
  if (style == EBorderStyle::kDouble ||
      adjacent_style == EBorderStyle::kDouble ||
      adjacent_style == EBorderStyle::kGroove ||
      adjacent_style == EBorderStyle::kRidge)
    return true;

  if (BorderStyleIsDottedOrDashed(style) !=
      BorderStyleIsDottedOrDashed(adjacent_style))
    return true;

  if (style != adjacent_style)
    return true;

  return BorderStyleHasUnmatchedColorsAtCorner(style, side, adjacent_side);
}

void SetToRightSideRect(gfx::RectF& rect, float edge_width) {
  rect.set_x(rect.right() - edge_width);
  rect.set_width(edge_width);
}

void SetToBottomSideRect(gfx::RectF& rect, float edge_width) {
  rect.set_y(rect.bottom() - edge_width);
  rect.set_height(edge_width);
}

gfx::RectF CalculateSideRect(const FloatRoundedRect& outer_border,
                             const BorderEdge& edge,
                             BoxSide side) {
  gfx::RectF side_rect = outer_border.Rect();
  float width = edge.Width();

  switch (side) {
    case BoxSide::kTop:
      side_rect.set_height(width);
      break;
    case BoxSide::kBottom:
      SetToBottomSideRect(side_rect, width);
      break;
    case BoxSide::kLeft:
      side_rect.set_width(width);
      break;
    case BoxSide::kRight:
      SetToRightSideRect(side_rect, width);
      break;
  }
  return side_rect;
}

FloatRoundedRect CalculateAdjustedInnerBorder(
    const FloatRoundedRect& inner_border,
    BoxSide side) {
  // Expand the inner border as necessary to make it a rounded rect (i.e. radii
  // contained within each edge).  This function relies on the fact we only get
  // radii not contained within each edge if one of the radii for an edge is
  // zero, so we can shift the arc towards the zero radius corner.
  FloatRoundedRect::Radii new_radii = inner_border.GetRadii();
  gfx::RectF new_rect = inner_border.Rect();

  float overshoot;
  float max_radii;

  switch (side) {
    case BoxSide::kTop:
      overshoot = new_radii.TopLeft().width() + new_radii.TopRight().width() -
                  new_rect.width();
      // FIXME: once we start pixel-snapping rounded rects after this point, the
      // overshoot concept should disappear.
      if (overshoot > 0.1) {
        new_rect.set_width(new_rect.width() + overshoot);
        if (!new_radii.TopLeft().width())
          new_rect.Offset(-overshoot, 0);
      }
      new_radii.SetBottomLeft(gfx::SizeF(0, 0));
      new_radii.SetBottomRight(gfx::SizeF(0, 0));
      max_radii =
          std::max(new_radii.TopLeft().height(), new_radii.TopRight().height());
      if (max_radii > new_rect.height())
        new_rect.set_height(max_radii);
      break;

    case BoxSide::kBottom:
      overshoot = new_radii.BottomLeft().width() +
                  new_radii.BottomRight().width() - new_rect.width();
      if (overshoot > 0.1) {
        new_rect.set_width(new_rect.width() + overshoot);
        if (!new_radii.BottomLeft().width())
          new_rect.Offset(-overshoot, 0);
      }
      new_radii.SetTopLeft(gfx::SizeF(0, 0));
      new_radii.SetTopRight(gfx::SizeF(0, 0));
      max_radii = std::max(new_radii.BottomLeft().height(),
                           new_radii.BottomRight().height());
      if (max_radii > new_rect.height()) {
        new_rect.Offset(0, new_rect.height() - max_radii);
        new_rect.set_height(max_radii);
      }
      break;

    case BoxSide::kLeft:
      overshoot = new_radii.TopLeft().height() +
                  new_radii.BottomLeft().height() - new_rect.height();
      if (overshoot > 0.1) {
        new_rect.set_height(new_rect.height() + overshoot);
        if (!new_radii.TopLeft().height())
          new_rect.Offset(0, -overshoot);
      }
      new_radii.SetTopRight(gfx::SizeF(0, 0));
      new_radii.SetBottomRight(gfx::SizeF(0, 0));
      max_radii =
          std::max(new_radii.TopLeft().width(), new_radii.BottomLeft().width());
      if (max_radii > new_rect.width())
        new_rect.set_width(max_radii);
      break;

    case BoxSide::kRight:
      overshoot = new_radii.TopRight().height() +
                  new_radii.BottomRight().height() - new_rect.height();
      if (overshoot > 0.1) {
        new_rect.set_height(new_rect.height() + overshoot);
        if (!new_radii.TopRight().height())
          new_rect.Offset(0, -overshoot);
      }
      new_radii.SetTopLeft(gfx::SizeF(0, 0));
      new_radii.SetBottomLeft(gfx::SizeF(0, 0));
      max_radii = std::max(new_radii.TopRight().width(),
                           new_radii.BottomRight().width());
      if (max_radii > new_rect.width()) {
        new_rect.Offset(new_rect.width() - max_radii, 0);
        new_rect.set_width(max_radii);
      }
      break;
  }

  return FloatRoundedRect(new_rect, new_radii);
}

void DrawSolidBorderRect(GraphicsContext& context,
                         const gfx::RectF& border_rect,
                         float border_width,
                         const Color& color,
                         const AutoDarkMode& auto_dark_mode) {
  gfx::RectF stroke_rect = border_rect;
  border_width = floorf(border_width);
  stroke_rect.Outset(-border_width / 2);

  bool was_antialias = context.ShouldAntialias();
  if (!was_antialias)
    context.SetShouldAntialias(true);

  context.SetStrokeStyle(kSolidStroke);
  context.SetStrokeColor(color);
  context.StrokeRect(stroke_rect, border_width, auto_dark_mode);

  if (!was_antialias)
    context.SetShouldAntialias(false);
}

void DrawBleedAdjustedDRRect(GraphicsContext& context,
                             BackgroundBleedAvoidance bleed_avoidance,
                             const FloatRoundedRect& outer,
                             const FloatRoundedRect& inner,
                             Color color,
                             const AutoDarkMode& auto_dark_mode) {
  switch (bleed_avoidance) {
    case kBackgroundBleedClipLayer: {
      // BackgroundBleedClipLayer clips the outer rrect for the whole layer.
      // Based on this, we can avoid background bleeding by filling the
      // *outside* of inner rrect, all the way to the layer bounds (enclosing
      // int rect for the clip, in device space).
      SkPath path;
      path.addRRect(SkRRect(inner));
      path.setFillType(SkPathFillType::kInverseWinding);

      cc::PaintFlags flags;
      flags.setColor(color.Rgb());
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setAntiAlias(true);
      context.DrawPath(path, flags, auto_dark_mode);

      break;
    }
    case kBackgroundBleedClipOnly:
      if (outer.IsRounded()) {
        // BackgroundBleedClipOnly clips the outer rrect corners for us.
        FloatRoundedRect adjusted_outer = outer;
        adjusted_outer.SetRadii(FloatRoundedRect::Radii());
        context.FillDRRect(adjusted_outer, inner, color, auto_dark_mode);
        break;
      }
      [[fallthrough]];
    default:
      context.FillDRRect(outer, inner, color, auto_dark_mode);
      break;
  }
}

// The LUTs below assume specific enum values.
static_assert(EBorderStyle::kNone == static_cast<EBorderStyle>(0),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kHidden == static_cast<EBorderStyle>(1),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kInset == static_cast<EBorderStyle>(2),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kGroove == static_cast<EBorderStyle>(3),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kOutset == static_cast<EBorderStyle>(4),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kRidge == static_cast<EBorderStyle>(5),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kDotted == static_cast<EBorderStyle>(6),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kDashed == static_cast<EBorderStyle>(7),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kSolid == static_cast<EBorderStyle>(8),
              "unexpected EBorderStyle value");
static_assert(EBorderStyle::kDouble == static_cast<EBorderStyle>(9),
              "unexpected EBorderStyle value");

static_assert(static_cast<unsigned>(BoxSide::kTop) == 0,
              "unexpected BoxSide value");
static_assert(static_cast<unsigned>(BoxSide::kRight) == 1,
              "unexpected BoxSide value");
static_assert(static_cast<unsigned>(BoxSide::kBottom) == 2,
              "unexpected BoxSide value");
static_assert(static_cast<unsigned>(BoxSide::kLeft) == 3,
              "unexpected BoxSide value");

// Style-based paint order: non-solid edges (dashed/dotted/double) are painted
// before solid edges (inset/outset/groove/ridge/solid) to maximize overdraw
// opportunities.
const unsigned kStylePriority[] = {
    0,  // EBorderStyle::kNone
    0,  // EBorderStyle::kHidden
    2,  // EBorderStyle::kInset
    2,  // EBorderStyle::kGroove
    2,  // EBorderStyle::kOutset
    2,  // EBorderStyle::kRidge,
    1,  // EBorderStyle::kDotted
    1,  // EBorderStyle::kDashed
    3,  // EBorderStyle::kSolid
    1,  // EBorderStyle::kDouble
};

// Given the same style, prefer drawing in non-adjacent order to minimize the
// number of sides which require miters.
const unsigned kSidePriority[] = {
    0,  // BoxSide::kTop
    2,  // BoxSide::kRight
    1,  // BoxSide::kBottom
    3,  // BoxSide::kLeft
};

// Edges sharing the same opacity. Stores both a side list and an edge bitfield
// to support constant time iteration + membership tests.
struct OpacityGroup {
  OpacityGroup(unsigned alpha) : edge_flags(0), alpha(alpha) {}

  Vector<BoxSide, 4> sides;
  BorderEdgeFlags edge_flags;
  unsigned alpha;
};

void ClipQuad(GraphicsContext& context,
              const gfx::PointF quad[],
              bool antialiased) {
  SkPathBuilder path;
  path.moveTo(gfx::PointFToSkPoint(quad[0]));
  path.lineTo(gfx::PointFToSkPoint(quad[1]));
  path.lineTo(gfx::PointFToSkPoint(quad[2]));
  path.lineTo(gfx::PointFToSkPoint(quad[3]));

  context.ClipPath(path.detach(), antialiased ? kAntiAliased : kNotAntiAliased);
}

void DrawDashedOrDottedBoxSide(GraphicsContext& context,
                               int x1,
                               int y1,
                               int x2,
                               int y2,
                               BoxSide side,
                               Color color,
                               int thickness,
                               EBorderStyle style,
                               bool antialias,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK_GT(thickness, 0);

  GraphicsContextStateSaver state_saver(context);
  context.SetShouldAntialias(antialias);
  context.SetStrokeColor(color);
  context.SetStrokeThickness(thickness);
  context.SetStrokeStyle(style == EBorderStyle::kDashed ? kDashedStroke
                                                        : kDottedStroke);

  switch (side) {
    case BoxSide::kBottom:
    case BoxSide::kTop: {
      int mid_y = y1 + thickness / 2;
      context.DrawLine(gfx::Point(x1, mid_y), gfx::Point(x2, mid_y),
                       auto_dark_mode);
      break;
    }
    case BoxSide::kRight:
    case BoxSide::kLeft: {
      int mid_x = x1 + thickness / 2;
      context.DrawLine(gfx::Point(mid_x, y1), gfx::Point(mid_x, y2),
                       auto_dark_mode);
      break;
    }
  }
}

void DrawDoubleBoxSide(GraphicsContext& context,
                       int x1,
                       int y1,
                       int x2,
                       int y2,
                       int length,
                       BoxSide side,
                       Color color,
                       float thickness,
                       int adjacent_width1,
                       int adjacent_width2,
                       bool antialias,
                       const AutoDarkMode& auto_dark_mode) {
  int third_of_thickness = (thickness + 1) / 3;
  DCHECK_GT(third_of_thickness, 0);

  if (!adjacent_width1 && !adjacent_width2) {
    StrokeStyle old_stroke_style = context.GetStrokeStyle();
    context.SetStrokeStyle(kNoStroke);
    context.SetFillColor(color);

    bool was_antialiased = context.ShouldAntialias();
    context.SetShouldAntialias(antialias);

    switch (side) {
      case BoxSide::kTop:
      case BoxSide::kBottom:
        context.DrawRect(gfx::Rect(x1, y1, length, third_of_thickness),
                         auto_dark_mode);
        context.DrawRect(
            gfx::Rect(x1, y2 - third_of_thickness, length, third_of_thickness),
            auto_dark_mode);
        break;
      case BoxSide::kLeft:
      case BoxSide::kRight:
        context.DrawRect(gfx::Rect(x1, y1, third_of_thickness, length),
                         auto_dark_mode);
        context.DrawRect(
            gfx::Rect(x2 - third_of_thickness, y1, third_of_thickness, length),
            auto_dark_mode);
        break;
    }

    context.SetShouldAntialias(was_antialiased);
    context.SetStrokeStyle(old_stroke_style);
    return;
  }

  int adjacent1_big_third =
      ((adjacent_width1 > 0) ? adjacent_width1 + 1 : adjacent_width1 - 1) / 3;
  int adjacent2_big_third =
      ((adjacent_width2 > 0) ? adjacent_width2 + 1 : adjacent_width2 - 1) / 3;

  switch (side) {
    case BoxSide::kTop:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0), y1,
          x2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0),
          y1 + third_of_thickness, side, color, EBorderStyle::kSolid,
          adjacent1_big_third, adjacent2_big_third, antialias, auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max((adjacent_width1 * 2 + 1) / 3, 0),
          y2 - third_of_thickness,
          x2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), y2, side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias, auto_dark_mode);
      break;
    case BoxSide::kLeft:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1, y1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
          x1 + third_of_thickness,
          y2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias, auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, x2 - third_of_thickness,
          y1 + std::max((adjacent_width1 * 2 + 1) / 3, 0), x2,
          y2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias, auto_dark_mode);
      break;
    case BoxSide::kBottom:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max((adjacent_width1 * 2 + 1) / 3, 0), y1,
          x2 - std::max((adjacent_width2 * 2 + 1) / 3, 0),
          y1 + third_of_thickness, side, color, EBorderStyle::kSolid,
          adjacent1_big_third, adjacent2_big_third, antialias, auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
          y2 - third_of_thickness,
          x2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), y2, side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias, auto_dark_mode);
      break;
    case BoxSide::kRight:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1, y1 + std::max((adjacent_width1 * 2 + 1) / 3, 0),
          x1 + third_of_thickness,
          y2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias, auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, x2 - third_of_thickness,
          y1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0), x2,
          y2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias, auto_dark_mode);
      break;
    default:
      break;
  }
}

void DrawRidgeOrGrooveBoxSide(GraphicsContext& context,
                              int x1,
                              int y1,
                              int x2,
                              int y2,
                              BoxSide side,
                              Color color,
                              EBorderStyle style,
                              int adjacent_width1,
                              int adjacent_width2,
                              bool antialias,
                              const AutoDarkMode& auto_dark_mode) {
  EBorderStyle s1;
  EBorderStyle s2;
  if (style == EBorderStyle::kGroove) {
    s1 = EBorderStyle::kInset;
    s2 = EBorderStyle::kOutset;
  } else {
    s1 = EBorderStyle::kOutset;
    s2 = EBorderStyle::kInset;
  }

  int adjacent1_big_half =
      ((adjacent_width1 > 0) ? adjacent_width1 + 1 : adjacent_width1 - 1) / 2;
  int adjacent2_big_half =
      ((adjacent_width2 > 0) ? adjacent_width2 + 1 : adjacent_width2 - 1) / 2;

  switch (side) {
    case BoxSide::kTop:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max(-adjacent_width1, 0) / 2, y1,
          x2 - std::max(-adjacent_width2, 0) / 2, (y1 + y2 + 1) / 2, side,
          color, s1, adjacent1_big_half, adjacent2_big_half, antialias,
          auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max(adjacent_width1 + 1, 0) / 2, (y1 + y2 + 1) / 2,
          x2 - std::max(adjacent_width2 + 1, 0) / 2, y2, side, color, s2,
          adjacent_width1 / 2, adjacent_width2 / 2, antialias, auto_dark_mode);
      break;
    case BoxSide::kLeft:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1, y1 + std::max(-adjacent_width1, 0) / 2,
          (x1 + x2 + 1) / 2, y2 - std::max(-adjacent_width2, 0) / 2, side,
          color, s1, adjacent1_big_half, adjacent2_big_half, antialias,
          auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, (x1 + x2 + 1) / 2, y1 + std::max(adjacent_width1 + 1, 0) / 2,
          x2, y2 - std::max(adjacent_width2 + 1, 0) / 2, side, color, s2,
          adjacent_width1 / 2, adjacent_width2 / 2, antialias, auto_dark_mode);
      break;
    case BoxSide::kBottom:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max(adjacent_width1, 0) / 2, y1,
          x2 - std::max(adjacent_width2, 0) / 2, (y1 + y2 + 1) / 2, side, color,
          s2, adjacent1_big_half, adjacent2_big_half, antialias,
          auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1 + std::max(-adjacent_width1 + 1, 0) / 2,
          (y1 + y2 + 1) / 2, x2 - std::max(-adjacent_width2 + 1, 0) / 2, y2,
          side, color, s1, adjacent_width1 / 2, adjacent_width2 / 2, antialias,
          auto_dark_mode);
      break;
    case BoxSide::kRight:
      BoxBorderPainter::DrawLineForBoxSide(
          context, x1, y1 + std::max(adjacent_width1, 0) / 2, (x1 + x2 + 1) / 2,
          y2 - std::max(adjacent_width2, 0) / 2, side, color, s2,
          adjacent1_big_half, adjacent2_big_half, antialias, auto_dark_mode);
      BoxBorderPainter::DrawLineForBoxSide(
          context, (x1 + x2 + 1) / 2,
          y1 + std::max(-adjacent_width1 + 1, 0) / 2, x2,
          y2 - std::max(-adjacent_width2 + 1, 0) / 2, side, color, s1,
          adjacent_width1 / 2, adjacent_width2 / 2, antialias, auto_dark_mode);
      break;
  }
}

void FillQuad(GraphicsContext& context,
              const gfx::PointF quad[],
              const Color& color,
              bool antialias,
              const AutoDarkMode& auto_dark_mode) {
  SkPathBuilder path;
  path.moveTo(gfx::PointFToSkPoint(quad[0]));
  path.lineTo(gfx::PointFToSkPoint(quad[1]));
  path.lineTo(gfx::PointFToSkPoint(quad[2]));
  path.lineTo(gfx::PointFToSkPoint(quad[3]));
  cc::PaintFlags flags(context.FillFlags());
  flags.setAntiAlias(antialias);
  flags.setColor(color.Rgb());

  context.DrawPath(path.detach(), flags, auto_dark_mode);
}

void DrawSolidBoxSide(GraphicsContext& context,
                      int x1,
                      int y1,
                      int x2,
                      int y2,
                      BoxSide side,
                      Color color,
                      int adjacent_width1,
                      int adjacent_width2,
                      bool antialias,
                      const AutoDarkMode& auto_dark_mode) {
  DCHECK_GE(x2, x1);
  DCHECK_GE(y2, y1);

  if (!adjacent_width1 && !adjacent_width2) {
    // Tweak antialiasing to match the behavior of fillQuad();
    // this matters for rects in transformed contexts.
    bool was_antialiased = context.ShouldAntialias();
    if (antialias != was_antialiased)
      context.SetShouldAntialias(antialias);
    context.FillRect(gfx::Rect(x1, y1, x2 - x1, y2 - y1), color,
                     auto_dark_mode);
    if (antialias != was_antialiased)
      context.SetShouldAntialias(was_antialiased);
    return;
  }

  gfx::PointF quad[4];
  switch (side) {
    case BoxSide::kTop:
      quad[0] = gfx::PointF(x1 + std::max(-adjacent_width1, 0), y1);
      quad[1] = gfx::PointF(x1 + std::max(adjacent_width1, 0), y2);
      quad[2] = gfx::PointF(x2 - std::max(adjacent_width2, 0), y2);
      quad[3] = gfx::PointF(x2 - std::max(-adjacent_width2, 0), y1);
      break;
    case BoxSide::kBottom:
      quad[0] = gfx::PointF(x1 + std::max(adjacent_width1, 0), y1);
      quad[1] = gfx::PointF(x1 + std::max(-adjacent_width1, 0), y2);
      quad[2] = gfx::PointF(x2 - std::max(-adjacent_width2, 0), y2);
      quad[3] = gfx::PointF(x2 - std::max(adjacent_width2, 0), y1);
      break;
    case BoxSide::kLeft:
      quad[0] = gfx::PointF(x1, y1 + std::max(-adjacent_width1, 0));
      quad[1] = gfx::PointF(x1, y2 - std::max(-adjacent_width2, 0));
      quad[2] = gfx::PointF(x2, y2 - std::max(adjacent_width2, 0));
      quad[3] = gfx::PointF(x2, y1 + std::max(adjacent_width1, 0));
      break;
    case BoxSide::kRight:
      quad[0] = gfx::PointF(x1, y1 + std::max(adjacent_width1, 0));
      quad[1] = gfx::PointF(x1, y2 - std::max(adjacent_width2, 0));
      quad[2] = gfx::PointF(x2, y2 - std::max(-adjacent_width2, 0));
      quad[3] = gfx::PointF(x2, y1 + std::max(-adjacent_width1, 0));
      break;
  }

  FillQuad(context, quad, color, antialias, auto_dark_mode);
}

void FindIntersection(const gfx::PointF& p1,
                      const gfx::PointF& p2,
                      const gfx::PointF& d1,
                      const gfx::PointF& d2,
                      gfx::PointF& intersection) {
  float px_length = p2.x() - p1.x();
  float py_length = p2.y() - p1.y();

  float dx_length = d2.x() - d1.x();
  float dy_length = d2.y() - d1.y();

  float denom = px_length * dy_length - py_length * dx_length;
  if (!denom)
    return;

  float param =
      ((d1.x() - p1.x()) * dy_length - (d1.y() - p1.y()) * dx_length) / denom;

  intersection.set_x(p1.x() + param * px_length);
  intersection.set_y(p1.y() + param * py_length);
}

}  // anonymous namespace

// Holds edges grouped by opacity and sorted in paint order.
struct BoxBorderPainter::ComplexBorderInfo {
  explicit ComplexBorderInfo(const BoxBorderPainter& border_painter) {
    Vector<BoxSide, 4> sorted_sides;

    // First, collect all visible sides.
    for (unsigned i = border_painter.first_visible_edge_; i < 4; ++i) {
      BoxSide side = static_cast<BoxSide>(i);

      if (IncludesEdge(border_painter.visible_edge_set_, side))
        sorted_sides.push_back(side);
    }
    DCHECK(!sorted_sides.IsEmpty());

    // Then sort them in paint order, based on three (prioritized) criteria:
    // alpha, style, side.
    std::sort(sorted_sides.begin(), sorted_sides.end(),
              [&border_painter](BoxSide a, BoxSide b) -> bool {
                const BorderEdge& edge_a = border_painter.Edge(a);
                const BorderEdge& edge_b = border_painter.Edge(b);

                const unsigned alpha_a = edge_a.color.Alpha();
                const unsigned alpha_b = edge_b.color.Alpha();
                if (alpha_a != alpha_b)
                  return alpha_a < alpha_b;

                const unsigned style_priority_a =
                    kStylePriority[static_cast<unsigned>(edge_a.BorderStyle())];
                const unsigned style_priority_b =
                    kStylePriority[static_cast<unsigned>(edge_b.BorderStyle())];
                if (style_priority_a != style_priority_b)
                  return style_priority_a < style_priority_b;

                return kSidePriority[static_cast<unsigned>(a)] <
                       kSidePriority[static_cast<unsigned>(b)];
              });

    // Finally, build the opacity group structures.
    BuildOpacityGroups(border_painter, sorted_sides);

    if (border_painter.is_rounded_)
      rounded_border_path.AddRoundedRect(border_painter.outer_);
  }

  Vector<OpacityGroup, 4> opacity_groups;

  // Potentially used when drawing rounded borders.
  Path rounded_border_path;

 private:
  void BuildOpacityGroups(const BoxBorderPainter& border_painter,
                          const Vector<BoxSide, 4>& sorted_sides) {
    unsigned current_alpha = 0;
    for (BoxSide side : sorted_sides) {
      const BorderEdge& edge = border_painter.Edge(side);
      const unsigned edge_alpha = edge.color.Alpha();

      DCHECK_GT(edge_alpha, 0u);
      DCHECK_GE(edge_alpha, current_alpha);
      if (edge_alpha != current_alpha) {
        opacity_groups.push_back(OpacityGroup(edge_alpha));
        current_alpha = edge_alpha;
      }

      DCHECK(!opacity_groups.IsEmpty());
      OpacityGroup& current_group = opacity_groups.back();
      current_group.sides.push_back(side);
      current_group.edge_flags |= EdgeFlagForSide(side);
    }

    DCHECK(!opacity_groups.IsEmpty());
  }
};

void BoxBorderPainter::DrawDoubleBorder() const {
  DCHECK(is_uniform_color_);
  DCHECK(is_uniform_style_);
  DCHECK(FirstEdge().BorderStyle() == EBorderStyle::kDouble);
  DCHECK(visible_edge_set_ == kAllBorderEdges);

  const Color color = FirstEdge().color;

  // When painting outlines, we ignore outer/inner radii.
  const auto force_rectangular = !outer_.IsRounded() && !inner_.IsRounded();

  AutoDarkMode auto_dark_mode(
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));

  // outer stripe
  const LayoutRectOutsets outer_third_outsets =
      DoubleStripeOutsets(BorderEdge::kDoubleBorderStripeOuter);
  FloatRoundedRect outer_third_rect =
      RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
          style_, border_rect_, outer_third_outsets, sides_to_include_);
  if (force_rectangular)
    outer_third_rect.SetRadii(FloatRoundedRect::Radii());
  DrawBleedAdjustedDRRect(context_, bleed_avoidance_, outer_, outer_third_rect,
                          color, auto_dark_mode);

  // inner stripe
  const LayoutRectOutsets inner_third_outsets =
      DoubleStripeOutsets(BorderEdge::kDoubleBorderStripeInner);
  FloatRoundedRect inner_third_rect =
      RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
          style_, border_rect_, inner_third_outsets, sides_to_include_);
  if (force_rectangular)
    inner_third_rect.SetRadii(FloatRoundedRect::Radii());
  context_.FillDRRect(inner_third_rect, inner_, color, auto_dark_mode);
}

bool BoxBorderPainter::PaintBorderFastPath() const {
  if (!is_uniform_color_ || !is_uniform_style_ || !inner_.IsRenderable())
    return false;

  if (FirstEdge().BorderStyle() != EBorderStyle::kSolid &&
      FirstEdge().BorderStyle() != EBorderStyle::kDouble)
    return false;

  if (visible_edge_set_ == kAllBorderEdges) {
    if (FirstEdge().BorderStyle() == EBorderStyle::kSolid) {
      if (is_uniform_width_ && !outer_.IsRounded()) {
        // 4-side, solid, uniform-width, rectangular border => one drawRect()
        DrawSolidBorderRect(
            context_, outer_.Rect(), FirstEdge().Width(), FirstEdge().color,
            PaintAutoDarkMode(style_,
                              DarkModeFilter::ElementRole::kBackground));
      } else {
        // 4-side, solid border => one drawDRRect()
        DrawBleedAdjustedDRRect(
            context_, bleed_avoidance_, outer_, inner_, FirstEdge().color,
            PaintAutoDarkMode(style_,
                              DarkModeFilter::ElementRole::kBackground));
      }
    } else {
      // 4-side, double border => 2x drawDRRect()
      DCHECK(FirstEdge().BorderStyle() == EBorderStyle::kDouble);
      DrawDoubleBorder();
    }

    return true;
  }

  // This is faster than the normal complex border path only if it avoids
  // creating transparency layers (when the border is translucent).
  if (FirstEdge().BorderStyle() == EBorderStyle::kSolid &&
      !outer_.IsRounded() && has_alpha_) {
    DCHECK(visible_edge_set_ != kAllBorderEdges);
    // solid, rectangular border => one drawPath()
    Path path;
    path.SetWindRule(RULE_NONZERO);

    for (auto side :
         {BoxSide::kTop, BoxSide::kRight, BoxSide::kBottom, BoxSide::kLeft}) {
      const BorderEdge& curr_edge = Edge(side);
      if (curr_edge.ShouldRender())
        path.AddRect(CalculateSideRect(outer_, curr_edge, side));
    }

    context_.SetFillColor(FirstEdge().color);
    context_.FillPath(
        path,
        PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));
    return true;
  }

  return false;
}

BoxBorderPainter::BoxBorderPainter(GraphicsContext& context,
                                   const PhysicalRect& border_rect,
                                   const ComputedStyle& style,
                                   BackgroundBleedAvoidance bleed_avoidance,
                                   PhysicalBoxSides sides_to_include)
    : context_(context),
      border_rect_(border_rect),
      style_(style),
      bleed_avoidance_(bleed_avoidance),
      sides_to_include_(sides_to_include),
      visible_edge_count_(0),
      first_visible_edge_(0),
      visible_edge_set_(0),
      is_uniform_style_(true),
      is_uniform_width_(true),
      is_uniform_color_(true),
      is_rounded_(false),
      has_alpha_(false) {
  style.GetBorderEdgeInfo(edges_, sides_to_include);
  ComputeBorderProperties();

  // No need to compute the rrects if we don't have any borders to draw.
  if (!visible_edge_set_)
    return;

  outer_ = RoundedBorderGeometry::PixelSnappedRoundedBorder(style_, border_rect,
                                                            sides_to_include);
  inner_ = RoundedBorderGeometry::PixelSnappedRoundedInnerBorder(
      style_, border_rect, sides_to_include);

  // Make sure that the border width isn't larger than the border box, which
  // can pixel snap smaller.
  float max_width = outer_.Rect().width();
  float max_height = outer_.Rect().height();
  Edge(BoxSide::kTop).ClampWidth(max_height);
  Edge(BoxSide::kRight).ClampWidth(max_width);
  Edge(BoxSide::kBottom).ClampWidth(max_height);
  Edge(BoxSide::kLeft).ClampWidth(max_width);

  is_rounded_ = outer_.IsRounded();
}

BoxBorderPainter::BoxBorderPainter(GraphicsContext& context,
                                   const ComputedStyle& style,
                                   const PhysicalRect& border_rect,
                                   int width,
                                   const LayoutRectOutsets& inner_outsets)
    : context_(context),
      border_rect_(border_rect),
      outer_outsets_(inner_outsets + LayoutUnit(width)),
      style_(style),
      bleed_avoidance_(kBackgroundBleedNone),
      sides_to_include_(PhysicalBoxSides()),
      visible_edge_count_(0),
      first_visible_edge_(0),
      visible_edge_set_(0),
      is_uniform_style_(true),
      is_uniform_width_(true),
      is_uniform_color_(true),
      is_rounded_(false),
      has_alpha_(false) {
  DCHECK(style.HasOutline());

  BorderEdge edge(width,
                  style.VisitedDependentColor(GetCSSPropertyOutlineColor()),
                  style.OutlineStyle());
  for (auto& e : edges_)
    e = edge;
  ComputeBorderProperties();

  outer_ = RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
      style, border_rect, outer_outsets_);
  is_rounded_ = outer_.IsRounded();

  inner_ = RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
      style, border_rect, inner_outsets);
}

void BoxBorderPainter::ComputeBorderProperties() {
  for (unsigned i = 0; i < std::size(edges_); ++i) {
    const BorderEdge& edge = edges_[i];

    if (!edge.ShouldRender()) {
      if (edge.PresentButInvisible()) {
        is_uniform_width_ = false;
        is_uniform_color_ = false;
      }

      continue;
    }

    DCHECK_GT(edge.color.Alpha(), 0);

    visible_edge_count_++;
    visible_edge_set_ |= EdgeFlagForSide(static_cast<BoxSide>(i));

    has_alpha_ |= edge.color.HasAlpha();

    if (visible_edge_count_ == 1) {
      first_visible_edge_ = i;
      continue;
    }

    is_uniform_style_ &=
        edge.BorderStyle() == edges_[first_visible_edge_].BorderStyle();
    is_uniform_width_ &= edge.Width() == edges_[first_visible_edge_].Width();
    is_uniform_color_ &= edge.color == edges_[first_visible_edge_].color;
  }
}

void BoxBorderPainter::Paint() const {
  if (!visible_edge_count_ || outer_.Rect().IsEmpty())
    return;

  if (PaintBorderFastPath())
    return;

  bool clip_to_outer_border = outer_.IsRounded();
  GraphicsContextStateSaver state_saver(context_, clip_to_outer_border);
  if (clip_to_outer_border) {
    // For BackgroundBleedClip{Only,Layer}, the outer rrect clip is already
    // applied.
    if (!BleedAvoidanceIsClipping(bleed_avoidance_))
      context_.ClipRoundedRect(outer_);

    if (inner_.IsRenderable() && !inner_.IsEmpty())
      context_.ClipOutRoundedRect(inner_);
  }

  const ComplexBorderInfo border_info(*this);
  PaintOpacityGroup(border_info, 0, 1);
}

// In order to maximize the use of overdraw as a corner seam avoidance
// technique, we draw translucent border sides using the following algorithm:
//
//   1) cluster sides sharing the same opacity into "opacity groups"
//      [ComplexBorderInfo]
//   2) sort groups in increasing opacity order [ComplexBorderInfo]
//   3) reverse-iterate over groups (decreasing opacity order), pushing nested
//      transparency layers with adjusted/relative opacity [paintOpacityGroup]
//   4) iterate over groups (increasing opacity order), painting actual group
//      contents and then ending their corresponding transparency layer
//      [PaintOpacityGroup]
//
// Layers are created in decreasing opacity order (top -> bottom), while actual
// border sides are drawn in increasing opacity order (bottom -> top). At each
// level, opacity is adjusted to account for accumulated/ancestor layer alpha.
// Because opacity is applied via layers, the actual draw paint is opaque.
//
// As an example, let's consider a border with the following sides/opacities:
//
//   top:    1.0
//   right:  0.25
//   bottom: 0.5
//   left:   0.25
//
// These are grouped and sorted in ComplexBorderInfo as follows:
//
//   group[0]: { alpha: 1.0,  sides: top }
//   group[1]: { alpha: 0.5,  sides: bottom }
//   group[2]: { alpha: 0.25, sides: right, left }
//
// Applying the algorithm yields the following paint sequence:
//
//                                // no layer needed for group 0 (alpha = 1)
//   beginLayer(0.5)              // layer for group 1
//     beginLayer(0.5)            // layer for group 2 (alpha: 0.5 * 0.5 = 0.25)
//       paintSides(right, left)  // paint group 2
//     endLayer
//     paintSides(bottom)         // paint group 1
//   endLayer
//   paintSides(top)              // paint group 0
//
// Note that we're always drawing using opaque paints on top of less-opaque
// content - hence we can use overdraw to mask portions of the previous sides.
//
BorderEdgeFlags BoxBorderPainter::PaintOpacityGroup(
    const ComplexBorderInfo& border_info,
    unsigned index,
    float effective_opacity) const {
  DCHECK(effective_opacity > 0 && effective_opacity <= 1);

  const wtf_size_t opacity_group_count = border_info.opacity_groups.size();

  // For overdraw logic purposes, treat missing/transparent edges as completed.
  if (index >= opacity_group_count)
    return ~visible_edge_set_;

  // Groups are sorted in increasing opacity order, but we need to create layers
  // in decreasing opacity order - hence the reverse iteration.
  const OpacityGroup& group =
      border_info.opacity_groups[opacity_group_count - index - 1];

  // Adjust this group's paint opacity to account for ancestor transparency
  // layers (needed in case we avoid creating a layer below).
  unsigned paint_alpha = group.alpha / effective_opacity;
  DCHECK_LE(paint_alpha, 255u);

  // For the last (bottom) group, we can skip the layer even in the presence of
  // opacity iff it contains no adjecent edges (no in-group overdraw
  // possibility).
  bool needs_layer =
      group.alpha != 255 && (IncludesAdjacentEdges(group.edge_flags) ||
                             (index + 1 < border_info.opacity_groups.size()));

  if (needs_layer) {
    const float group_opacity = static_cast<float>(group.alpha) / 255;
    DCHECK_LT(group_opacity, effective_opacity);

    context_.BeginLayer(group_opacity / effective_opacity);
    effective_opacity = group_opacity;

    // Group opacity is applied via a layer => we draw the members using opaque
    // paint.
    paint_alpha = 255;
  }

  // Recursion may seem unpalatable here, but
  //   a) it has an upper bound of 4
  //   b) only triggers at all when mixing border sides with different opacities
  //   c) it allows us to express the layer nesting algorithm more naturally
  BorderEdgeFlags completed_edges =
      PaintOpacityGroup(border_info, index + 1, effective_opacity);

  // Paint the actual group edges with an alpha adjusted to account for
  // ancenstor layers opacity.
  for (BoxSide side : group.sides) {
    PaintSide(border_info, side, paint_alpha, completed_edges);
    completed_edges |= EdgeFlagForSide(side);
  }

  if (needs_layer)
    context_.EndLayer();

  return completed_edges;
}

void BoxBorderPainter::PaintSide(const ComplexBorderInfo& border_info,
                                 BoxSide side,
                                 unsigned alpha,
                                 BorderEdgeFlags completed_edges) const {
  const BorderEdge& edge = Edge(side);
  DCHECK(edge.ShouldRender());
  const Color color(edge.color.Red(), edge.color.Green(), edge.color.Blue(),
                    alpha);

  gfx::RectF side_rect = outer_.Rect();
  const Path* path = nullptr;

  // TODO(fmalita): find a way to consolidate these without sacrificing
  // readability.
  switch (side) {
    case BoxSide::kTop: {
      bool use_path =
          is_rounded_ && (BorderStyleHasInnerDetail(edge.BorderStyle()) ||
                          BorderWillArcInnerEdge(inner_.GetRadii().TopLeft(),
                                                 inner_.GetRadii().TopRight()));
      if (use_path)
        path = &border_info.rounded_border_path;
      else
        side_rect.set_height(floorf(edge.Width()));

      PaintOneBorderSide(side_rect, BoxSide::kTop, BoxSide::kLeft,
                         BoxSide::kRight, path, color, completed_edges);
      break;
    }
    case BoxSide::kBottom: {
      bool use_path = is_rounded_ &&
                      (BorderStyleHasInnerDetail(edge.BorderStyle()) ||
                       BorderWillArcInnerEdge(inner_.GetRadii().BottomLeft(),
                                              inner_.GetRadii().BottomRight()));
      if (use_path)
        path = &border_info.rounded_border_path;
      else
        SetToBottomSideRect(side_rect, floorf(edge.Width()));

      PaintOneBorderSide(side_rect, BoxSide::kBottom, BoxSide::kLeft,
                         BoxSide::kRight, path, color, completed_edges);
      break;
    }
    case BoxSide::kLeft: {
      bool use_path =
          is_rounded_ && (BorderStyleHasInnerDetail(edge.BorderStyle()) ||
                          BorderWillArcInnerEdge(inner_.GetRadii().BottomLeft(),
                                                 inner_.GetRadii().TopLeft()));
      if (use_path)
        path = &border_info.rounded_border_path;
      else
        side_rect.set_width(floorf(edge.Width()));

      PaintOneBorderSide(side_rect, BoxSide::kLeft, BoxSide::kTop,
                         BoxSide::kBottom, path, color, completed_edges);
      break;
    }
    case BoxSide::kRight: {
      bool use_path = is_rounded_ &&
                      (BorderStyleHasInnerDetail(edge.BorderStyle()) ||
                       BorderWillArcInnerEdge(inner_.GetRadii().BottomRight(),
                                              inner_.GetRadii().TopRight()));
      if (use_path)
        path = &border_info.rounded_border_path;
      else
        SetToRightSideRect(side_rect, floorf(edge.Width()));

      PaintOneBorderSide(side_rect, BoxSide::kRight, BoxSide::kTop,
                         BoxSide::kBottom, path, color, completed_edges);
      break;
    }
    default:
      NOTREACHED();
  }
}

BoxBorderPainter::MiterType BoxBorderPainter::ComputeMiter(
    BoxSide side,
    BoxSide adjacent_side,
    BorderEdgeFlags completed_edges) const {
  const BorderEdge& adjacent_edge = Edge(adjacent_side);

  // No miters for missing edges.
  if (!adjacent_edge.is_present)
    return kNoMiter;

  // The adjacent edge will overdraw this corner, resulting in a correct miter.
  if (WillOverdraw(adjacent_side, adjacent_edge.BorderStyle(), completed_edges))
    return kNoMiter;

  // Color transitions require miters. Use miters compatible with the AA drawing
  // mode to avoid introducing extra clips.
  if (!ColorsMatchAtCorner(side, adjacent_side))
    return kSoftMiter;

  // Non-anti-aliased miters ensure correct same-color seaming when required by
  // style.
  if (BorderStylesRequireMiter(side, adjacent_side, Edge(side).BorderStyle(),
                               adjacent_edge.BorderStyle()))
    return kHardMiter;

  // Overdraw the adjacent edge when the colors match and we have no style
  // restrictions.
  return kNoMiter;
}

bool BoxBorderPainter::MitersRequireClipping(MiterType miter1,
                                             MiterType miter2,
                                             EBorderStyle style) {
  // Clipping is required if any of the present miters doesn't match the current
  // AA mode.
  bool should_clip = miter1 == kHardMiter || miter2 == kHardMiter;

  // Some styles require clipping for any type of miter.
  should_clip = should_clip || ((miter1 != kNoMiter || miter2 != kNoMiter) &&
                                StyleRequiresClipPolygon(style));

  return should_clip;
}

void BoxBorderPainter::PaintOneBorderSide(
    const gfx::RectF& side_rect,
    BoxSide side,
    BoxSide adjacent_side1,
    BoxSide adjacent_side2,
    const Path* path,
    Color color,
    BorderEdgeFlags completed_edges) const {
  const BorderEdge& edge_to_render = Edge(side);
  DCHECK(edge_to_render.Width());
  const BorderEdge& adjacent_edge1 = Edge(adjacent_side1);
  const BorderEdge& adjacent_edge2 = Edge(adjacent_side2);

  if (path) {
    MiterType miter1 =
        ColorsMatchAtCorner(side, adjacent_side1) ? kHardMiter : kSoftMiter;
    MiterType miter2 =
        ColorsMatchAtCorner(side, adjacent_side2) ? kHardMiter : kSoftMiter;

    GraphicsContextStateSaver state_saver(context_);
    if (inner_.IsRenderable())
      ClipBorderSidePolygon(side, miter1, miter2);
    else
      ClipBorderSideForComplexInnerPath(side);
    float stroke_thickness =
        std::max(std::max(edge_to_render.Width(), adjacent_edge1.Width()),
                 adjacent_edge2.Width());
    DrawBoxSideFromPath(*path, edge_to_render.Width(), stroke_thickness, side,
                        color, edge_to_render.BorderStyle());
  } else {
    MiterType miter1 = ComputeMiter(side, adjacent_side1, completed_edges);
    MiterType miter2 = ComputeMiter(side, adjacent_side2, completed_edges);
    bool should_clip =
        MitersRequireClipping(miter1, miter2, edge_to_render.BorderStyle());

    GraphicsContextStateSaver clip_state_saver(context_, should_clip);
    if (should_clip) {
      ClipBorderSidePolygon(side, miter1, miter2);
      // Miters are applied via clipping, no need to draw them.
      miter1 = miter2 = kNoMiter;
    }

    DrawLineForBoxSide(
        context_, side_rect.x(), side_rect.y(), side_rect.right(),
        side_rect.bottom(), side, color, edge_to_render.BorderStyle(),
        miter1 != kNoMiter ? floorf(adjacent_edge1.Width()) : 0,
        miter2 != kNoMiter ? floorf(adjacent_edge2.Width()) : 0,
        /*antialias*/ true,
        PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));
  }
}

void BoxBorderPainter::DrawBoxSideFromPath(const Path& border_path,
                                           float border_thickness,
                                           float stroke_thickness,
                                           BoxSide side,
                                           Color color,
                                           EBorderStyle border_style) const {
  if (border_thickness <= 0)
    return;

  if (border_style == EBorderStyle::kDouble && border_thickness < 3)
    border_style = EBorderStyle::kSolid;

  switch (border_style) {
    case EBorderStyle::kNone:
    case EBorderStyle::kHidden:
      return;
    case EBorderStyle::kDotted:
    case EBorderStyle::kDashed: {
      DrawDashedDottedBoxSideFromPath(border_thickness, stroke_thickness, color,
                                      border_style);
      return;
    }
    case EBorderStyle::kDouble: {
      DrawDoubleBoxSideFromPath(border_path, border_thickness, stroke_thickness,
                                side, color);
      return;
    }
    case EBorderStyle::kRidge:
    case EBorderStyle::kGroove: {
      DrawRidgeGrooveBoxSideFromPath(border_path, border_thickness,
                                     stroke_thickness, side, color,
                                     border_style);
      return;
    }
    case EBorderStyle::kInset:
      if (side == BoxSide::kTop || side == BoxSide::kLeft)
        color = color.Dark();
      break;
    case EBorderStyle::kOutset:
      if (side == BoxSide::kBottom || side == BoxSide::kRight)
        color = color.Dark();
      break;
    default:
      break;
  }

  context_.SetStrokeStyle(kNoStroke);
  context_.SetFillColor(color);
  context_.DrawRect(
      gfx::ToRoundedRect(outer_.Rect()),
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));
}

void BoxBorderPainter::DrawDashedDottedBoxSideFromPath(
    float border_thickness,
    float stroke_thickness,
    Color color,
    EBorderStyle border_style) const {
  // Convert the path to be down the middle of the dots or dashes.
  Path centerline_path;
  centerline_path.AddRoundedRect(
      RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
          style_, border_rect_, CenterOutsets(), sides_to_include_));

  context_.SetStrokeColor(color);

  if (!StrokeData::StrokeIsDashed(border_thickness,
                                  border_style == EBorderStyle::kDashed
                                      ? kDashedStroke
                                      : kDottedStroke)) {
    DrawWideDottedBoxSideFromPath(centerline_path, border_thickness);
    return;
  }

  // The stroke is doubled here because the provided path is the
  // outside edge of the border so half the stroke is clipped off, with
  // the extra multiplier so that the clipping mask can antialias
  // the edges to prevent jaggies.
  const float thickness_multiplier = 2 * 1.1f;
  context_.SetStrokeThickness(stroke_thickness * thickness_multiplier);
  context_.SetStrokeStyle(
      border_style == EBorderStyle::kDashed ? kDashedStroke : kDottedStroke);

  // TODO(schenney): stroking the border path causes issues with tight corners:
  // https://bugs.chromium.org/p/chromium/issues/detail?id=344234
  context_.StrokePath(
      centerline_path,
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground),
      centerline_path.length(), border_thickness);
}

void BoxBorderPainter::DrawWideDottedBoxSideFromPath(
    const Path& border_path,
    float border_thickness) const {
  context_.SetStrokeThickness(border_thickness);
  context_.SetStrokeStyle(kDottedStroke);
  context_.SetLineCap(kRoundCap);

  // TODO(schenney): stroking the border path causes issues with tight corners:
  // https://bugs.webkit.org/show_bug.cgi?id=58711
  context_.StrokePath(
      border_path,
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground),
      border_path.length(), border_thickness);
}

void BoxBorderPainter::DrawDoubleBoxSideFromPath(
    const Path& border_path,
    float border_thickness,
    float stroke_thickness,
    BoxSide side,
    Color color) const {
  // Draw inner border line
  {
    GraphicsContextStateSaver state_saver(context_);
    const LayoutRectOutsets inner_outsets =
        DoubleStripeOutsets(BorderEdge::kDoubleBorderStripeInner);
    FloatRoundedRect inner_clip =
        RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
            style_, border_rect_, inner_outsets, sides_to_include_);

    context_.ClipRoundedRect(inner_clip);
    DrawBoxSideFromPath(border_path, border_thickness, stroke_thickness, side,
                        color, EBorderStyle::kSolid);
  }

  // Draw outer border line
  {
    GraphicsContextStateSaver state_saver(context_);
    PhysicalRect used_border_rect = border_rect_;
    LayoutRectOutsets outer_outsets =
        DoubleStripeOutsets(BorderEdge::kDoubleBorderStripeOuter);

    if (BleedAvoidanceIsClipping(bleed_avoidance_)) {
      used_border_rect.Inflate(LayoutUnit(1));
      outer_outsets.SetTop(outer_outsets.Top() - 1);
      outer_outsets.SetRight(outer_outsets.Right() - 1);
      outer_outsets.SetBottom(outer_outsets.Bottom() - 1);
      outer_outsets.SetLeft(outer_outsets.Left() - 1);
    }

    FloatRoundedRect outer_clip =
        RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
            style_, used_border_rect, outer_outsets, sides_to_include_);
    context_.ClipOutRoundedRect(outer_clip);
    DrawBoxSideFromPath(border_path, border_thickness, stroke_thickness, side,
                        color, EBorderStyle::kSolid);
  }
}

void BoxBorderPainter::DrawRidgeGrooveBoxSideFromPath(
    const Path& border_path,
    float border_thickness,
    float stroke_thickness,
    BoxSide side,
    Color color,
    EBorderStyle border_style) const {
  EBorderStyle s1;
  EBorderStyle s2;
  if (border_style == EBorderStyle::kGroove) {
    s1 = EBorderStyle::kInset;
    s2 = EBorderStyle::kOutset;
  } else {
    s1 = EBorderStyle::kOutset;
    s2 = EBorderStyle::kInset;
  }

  // Paint full border
  DrawBoxSideFromPath(border_path, border_thickness, stroke_thickness, side,
                      color, s1);

  // Paint inner only
  GraphicsContextStateSaver state_saver(context_);
  FloatRoundedRect clip_rect =
      RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
          style_, border_rect_, CenterOutsets(), sides_to_include_);

  context_.ClipRoundedRect(clip_rect);
  DrawBoxSideFromPath(border_path, border_thickness, stroke_thickness, side,
                      color, s2);
}

gfx::RectF BoxBorderPainter::CalculateSideRectIncludingInner(
    BoxSide side) const {
  gfx::RectF side_rect = outer_.Rect();
  float width;

  switch (side) {
    case BoxSide::kTop:
      width = side_rect.height() - Edge(BoxSide::kBottom).Width();
      side_rect.set_height(width);
      break;
    case BoxSide::kBottom:
      width = side_rect.height() - Edge(BoxSide::kTop).Width();
      SetToBottomSideRect(side_rect, width);
      break;
    case BoxSide::kLeft:
      width = side_rect.width() - Edge(BoxSide::kRight).Width();
      side_rect.set_width(width);
      break;
    case BoxSide::kRight:
      width = side_rect.width() - Edge(BoxSide::kLeft).Width();
      SetToRightSideRect(side_rect, width);
      break;
  }

  return side_rect;
}

void BoxBorderPainter::ClipBorderSideForComplexInnerPath(BoxSide side) const {
  context_.Clip(CalculateSideRectIncludingInner(side));
  FloatRoundedRect adjusted_inner_rect =
      CalculateAdjustedInnerBorder(inner_, side);
  if (!adjusted_inner_rect.IsEmpty())
    context_.ClipOutRoundedRect(adjusted_inner_rect);
}

void BoxBorderPainter::ClipBorderSidePolygon(BoxSide side,
                                             MiterType first_miter,
                                             MiterType second_miter) const {
  DCHECK(first_miter != kNoMiter || second_miter != kNoMiter);

  gfx::PointF edge_quad[4];  // The boundary of the edge for fill
  gfx::PointF
      bound_quad1;  // Point 1 of the rectilinear bounding box of EdgeQuad
  gfx::PointF
      bound_quad2;  // Point 2 of the rectilinear bounding box of EdgeQuad

  const PhysicalRect outer_rect = PhysicalRect::EnclosingRect(outer_.Rect());
  const PhysicalRect inner_rect = PhysicalRect::EnclosingRect(inner_.Rect());

  // For each side, create a quad that encompasses all parts of that side that
  // may draw, including areas inside the innerBorder.
  //
  //         0----------------3
  //       3  \              /  0
  //       |\  1----------- 2  /|
  //       | 2                1 |
  //       | |                | |
  //       | |                | |
  //       | 1                2 |
  //       |/  2------------1  \|
  //       0  /              \  3
  //         3----------------0

  // Offset size and direction to expand clipping quad
  const static float kExtensionLength = 1e-1f;
  gfx::Vector2dF extension_offset;
  switch (side) {
    case BoxSide::kTop:
      edge_quad[0] = gfx::PointF(outer_rect.MinXMinYCorner());
      edge_quad[1] = gfx::PointF(inner_rect.MinXMinYCorner());
      edge_quad[2] = gfx::PointF(inner_rect.MaxXMinYCorner());
      edge_quad[3] = gfx::PointF(outer_rect.MaxXMinYCorner());

      DCHECK(edge_quad[0].y() == edge_quad[3].y());
      DCHECK(edge_quad[1].y() == edge_quad[2].y());

      bound_quad1 = gfx::PointF(edge_quad[0].x(), edge_quad[1].y());
      bound_quad2 = gfx::PointF(edge_quad[3].x(), edge_quad[2].y());

      extension_offset.set_x(-kExtensionLength);
      extension_offset.set_y(0);

      if (!inner_.GetRadii().TopLeft().IsZero()) {
        FindIntersection(
            edge_quad[0], edge_quad[1],
            gfx::PointF(edge_quad[1].x() + inner_.GetRadii().TopLeft().width(),
                        edge_quad[1].y()),
            gfx::PointF(
                edge_quad[1].x(),
                edge_quad[1].y() + inner_.GetRadii().TopLeft().height()),
            edge_quad[1]);
        DCHECK(bound_quad1.y() <= edge_quad[1].y());
        bound_quad1.set_y(edge_quad[1].y());
        bound_quad2.set_y(edge_quad[1].y());
      }

      if (!inner_.GetRadii().TopRight().IsZero()) {
        FindIntersection(
            edge_quad[3], edge_quad[2],
            gfx::PointF(edge_quad[2].x() - inner_.GetRadii().TopRight().width(),
                        edge_quad[2].y()),
            gfx::PointF(
                edge_quad[2].x(),
                edge_quad[2].y() + inner_.GetRadii().TopRight().height()),
            edge_quad[2]);
        if (bound_quad1.y() < edge_quad[2].y()) {
          bound_quad1.set_y(edge_quad[2].y());
          bound_quad2.set_y(edge_quad[2].y());
        }
      }
      break;

    case BoxSide::kLeft:
      // Swap the order of adjacent edges to allow common code
      std::swap(first_miter, second_miter);
      edge_quad[0] = gfx::PointF(outer_rect.MinXMaxYCorner());
      edge_quad[1] = gfx::PointF(inner_rect.MinXMaxYCorner());
      edge_quad[2] = gfx::PointF(inner_rect.MinXMinYCorner());
      edge_quad[3] = gfx::PointF(outer_rect.MinXMinYCorner());

      DCHECK(edge_quad[0].x() == edge_quad[3].x());
      DCHECK(edge_quad[1].x() == edge_quad[2].x());

      bound_quad1 = gfx::PointF(edge_quad[1].x(), edge_quad[0].y());
      bound_quad2 = gfx::PointF(edge_quad[2].x(), edge_quad[3].y());

      extension_offset.set_x(0);
      extension_offset.set_y(kExtensionLength);

      if (!inner_.GetRadii().TopLeft().IsZero()) {
        FindIntersection(
            edge_quad[3], edge_quad[2],
            gfx::PointF(edge_quad[2].x() + inner_.GetRadii().TopLeft().width(),
                        edge_quad[2].y()),
            gfx::PointF(
                edge_quad[2].x(),
                edge_quad[2].y() + inner_.GetRadii().TopLeft().height()),
            edge_quad[2]);
        DCHECK(bound_quad2.x() <= edge_quad[2].x());
        bound_quad1.set_x(edge_quad[2].x());
        bound_quad2.set_x(edge_quad[2].x());
      }

      if (!inner_.GetRadii().BottomLeft().IsZero()) {
        FindIntersection(
            edge_quad[0], edge_quad[1],
            gfx::PointF(
                edge_quad[1].x() + inner_.GetRadii().BottomLeft().width(),
                edge_quad[1].y()),
            gfx::PointF(
                edge_quad[1].x(),
                edge_quad[1].y() - inner_.GetRadii().BottomLeft().height()),
            edge_quad[1]);
        if (bound_quad1.x() < edge_quad[1].x()) {
          bound_quad1.set_x(edge_quad[1].x());
          bound_quad2.set_x(edge_quad[1].x());
        }
      }
      break;

    case BoxSide::kBottom:
      // Swap the order of adjacent edges to allow common code
      std::swap(first_miter, second_miter);
      edge_quad[0] = gfx::PointF(outer_rect.MaxXMaxYCorner());
      edge_quad[1] = gfx::PointF(inner_rect.MaxXMaxYCorner());
      edge_quad[2] = gfx::PointF(inner_rect.MinXMaxYCorner());
      edge_quad[3] = gfx::PointF(outer_rect.MinXMaxYCorner());

      DCHECK(edge_quad[0].y() == edge_quad[3].y());
      DCHECK(edge_quad[1].y() == edge_quad[2].y());

      bound_quad1 = gfx::PointF(edge_quad[0].x(), edge_quad[1].y());
      bound_quad2 = gfx::PointF(edge_quad[3].x(), edge_quad[2].y());

      extension_offset.set_x(kExtensionLength);
      extension_offset.set_y(0);

      if (!inner_.GetRadii().BottomLeft().IsZero()) {
        FindIntersection(
            edge_quad[3], edge_quad[2],
            gfx::PointF(
                edge_quad[2].x() + inner_.GetRadii().BottomLeft().width(),
                edge_quad[2].y()),
            gfx::PointF(
                edge_quad[2].x(),
                edge_quad[2].y() - inner_.GetRadii().BottomLeft().height()),
            edge_quad[2]);
        DCHECK(bound_quad2.y() >= edge_quad[2].y());
        bound_quad1.set_y(edge_quad[2].y());
        bound_quad2.set_y(edge_quad[2].y());
      }

      if (!inner_.GetRadii().BottomRight().IsZero()) {
        FindIntersection(
            edge_quad[0], edge_quad[1],
            gfx::PointF(
                edge_quad[1].x() - inner_.GetRadii().BottomRight().width(),
                edge_quad[1].y()),
            gfx::PointF(
                edge_quad[1].x(),
                edge_quad[1].y() - inner_.GetRadii().BottomRight().height()),
            edge_quad[1]);
        if (bound_quad1.y() > edge_quad[1].y()) {
          bound_quad1.set_y(edge_quad[1].y());
          bound_quad2.set_y(edge_quad[1].y());
        }
      }
      break;

    case BoxSide::kRight:
      edge_quad[0] = gfx::PointF(outer_rect.MaxXMinYCorner());
      edge_quad[1] = gfx::PointF(inner_rect.MaxXMinYCorner());
      edge_quad[2] = gfx::PointF(inner_rect.MaxXMaxYCorner());
      edge_quad[3] = gfx::PointF(outer_rect.MaxXMaxYCorner());

      DCHECK(edge_quad[0].x() == edge_quad[3].x());
      DCHECK(edge_quad[1].x() == edge_quad[2].x());

      bound_quad1 = gfx::PointF(edge_quad[1].x(), edge_quad[0].y());
      bound_quad2 = gfx::PointF(edge_quad[2].x(), edge_quad[3].y());

      extension_offset.set_x(0);
      extension_offset.set_y(-kExtensionLength);

      if (!inner_.GetRadii().TopRight().IsZero()) {
        FindIntersection(
            edge_quad[0], edge_quad[1],
            gfx::PointF(edge_quad[1].x() - inner_.GetRadii().TopRight().width(),
                        edge_quad[1].y()),
            gfx::PointF(
                edge_quad[1].x(),
                edge_quad[1].y() + inner_.GetRadii().TopRight().height()),
            edge_quad[1]);
        DCHECK(bound_quad1.x() >= edge_quad[1].x());
        bound_quad1.set_x(edge_quad[1].x());
        bound_quad2.set_x(edge_quad[1].x());
      }

      if (!inner_.GetRadii().BottomRight().IsZero()) {
        FindIntersection(
            edge_quad[3], edge_quad[2],
            gfx::PointF(
                edge_quad[2].x() - inner_.GetRadii().BottomRight().width(),
                edge_quad[2].y()),
            gfx::PointF(
                edge_quad[2].x(),
                edge_quad[2].y() - inner_.GetRadii().BottomRight().height()),
            edge_quad[2]);
        if (bound_quad1.x() > edge_quad[2].x()) {
          bound_quad1.set_x(edge_quad[2].x());
          bound_quad2.set_x(edge_quad[2].x());
        }
      }
      break;
  }

  if (first_miter == second_miter) {
    ClipQuad(context_, edge_quad, first_miter == kSoftMiter);
    return;
  }

  // If antialiasing settings for the first edge and second edge are different,
  // they have to be addressed separately. We do this by applying 2 clips, one
  // for each miter, with the appropriate anti-aliasing setting. Each clip uses
  // 3 sides of the quad rectilinear bounding box and a 4th side aligned with
  // the miter edge. We extend the clip in the miter direction to ensure overlap
  // as each edge is drawn.
  if (first_miter != kNoMiter) {
    gfx::PointF clipping_quad[4];

    clipping_quad[0] = edge_quad[0] + extension_offset;
    FindIntersection(edge_quad[0], edge_quad[1], bound_quad1, bound_quad2,
                     clipping_quad[1]);
    clipping_quad[1] += extension_offset;
    clipping_quad[2] = bound_quad2;
    clipping_quad[3] = edge_quad[3];

    ClipQuad(context_, clipping_quad, first_miter == kSoftMiter);
  }

  if (second_miter != kNoMiter) {
    gfx::PointF clipping_quad[4];

    clipping_quad[0] = edge_quad[0];
    clipping_quad[1] = bound_quad1;
    FindIntersection(edge_quad[2], edge_quad[3], bound_quad1, bound_quad2,
                     clipping_quad[2]);
    clipping_quad[2] -= extension_offset;
    clipping_quad[3] = edge_quad[3] - extension_offset;

    ClipQuad(context_, clipping_quad, second_miter == kSoftMiter);
  }
}

LayoutRectOutsets BoxBorderPainter::DoubleStripeOutsets(
    BorderEdge::DoubleBorderStripe stripe) const {
  return outer_outsets_ -
         LayoutRectOutsets(
             Edge(BoxSide::kTop).GetDoubleBorderStripeWidth(stripe),
             Edge(BoxSide::kRight).GetDoubleBorderStripeWidth(stripe),
             Edge(BoxSide::kBottom).GetDoubleBorderStripeWidth(stripe),
             Edge(BoxSide::kLeft).GetDoubleBorderStripeWidth(stripe));
}

LayoutRectOutsets BoxBorderPainter::CenterOutsets() const {
  return outer_outsets_ -
         LayoutRectOutsets(Edge(BoxSide::kTop).UsedWidth() * 0.5,
                           Edge(BoxSide::kRight).UsedWidth() * 0.5,
                           Edge(BoxSide::kBottom).UsedWidth() * 0.5,
                           Edge(BoxSide::kLeft).UsedWidth() * 0.5);
}

bool BoxBorderPainter::ColorsMatchAtCorner(BoxSide side,
                                           BoxSide adjacent_side) const {
  if (!Edge(adjacent_side).ShouldRender())
    return false;

  if (!Edge(side).SharesColorWith(Edge(adjacent_side)))
    return false;

  return !BorderStyleHasUnmatchedColorsAtCorner(Edge(side).BorderStyle(), side,
                                                adjacent_side);
}

void BoxBorderPainter::DrawLineForBoxSide(GraphicsContext& context,
                                          float x1,
                                          float y1,
                                          float x2,
                                          float y2,
                                          BoxSide side,
                                          Color color,
                                          EBorderStyle style,
                                          int adjacent_width1,
                                          int adjacent_width2,
                                          bool antialias,
                                          const AutoDarkMode& auto_dark_mode) {
  float thickness;
  float length;
  if (side == BoxSide::kTop || side == BoxSide::kBottom) {
    thickness = y2 - y1;
    length = x2 - x1;
  } else {
    thickness = x2 - x1;
    length = y2 - y1;
  }

  // We would like this check to be an ASSERT as we don't want to draw empty
  // borders. However nothing guarantees that the following recursive calls to
  // DrawLineForBoxSide() will have positive thickness and length.
  if (length <= 0 || thickness <= 0)
    return;

  if (style == EBorderStyle::kDouble && thickness < 3)
    style = EBorderStyle::kSolid;

  switch (style) {
    case EBorderStyle::kNone:
    case EBorderStyle::kHidden:
      return;
    case EBorderStyle::kDotted:
    case EBorderStyle::kDashed:
      DrawDashedOrDottedBoxSide(context, x1, y1, x2, y2, side, color, thickness,
                                style, antialias, auto_dark_mode);
      break;
    case EBorderStyle::kDouble:
      DrawDoubleBoxSide(context, x1, y1, x2, y2, length, side, color, thickness,
                        adjacent_width1, adjacent_width2, antialias,
                        auto_dark_mode);
      break;
    case EBorderStyle::kRidge:
    case EBorderStyle::kGroove:
      DrawRidgeOrGrooveBoxSide(context, x1, y1, x2, y2, side, color, style,
                               adjacent_width1, adjacent_width2, antialias,
                               auto_dark_mode);
      break;
    case EBorderStyle::kInset:
      // FIXME: Maybe we should lighten the colors on one side like Firefox.
      // https://bugs.webkit.org/show_bug.cgi?id=58608
      if (side == BoxSide::kTop || side == BoxSide::kLeft)
        color = color.Dark();
      [[fallthrough]];
    case EBorderStyle::kOutset:
      if (style == EBorderStyle::kOutset &&
          (side == BoxSide::kBottom || side == BoxSide::kRight))
        color = color.Dark();
      [[fallthrough]];
    case EBorderStyle::kSolid:
      DrawSolidBoxSide(context, x1, y1, x2, y2, side, color, adjacent_width1,
                       adjacent_width2, antialias, auto_dark_mode);
      break;
  }
}

}  // namespace blink
