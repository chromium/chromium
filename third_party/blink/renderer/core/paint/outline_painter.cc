// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/outline_painter.h"

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
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

// A negative outline-offset should not cause the rendered outline shape to
// become smaller than twice the computed value of the outline-width, in each
// direction separately. See: https://drafts.csswg.org/css-ui/#outline-offset
int AdjustedOutlineOffsetX(const IntRect& rect, int offset) {
  return std::max(offset, -rect.Width() / 2);
}
int AdjustedOutlineOffsetY(const IntRect& rect, int offset) {
  return std::max(offset, -rect.Height() / 2);
}

void ApplyOutlineOffset(IntRect& rect, int offset) {
  rect.InflateX(AdjustedOutlineOffsetX(rect, offset));
  rect.InflateY(AdjustedOutlineOffsetY(rect, offset));
}

void PaintComplexOutline(GraphicsContext& graphics_context,
                         const Vector<IntRect> rects,
                         const ComputedStyle& style) {
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

  Color color = style.VisitedDependentColor(GetCSSPropertyOutlineColor());
  bool use_transparency_layer = color.HasAlpha();
  if (use_transparency_layer) {
    graphics_context.BeginLayer(static_cast<float>(color.Alpha()) / 255);
    color.SetRGB(color.Red(), color.Green(), color.Blue());
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
        graphics_context, edge.x1, edge.y1, edge.x2, edge.y2, edge.side, color,
        style.OutlineStyle(), adjacent_width1, adjacent_width2,
        /*antialias*/ false);
    adjacent_width_start = adjacent_width_end;
  }

  if (use_transparency_layer)
    graphics_context.EndLayer();
}

float DefaultFocusRingCornerRadius(const ComputedStyle& style) {
  // Default style is corner radius equal to outline width.
  return style.FocusRingStrokeWidth();
}

FloatRoundedRect::Radii GetFocusRingCornerRadii(
    const ComputedStyle& style,
    const PhysicalRect& reference_border_rect) {
  if (style.HasBorderRadius() &&
      (!style.HasEffectiveAppearance() || style.HasAuthorBorderRadius())) {
    int outset = style.OutlineOffsetInt();
    FloatRoundedRect rect =
        RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
            style, reference_border_rect,
            LayoutRectOutsets(outset, outset, outset, outset));
    auto radii = rect.GetRadii();
    radii.SetMinimumRadius(DefaultFocusRingCornerRadius(style));
    return radii;
  }

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
      float corner_radius =
          ui::NativeTheme::GetInstanceForWeb()->GetBorderRadiusForPart(
              part.value(), style.Width().GetFloatValue(),
              style.Height().GetFloatValue());
      corner_radius =
          ui::NativeTheme::GetInstanceForWeb()->AdjustBorderRadiusByZoom(
              part.value(), corner_radius, style.EffectiveZoom());
      return FloatRoundedRect::Radii(corner_radius);
    }
  }

  return FloatRoundedRect::Radii(DefaultFocusRingCornerRadius(style));
}

// Given 3 points defining a corner, returns the corresponding corner in
// |radii|.
FloatSize GetRadiiCorner(const FloatRoundedRect::Radii& radii,
                         const SkPoint& p1,
                         const SkPoint& p2,
                         const SkPoint& p3) {
  if (p1.x() == p2.x()) {
    DCHECK_NE(p1.y(), p2.y());
    DCHECK_NE(p2.x(), p3.x());
    DCHECK_EQ(p2.y(), p3.y());
    if (p1.y() < p2.y())
      return p2.x() < p3.x() ? radii.BottomLeft() : radii.BottomRight();
    return p2.x() < p3.x() ? radii.TopLeft() : radii.TopRight();
  }
  DCHECK_EQ(p1.y(), p2.y());
  DCHECK_EQ(p2.x(), p3.x());
  DCHECK_NE(p2.y(), p3.y());
  if (p1.x() < p2.x())
    return p2.y() < p3.y() ? radii.TopRight() : radii.BottomRight();
  return p2.y() < p3.y() ? radii.TopLeft() : radii.BottomLeft();
}

SkPathDirection CornerArcDirection(const FloatRoundedRect::Radii& radii,
                                   const SkPoint& p1,
                                   const SkPoint& p2,
                                   const SkPoint& p3) {
  if (p1.x() == p2.x()) {
    return (p1.y() < p2.y()) == (p2.x() < p3.x()) ? SkPathDirection::kCCW
                                                  : SkPathDirection::kCW;
  }
  return (p1.x() < p2.x()) == (p2.y() < p3.y()) ? SkPathDirection::kCW
                                                : SkPathDirection::kCCW;
}

struct Line {
  SkPoint start;
  SkPoint end;
};

// Merge line2 into line1 if they are in the same straight line.
bool MergeLineIfPossible(Line& line1, const Line& line2) {
  DCHECK(line1.end == line2.start);
  if ((line1.start.x() == line1.end.x() && line1.start.x() == line2.end.x()) ||
      (line1.start.y() == line1.end.y() && line1.start.y() == line2.end.y())) {
    line1.end = line2.end;
    return true;
  }
  return false;
}

// Shorten |line| between rounded corners.
void AdjustLineBetweenCorners(Line& line,
                              const FloatRoundedRect::Radii& radii,
                              const SkPoint& prev_point,
                              const SkPoint& next_point) {
  FloatSize corner1 = GetRadiiCorner(radii, prev_point, line.start, line.end);
  FloatSize corner2 = GetRadiiCorner(radii, line.start, line.end, next_point);
  if (line.start.x() == line.end.x()) {
    // |line| is vertical, and adjacent lines are horizontal.
    float height = std::abs(line.end.y() - line.start.y());
    float corner1_height = corner1.Height();
    float corner2_height = corner2.Height();
    if (corner1_height + corner2_height > height) {
      // Scale down the corner heights to make the corners fit in |height|.
      float scale = height / (corner1_height + corner2_height);
      corner1_height = floorf(corner1_height * scale);
      corner2_height = floorf(corner2_height * scale);
    }
    if (line.start.y() < line.end.y()) {
      line.start.offset(0, corner1_height);
      line.end.offset(0, -corner2_height);
    } else {
      line.start.offset(0, -corner1_height);
      line.end.offset(0, corner2_height);
    }
  } else {
    // |line| is horizontal, and adjacent lines are vertical.
    float width = std::abs(line.end.x() - line.start.x());
    float corner1_width = corner1.Width();
    float corner2_width = corner2.Width();
    if (corner1_width + corner2_width > width) {
      // Scale down the corner widths to make the corners fit in |width|.
      float scale = width / (corner1_width + corner2_width);
      corner1_width = floorf(corner1_width * scale);
      corner2_width = floorf(corner2_width * scale);
    }
    if (line.start.x() < line.end.x()) {
      line.start.offset(corner1_width, 0);
      line.end.offset(-corner2_width, 0);
    } else {
      line.start.offset(-corner1_width, 0);
      line.end.offset(corner2_width, 0);
    }
  }
}

// Create a rounded path from |path| containing right angle lines by
// - inserting arc segments for corners;
// - adjusting length of the lines.
void AddCornerRadiiToPath(SkPath& path, const FloatRoundedRect::Radii& radii) {
  SkPath input;
  input.swap(path);

  // The path may contain multiple contours each of which is like (kMove_Verb,
  // kLine_Verb, ..., kClose_Verb). Each line must be either horizontal or
  // vertical. Two adjacent lines create a right angle.
  SkPath::Iter iter(input, /*forceClose*/ true);
  SkPoint points[4];  // for iter.next().
  Vector<Line> lines;
  for (SkPath::Verb verb = iter.next(points); verb != SkPath::kDone_Verb;
       verb = iter.next(points)) {
    switch (verb) {
      case SkPath::kMove_Verb:
        DCHECK(lines.IsEmpty());
        break;
      case SkPath::kLine_Verb: {
        Line new_line{points[0], points[1]};
        if (lines.IsEmpty() || !MergeLineIfPossible(lines.back(), new_line))
          lines.push_back(new_line);
        break;
      }
      case SkPath::kClose_Verb: {
        DCHECK_GE(lines.size(), 4u);
        if (MergeLineIfPossible(lines.back(), lines.front())) {
          lines.front() = lines.back();
          lines.pop_back();
        }
        Vector<SkPathDirection> arc_directions(lines.size());
        // Save the first line before adjustment because we may adjust the line
        // to zero length which loses direction. Will be used to adjust the last
        // line.
        Line original_first_line = lines.front();
        for (wtf_size_t i = 0; i < lines.size(); i++) {
          const SkPoint& prev_point =
              lines[i == 0 ? lines.size() - 1 : i - 1].start;
          const Line& next_line =
              i == lines.size() - 1 ? original_first_line : lines[i + 1];
          DCHECK(lines[i].end == next_line.start);
          arc_directions[i] = CornerArcDirection(
              radii, lines[i].start, next_line.start, next_line.end);
          AdjustLineBetweenCorners(lines[i], radii, prev_point, next_line.end);
        }
        // Generate the new contour into |result|.
        path.moveTo(lines[0].start);
        for (wtf_size_t i = 0; i < lines.size(); i++) {
          const Line& line = lines[i];
          if (line.end != line.start)
            path.lineTo(line.end);
          const Line& next_line = lines[i == lines.size() - 1 ? 0 : i + 1];
          if (line.end != next_line.start) {
            path.arcTo(std::abs(next_line.start.x() - line.end.x()),
                       std::abs(next_line.start.y() - line.end.y()), 0,
                       SkPath::kSmall_ArcSize, arc_directions[i],
                       next_line.start.x(), next_line.start.y());
          }
        }
        lines.clear();
        path.close();
        break;
      }
      default:
        NOTREACHED();
    }
  }
}

void PaintSingleFocusRing(GraphicsContext& context,
                          const Vector<IntRect>& rects,
                          float width,
                          int offset,
                          const FloatRoundedRect::Radii& corner_radii,
                          const Color& color) {
  unsigned rect_count = rects.size();
  if (!rect_count)
    return;

  SkRegion focus_ring_region;
  for (unsigned i = 0; i < rect_count; i++) {
    SkIRect r = rects[i];
    if (r.isEmpty())
      continue;
    r.outset(offset, offset);
    focus_ring_region.op(r, SkRegion::kUnion_Op);
  }

  if (focus_ring_region.isEmpty())
    return;

  if (focus_ring_region.isRect()) {
    context.DrawFocusRingRect(
        FloatRoundedRect(IntRect(focus_ring_region.getBounds()), corner_radii),
        color, width);
    return;
  }

  SkPath path;
  if (!focus_ring_region.getBoundaryPath(&path))
    return;
  absl::optional<float> corner_radius = corner_radii.UniformRadius();
  if (corner_radius.has_value()) {
    context.DrawFocusRingPath(path, color, width, *corner_radius);
    return;
  }

  // Bake non-uniform radii into the path, and draw the path with 0 corner
  // radius as the path already has rounded corners.
  AddCornerRadiiToPath(path, corner_radii);
  context.DrawFocusRingPath(path, color, width, 0);
}

void PaintFocusRing(GraphicsContext& context,
                    const Vector<IntRect>& rects,
                    const ComputedStyle& style,
                    const FloatRoundedRect::Radii& corner_radii) {
  Color inner_color = style.VisitedDependentColor(GetCSSPropertyOutlineColor());
#if !defined(OS_MAC)
  if (style.DarkColorScheme())
    inner_color = Color::kWhite;
#endif

  const float outer_ring_width = style.FocusRingOuterStrokeWidth();
  const float inner_ring_width = style.FocusRingInnerStrokeWidth();
  const int offset = style.FocusRingOffset();
  Color outer_color =
      style.DarkColorScheme() ? Color(0x10, 0x10, 0x10) : Color::kWhite;
  PaintSingleFocusRing(context, rects, outer_ring_width,
                       offset + std::ceil(inner_ring_width), corner_radii,
                       outer_color);
  // Draw the inner ring using |outer_ring_width| (which should be wider than
  // the additional offset of the outer ring) over the outer ring to ensure no
  // gaps or AA artifacts.
  DCHECK_GE(outer_ring_width, std::ceil(inner_ring_width));
  PaintSingleFocusRing(context, rects, outer_ring_width, offset, corner_radii,
                       inner_color);
}

}  // anonymous namespace

void OutlinePainter::PaintOutlineRects(
    GraphicsContext& context,
    const Vector<PhysicalRect>& outline_rects,
    const ComputedStyle& style) {
  Vector<IntRect> pixel_snapped_outline_rects;
  for (auto& r : outline_rects)
    pixel_snapped_outline_rects.push_back(PixelSnappedIntRect(r));

  if (style.OutlineStyleIsAuto()) {
    auto corner_radii = GetFocusRingCornerRadii(style, outline_rects[0]);
    PaintFocusRing(context, pixel_snapped_outline_rects, style, corner_radii);
    return;
  }

  IntRect united_outline_rect = UnionRect(pixel_snapped_outline_rects);
  if (united_outline_rect == pixel_snapped_outline_rects[0]) {
    BoxBorderPainter::PaintSingleRectOutline(
        context, style, outline_rects[0],
        AdjustedOutlineOffsetX(united_outline_rect, style.OutlineOffsetInt()),
        AdjustedOutlineOffsetY(united_outline_rect, style.OutlineOffsetInt()));
    return;
  }
  PaintComplexOutline(context, pixel_snapped_outline_rects, style);
}

void OutlinePainter::PaintFocusRingPath(GraphicsContext& context,
                                        const Path& focus_ring_path,
                                        const ComputedStyle& style) {
  // TODO(wangxianzhu):
  // 1. Implement support for offset.
  // 2. Implement double focus rings like rectangular focus rings.
  context.DrawFocusRingPath(
      focus_ring_path.GetSkPath(),
      style.VisitedDependentColor(GetCSSPropertyOutlineColor()),
      style.FocusRingStrokeWidth(), DefaultFocusRingCornerRadius(style));
}

}  // namespace blink
