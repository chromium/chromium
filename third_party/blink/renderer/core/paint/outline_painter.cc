// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/outline_painter.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "ui/native_theme/native_theme.h"

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
  //   BoxBorderPainter::DrawLineForBoxSide).
  // A counterclockwise joint:
  // - needs to increase the edge length to include the joint;
  // - needs a negative adjacent joint width (required by
  //   BoxBorderPainter::DrawLineForBoxSide).
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

void ApplyOutlineOffset(IntRect& rect, int offset) {
  // A negative outline-offset should not cause the rendered outline shape to
  // become smaller than twice the computed value of the outline-width, in each
  // direction separately. See: https://drafts.csswg.org/css-ui/#outline-offset
  rect.InflateX(std::max(offset, -rect.Width() / 2));
  rect.InflateY(std::max(offset, -rect.Height() / 2));
}

void PaintComplexOutline(GraphicsContext& graphics_context,
                         const Vector<IntRect> rects,
                         const ComputedStyle& style,
                         const Color& color) {
  DCHECK(!style.OutlineStyleIsAuto());

  // Construct a clockwise path along the outer edge of the outline.
  SkRegion region;
  uint16_t width = style.OutlineWidthInt();
  int offset = style.OutlineOffsetInt();
  for (auto& r : rects) {
    IntRect rect = r;
    ApplyOutlineOffset(rect, offset);
    rect.Inflate(width);
    region.op(rect, SkRegion::kUnion_Op);
  }
  SkPath path;
  if (!region.getBoundaryPath(&path))
    return;

  Vector<OutlineEdgeInfo, 4> edges;

  SkPath::RawIter iter(path);
  SkPoint points[4], first_point, last_point;
  wtf_size_t count = 0;
  for (SkPath::Verb verb = iter.next(points); verb != SkPath::kDone_Verb;
       verb = iter.next(points)) {
    // Keep track of the first and last point of each contour (started with
    // kMove_Verb) so we can add the closing-line on kClose_Verb.
    if (verb == SkPath::kMove_Verb) {
      first_point = points[0];
      last_point = first_point;  // this gets reset after each line, but we
                                 // initialize it here
    } else if (verb == SkPath::kClose_Verb) {
      // create an artificial line to close the contour
      verb = SkPath::kLine_Verb;
      points[0] = last_point;
      points[1] = first_point;
    }
    if (verb != SkPath::kLine_Verb)
      continue;
    last_point = points[1];

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
    BoxBorderPainter::DrawLineForBoxSide(
        graphics_context, edge.x1, edge.y1, edge.x2, edge.y2, edge.side,
        outline_color, style.OutlineStyle(), adjacent_width1, adjacent_width2,
        false);
    adjacent_width_start = adjacent_width_end;
  }

  if (use_transparency_layer)
    graphics_context.EndLayer();
}

void PaintSingleRectangleOutline(GraphicsContext& context,
                                 const IntRect& rect,
                                 const ComputedStyle& style,
                                 const Color& color) {
  DCHECK(!style.OutlineStyleIsAuto());

  IntRect offset_rect = rect;
  ApplyOutlineOffset(offset_rect, style.OutlineOffsetInt());

  PhysicalRect inner(offset_rect);
  PhysicalRect outer(inner);
  outer.Inflate(LayoutUnit(style.OutlineWidthInt()));
  const BorderEdge edge(style.OutlineWidthInt(), color, style.OutlineStyle());
  BoxBorderPainter::PaintSingleRectOutline(context, style, outer, inner, edge);
}

float GetFocusRingBorderRadius(const ComputedStyle& style) {
  // Default style is border-radius equal to outline width.
  float border_radius = style.GetOutlineStrokeWidthForFocusRing();
  if (!style.HasAuthorBorder() && style.HasEffectiveAppearance()) {
    // For the elements that have not been styled and that have an appearance,
    // the focus ring should use the same border radius as the one used for
    // drawing the element.
    absl::optional<ui::NativeTheme::Part> part;
    switch (style.EffectiveAppearance()) {
      case kCheckboxPart:
        part = ui::NativeTheme::kCheckbox;
        break;
      case kRadioPart:
        part = ui::NativeTheme::kRadio;
        break;
      case kPushButtonPart:
      case kSquareButtonPart:
      case kButtonPart:
        part = ui::NativeTheme::kPushButton;
        break;
      case kTextFieldPart:
      case kTextAreaPart:
      case kSearchFieldPart:
        part = ui::NativeTheme::kTextField;
        break;
      default:
        break;
    }
    if (part) {
      border_radius =
          ui::NativeTheme::GetInstanceForWeb()->GetBorderRadiusForPart(
              part.value(), style.Width().GetFloatValue(),
              style.Height().GetFloatValue());

      border_radius =
          ui::NativeTheme::GetInstanceForWeb()->AdjustBorderRadiusByZoom(
              part.value(), border_radius, style.EffectiveZoom());
    }
  }

  return border_radius;
}

}  // anonymous namespace

void OutlinePainter::PaintOutlineRects(
    GraphicsContext& context,
    const Vector<PhysicalRect>& outline_rects,
    const ComputedStyle& style) {
  Vector<IntRect> pixel_snapped_outline_rects;
  for (auto& r : outline_rects)
    pixel_snapped_outline_rects.push_back(PixelSnappedIntRect(r));

  Color color = style.VisitedDependentColor(GetCSSPropertyOutlineColor());
  if (style.OutlineStyleIsAuto()) {
    // Logic in draw focus ring is dependent on whether the border is large
    // enough to have an inset outline. Use the smallest border edge for that
    // test.
    float min_border_width =
        std::min(std::min(style.BorderTopWidth(), style.BorderBottomWidth()),
                 std::min(style.BorderLeftWidth(), style.BorderRightWidth()));
    float border_radius = GetFocusRingBorderRadius(style);
    context.DrawFocusRing(pixel_snapped_outline_rects,
                          style.GetOutlineStrokeWidthForFocusRing(),
                          style.OutlineOffsetInt(), border_radius,
                          min_border_width, color, style.UsedColorScheme());
    return;
  }

  IntRect united_outline_rect = UnionRect(pixel_snapped_outline_rects);
  if (united_outline_rect == pixel_snapped_outline_rects[0]) {
    PaintSingleRectangleOutline(context, united_outline_rect, style, color);
    return;
  }
  PaintComplexOutline(context, pixel_snapped_outline_rects, style, color);
}

}  // namespace blink
