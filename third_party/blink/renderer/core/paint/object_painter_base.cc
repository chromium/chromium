// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/object_painter_base.h"

#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

namespace blink {

namespace {

struct OutlineEdgeInfo {
  int x1;
  int y1;
  int x2;
  int y2;
  BoxSide side;
};

// Adjust length of edges if needed. Returns the width of the joint.
int AdjustJoint(int outline_width,
                OutlineEdgeInfo& edge1,
                OutlineEdgeInfo& edge2) {
  // A clockwise joint:
  // - needs no adjustment of edge length because our edges are along the
  //   clockwise outer edge of the outline;
  // - needs a positive adjacent joint width (required by
  // ObjectPainterBase::DrawLineForBoxSide). A counterclockwise joint: - needs
  // to increase the edge length to include the joint; - needs a negative
  // adjacent joint width (required by ObjectPainterBase::DrawLineForBoxSide).
  switch (edge1.side) {
    case BoxSide::kTop:
      switch (edge2.side) {
        case BoxSide::kRight:  // Clockwise
          return outline_width;
        case BoxSide::kLeft:  // Counterclockwise
          edge1.x2 += outline_width;
          edge2.y2 += outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    case BoxSide::kRight:
      switch (edge2.side) {
        case BoxSide::kBottom:  // Clockwise
          return outline_width;
        case BoxSide::kTop:  // Counterclockwise
          edge1.y2 += outline_width;
          edge2.x1 -= outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    case BoxSide::kBottom:
      switch (edge2.side) {
        case BoxSide::kLeft:  // Clockwise
          return outline_width;
        case BoxSide::kRight:  // Counterclockwise
          edge1.x1 -= outline_width;
          edge2.y1 -= outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    case BoxSide::kLeft:
      switch (edge2.side) {
        case BoxSide::kTop:  // Clockwise
          return outline_width;
        case BoxSide::kBottom:  // Counterclockwise
          edge1.y1 -= outline_width;
          edge2.x2 += outline_width;
          return -outline_width;
        default:  // Same side or no joint.
          return 0;
      }
    default:
      NOTREACHED();
      return 0;
  }
}

void PaintComplexOutline(GraphicsContext& graphics_context,
                         const Vector<IntRect> rects,
                         const ComputedStyle& style,
                         const Color& color) {
  DCHECK(!style.OutlineStyleIsAuto());

  // Construct a clockwise path along the outer edge of the outline.
  SkRegion region;
  uint16_t width = style.OutlineWidth();
  int outset = style.OutlineOffset() + style.OutlineWidth();
  for (auto& r : rects) {
    IntRect rect = r;
    rect.Inflate(outset);
    region.op(rect, SkRegion::kUnion_Op);
  }
  SkPath path;
  if (!region.getBoundaryPath(&path))
    return;

  Vector<OutlineEdgeInfo, 4> edges;

  SkPath::Iter iter(path, false);
  SkPoint points[4];
  wtf_size_t count = 0;
  for (SkPath::Verb verb = iter.next(points, false); verb != SkPath::kDone_Verb;
       verb = iter.next(points, false)) {
    if (verb != SkPath::kLine_Verb)
      continue;

    edges.Grow(++count);
    OutlineEdgeInfo& edge = edges.back();
    edge.x1 = SkScalarTruncToInt(points[0].x());
    edge.y1 = SkScalarTruncToInt(points[0].y());
    edge.x2 = SkScalarTruncToInt(points[1].x());
    edge.y2 = SkScalarTruncToInt(points[1].y());
    if (edge.x1 == edge.x2) {
      if (edge.y1 < edge.y2) {
        edge.x1 -= width;
        edge.side = BoxSide::kRight;
      } else {
        std::swap(edge.y1, edge.y2);
        edge.x2 += width;
        edge.side = BoxSide::kLeft;
      }
    } else {
      DCHECK(edge.y1 == edge.y2);
      if (edge.x1 < edge.x2) {
        edge.y2 += width;
        edge.side = BoxSide::kTop;
      } else {
        std::swap(edge.x1, edge.x2);
        edge.y1 -= width;
        edge.side = BoxSide::kBottom;
      }
    }
  }

  if (!count)
    return;

  Color outline_color = color;
  bool use_transparency_layer = color.HasAlpha();
  if (use_transparency_layer) {
    graphics_context.BeginLayer(static_cast<float>(color.Alpha()) / 255);
    outline_color =
        Color(outline_color.Red(), outline_color.Green(), outline_color.Blue());
  }

  DCHECK(count >= 4 && edges.size() == count);
  int first_adjacent_width = AdjustJoint(width, edges.back(), edges.front());

  // The width of the angled part of starting and ending joint of the current
  // edge.
  int adjacent_width_start = first_adjacent_width;
  int adjacent_width_end;
  for (wtf_size_t i = 0; i < count; ++i) {
    OutlineEdgeInfo& edge = edges[i];
    adjacent_width_end = i == count - 1
                             ? first_adjacent_width
                             : AdjustJoint(width, edge, edges[i + 1]);
    int adjacent_width1 = adjacent_width_start;
    int adjacent_width2 = adjacent_width_end;
    if (edge.side == BoxSide::kLeft || edge.side == BoxSide::kBottom)
      std::swap(adjacent_width1, adjacent_width2);
    ObjectPainterBase::DrawLineForBoxSide(
        graphics_context, edge.x1, edge.y1, edge.x2, edge.y2, edge.side,
        outline_color, style.OutlineStyle(), adjacent_width1, adjacent_width2,
        false);
    adjacent_width_start = adjacent_width_end;
  }

  if (use_transparency_layer)
    graphics_context.EndLayer();
}

void PaintSingleRectangleOutline(const PaintInfo& paint_info,
                                 const IntRect& rect,
                                 const ComputedStyle& style,
                                 const Color& color) {
  DCHECK(!style.OutlineStyleIsAuto());

  PhysicalRect inner(rect);
  inner.Inflate(LayoutUnit(style.OutlineOffset()));
  PhysicalRect outer(inner);
  outer.Inflate(LayoutUnit(style.OutlineWidth()));
  const BorderEdge common_edge_info(style.OutlineWidth(), color,
                                    style.OutlineStyle());
  BoxBorderPainter(style, outer, inner, common_edge_info)
      .PaintBorder(paint_info, outer);
}

void FillQuad(GraphicsContext& context,
              const FloatPoint quad[],
              const Color& color,
              bool antialias) {
  SkPath path;
  path.moveTo(FloatPointToSkPoint(quad[0]));
  path.lineTo(FloatPointToSkPoint(quad[1]));
  path.lineTo(FloatPointToSkPoint(quad[2]));
  path.lineTo(FloatPointToSkPoint(quad[3]));
  PaintFlags flags(context.FillFlags());
  flags.setAntiAlias(antialias);
  flags.setColor(color.Rgb());

  context.DrawPath(path, flags);
}

void DrawDashedOrDottedBoxSide(GraphicsContext& graphics_context,
                               int x1,
                               int y1,
                               int x2,
                               int y2,
                               BoxSide side,
                               Color color,
                               int thickness,
                               EBorderStyle style,
                               bool antialias) {
  DCHECK_GT(thickness, 0);

  GraphicsContextStateSaver state_saver(graphics_context);
  graphics_context.SetShouldAntialias(antialias);
  graphics_context.SetStrokeColor(color);
  graphics_context.SetStrokeThickness(thickness);
  graphics_context.SetStrokeStyle(
      style == EBorderStyle::kDashed ? kDashedStroke : kDottedStroke);

  switch (side) {
    case BoxSide::kBottom:
    case BoxSide::kTop: {
      int mid_y = y1 + thickness / 2;
      graphics_context.DrawLine(IntPoint(x1, mid_y), IntPoint(x2, mid_y));
      break;
    }
    case BoxSide::kRight:
    case BoxSide::kLeft: {
      int mid_x = x1 + thickness / 2;
      graphics_context.DrawLine(IntPoint(mid_x, y1), IntPoint(mid_x, y2));
      break;
    }
  }
}

void DrawDoubleBoxSide(GraphicsContext& graphics_context,
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
                       bool antialias) {
  int third_of_thickness = (thickness + 1) / 3;
  DCHECK_GT(third_of_thickness, 0);

  if (!adjacent_width1 && !adjacent_width2) {
    StrokeStyle old_stroke_style = graphics_context.GetStrokeStyle();
    graphics_context.SetStrokeStyle(kNoStroke);
    graphics_context.SetFillColor(color);

    bool was_antialiased = graphics_context.ShouldAntialias();
    graphics_context.SetShouldAntialias(antialias);

    switch (side) {
      case BoxSide::kTop:
      case BoxSide::kBottom:
        graphics_context.DrawRect(IntRect(x1, y1, length, third_of_thickness));
        graphics_context.DrawRect(
            IntRect(x1, y2 - third_of_thickness, length, third_of_thickness));
        break;
      case BoxSide::kLeft:
      case BoxSide::kRight:
        graphics_context.DrawRect(IntRect(x1, y1, third_of_thickness, length));
        graphics_context.DrawRect(
            IntRect(x2 - third_of_thickness, y1, third_of_thickness, length));
        break;
    }

    graphics_context.SetShouldAntialias(was_antialiased);
    graphics_context.SetStrokeStyle(old_stroke_style);
    return;
  }

  int adjacent1_big_third =
      ((adjacent_width1 > 0) ? adjacent_width1 + 1 : adjacent_width1 - 1) / 3;
  int adjacent2_big_third =
      ((adjacent_width2 > 0) ? adjacent_width2 + 1 : adjacent_width2 - 1) / 3;

  switch (side) {
    case BoxSide::kTop:
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
          y1, x2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0),
          y1 + third_of_thickness, side, color, EBorderStyle::kSolid,
          adjacent1_big_third, adjacent2_big_third, antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max((adjacent_width1 * 2 + 1) / 3, 0),
          y2 - third_of_thickness,
          x2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), y2, side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias);
      break;
    case BoxSide::kLeft:
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1,
          y1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
          x1 + third_of_thickness,
          y2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x2 - third_of_thickness,
          y1 + std::max((adjacent_width1 * 2 + 1) / 3, 0), x2,
          y2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias);
      break;
    case BoxSide::kBottom:
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max((adjacent_width1 * 2 + 1) / 3, 0), y1,
          x2 - std::max((adjacent_width2 * 2 + 1) / 3, 0),
          y1 + third_of_thickness, side, color, EBorderStyle::kSolid,
          adjacent1_big_third, adjacent2_big_third, antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0),
          y2 - third_of_thickness,
          x2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), y2, side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias);
      break;
    case BoxSide::kRight:
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1, y1 + std::max((adjacent_width1 * 2 + 1) / 3, 0),
          x1 + third_of_thickness,
          y2 - std::max((adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x2 - third_of_thickness,
          y1 + std::max((-adjacent_width1 * 2 + 1) / 3, 0), x2,
          y2 - std::max((-adjacent_width2 * 2 + 1) / 3, 0), side, color,
          EBorderStyle::kSolid, adjacent1_big_third, adjacent2_big_third,
          antialias);
      break;
    default:
      break;
  }
}

void DrawRidgeOrGrooveBoxSide(GraphicsContext& graphics_context,
                              int x1,
                              int y1,
                              int x2,
                              int y2,
                              BoxSide side,
                              Color color,
                              EBorderStyle style,
                              int adjacent_width1,
                              int adjacent_width2,
                              bool antialias) {
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
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max(-adjacent_width1, 0) / 2, y1,
          x2 - std::max(-adjacent_width2, 0) / 2, (y1 + y2 + 1) / 2, side,
          color, s1, adjacent1_big_half, adjacent2_big_half, antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max(adjacent_width1 + 1, 0) / 2,
          (y1 + y2 + 1) / 2, x2 - std::max(adjacent_width2 + 1, 0) / 2, y2,
          side, color, s2, adjacent_width1 / 2, adjacent_width2 / 2, antialias);
      break;
    case BoxSide::kLeft:
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1, y1 + std::max(-adjacent_width1, 0) / 2,
          (x1 + x2 + 1) / 2, y2 - std::max(-adjacent_width2, 0) / 2, side,
          color, s1, adjacent1_big_half, adjacent2_big_half, antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, (x1 + x2 + 1) / 2,
          y1 + std::max(adjacent_width1 + 1, 0) / 2, x2,
          y2 - std::max(adjacent_width2 + 1, 0) / 2, side, color, s2,
          adjacent_width1 / 2, adjacent_width2 / 2, antialias);
      break;
    case BoxSide::kBottom:
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max(adjacent_width1, 0) / 2, y1,
          x2 - std::max(adjacent_width2, 0) / 2, (y1 + y2 + 1) / 2, side, color,
          s2, adjacent1_big_half, adjacent2_big_half, antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1 + std::max(-adjacent_width1 + 1, 0) / 2,
          (y1 + y2 + 1) / 2, x2 - std::max(-adjacent_width2 + 1, 0) / 2, y2,
          side, color, s1, adjacent_width1 / 2, adjacent_width2 / 2, antialias);
      break;
    case BoxSide::kRight:
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, x1, y1 + std::max(adjacent_width1, 0) / 2,
          (x1 + x2 + 1) / 2, y2 - std::max(adjacent_width2, 0) / 2, side, color,
          s2, adjacent1_big_half, adjacent2_big_half, antialias);
      ObjectPainterBase::DrawLineForBoxSide(
          graphics_context, (x1 + x2 + 1) / 2,
          y1 + std::max(-adjacent_width1 + 1, 0) / 2, x2,
          y2 - std::max(-adjacent_width2 + 1, 0) / 2, side, color, s1,
          adjacent_width1 / 2, adjacent_width2 / 2, antialias);
      break;
  }
}

void DrawSolidBoxSide(GraphicsContext& graphics_context,
                      int x1,
                      int y1,
                      int x2,
                      int y2,
                      BoxSide side,
                      Color color,
                      int adjacent_width1,
                      int adjacent_width2,
                      bool antialias) {
  DCHECK_GE(x2, x1);
  DCHECK_GE(y2, y1);

  if (!adjacent_width1 && !adjacent_width2) {
    // Tweak antialiasing to match the behavior of fillQuad();
    // this matters for rects in transformed contexts.
    bool was_antialiased = graphics_context.ShouldAntialias();
    if (antialias != was_antialiased)
      graphics_context.SetShouldAntialias(antialias);
    graphics_context.FillRect(IntRect(x1, y1, x2 - x1, y2 - y1), color);
    if (antialias != was_antialiased)
      graphics_context.SetShouldAntialias(was_antialiased);
    return;
  }

  FloatPoint quad[4];
  switch (side) {
    case BoxSide::kTop:
      quad[0] = FloatPoint(x1 + std::max(-adjacent_width1, 0), y1);
      quad[1] = FloatPoint(x1 + std::max(adjacent_width1, 0), y2);
      quad[2] = FloatPoint(x2 - std::max(adjacent_width2, 0), y2);
      quad[3] = FloatPoint(x2 - std::max(-adjacent_width2, 0), y1);
      break;
    case BoxSide::kBottom:
      quad[0] = FloatPoint(x1 + std::max(adjacent_width1, 0), y1);
      quad[1] = FloatPoint(x1 + std::max(-adjacent_width1, 0), y2);
      quad[2] = FloatPoint(x2 - std::max(-adjacent_width2, 0), y2);
      quad[3] = FloatPoint(x2 - std::max(adjacent_width2, 0), y1);
      break;
    case BoxSide::kLeft:
      quad[0] = FloatPoint(x1, y1 + std::max(-adjacent_width1, 0));
      quad[1] = FloatPoint(x1, y2 - std::max(-adjacent_width2, 0));
      quad[2] = FloatPoint(x2, y2 - std::max(adjacent_width2, 0));
      quad[3] = FloatPoint(x2, y1 + std::max(adjacent_width1, 0));
      break;
    case BoxSide::kRight:
      quad[0] = FloatPoint(x1, y1 + std::max(adjacent_width1, 0));
      quad[1] = FloatPoint(x1, y2 - std::max(adjacent_width2, 0));
      quad[2] = FloatPoint(x2, y2 - std::max(-adjacent_width2, 0));
      quad[3] = FloatPoint(x2, y1 + std::max(-adjacent_width1, 0));
      break;
  }

  FillQuad(graphics_context, quad, color, antialias);
}

}  // anonymous namespace

void ObjectPainterBase::PaintOutlineRects(
    const PaintInfo& paint_info,
    const Vector<PhysicalRect>& outline_rects,
    const ComputedStyle& style) {
  Vector<IntRect> pixel_snapped_outline_rects;
  for (auto& r : outline_rects)
    pixel_snapped_outline_rects.push_back(PixelSnappedIntRect(r));

  Color color = style.VisitedDependentColor(GetCSSPropertyOutlineColor());
  if (style.OutlineStyleIsAuto()) {
    paint_info.context.DrawFocusRing(
        pixel_snapped_outline_rects, style.GetOutlineStrokeWidthForFocusRing(),
        style.OutlineOffset(), color,
        LayoutTheme::GetTheme().IsFocusRingOutset());
    return;
  }

  IntRect united_outline_rect =
      UnionRectEvenIfEmpty(pixel_snapped_outline_rects);
  if (united_outline_rect == pixel_snapped_outline_rects[0]) {
    PaintSingleRectangleOutline(paint_info, united_outline_rect, style, color);
    return;
  }
  PaintComplexOutline(paint_info.context, pixel_snapped_outline_rects, style,
                      color);
}

void ObjectPainterBase::DrawLineForBoxSide(GraphicsContext& graphics_context,
                                           float x1,
                                           float y1,
                                           float x2,
                                           float y2,
                                           BoxSide side,
                                           Color color,
                                           EBorderStyle style,
                                           int adjacent_width1,
                                           int adjacent_width2,
                                           bool antialias) {
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
  // ObjectPainterBase::DrawLineForBoxSide will have positive thickness and
  // length.
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
      DrawDashedOrDottedBoxSide(graphics_context, x1, y1, x2, y2, side, color,
                                thickness, style, antialias);
      break;
    case EBorderStyle::kDouble:
      DrawDoubleBoxSide(graphics_context, x1, y1, x2, y2, length, side, color,
                        thickness, adjacent_width1, adjacent_width2, antialias);
      break;
    case EBorderStyle::kRidge:
    case EBorderStyle::kGroove:
      DrawRidgeOrGrooveBoxSide(graphics_context, x1, y1, x2, y2, side, color,
                               style, adjacent_width1, adjacent_width2,
                               antialias);
      break;
    case EBorderStyle::kInset:
      // FIXME: Maybe we should lighten the colors on one side like Firefox.
      // https://bugs.webkit.org/show_bug.cgi?id=58608
      if (side == BoxSide::kTop || side == BoxSide::kLeft)
        color = color.Dark();
      FALLTHROUGH;
    case EBorderStyle::kOutset:
      if (style == EBorderStyle::kOutset &&
          (side == BoxSide::kBottom || side == BoxSide::kRight))
        color = color.Dark();
      FALLTHROUGH;
    case EBorderStyle::kSolid:
      DrawSolidBoxSide(graphics_context, x1, y1, x2, y2, side, color,
                       adjacent_width1, adjacent_width2, antialias);
      break;
  }
}

}  // namespace blink
