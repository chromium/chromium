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

float GetFocusRingCornerRadius(const ComputedStyle& style,
                               const PhysicalRect& reference_border_rect) {
  if (style.HasBorderRadius() &&
      (!style.HasEffectiveAppearance() || style.HasAuthorBorderRadius())) {
    int outset = style.OutlineOffsetInt();
    FloatRoundedRect rect =
        RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
            style, reference_border_rect,
            LayoutRectOutsets(outset, outset, outset, outset));
    // For now we only support uniform border radius for all corners of the
    // focus ring. Use the minimum radius of all corners to prevent the focus
    // ring from overlapping with the element, but not smaller than the default
    // border radius.
    const auto& radii = rect.GetRadii();
    return std::max(
        DefaultFocusRingCornerRadius(style),
        std::min({radii.TopLeft().Width(), radii.TopLeft().Height(),
                  radii.TopRight().Width(), radii.TopRight().Height(),
                  radii.BottomRight().Width(), radii.BottomRight().Height(),
                  radii.BottomLeft().Width(), radii.BottomLeft().Height()}));
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
      return corner_radius;
    }
  }

  return DefaultFocusRingCornerRadius(style);
}

void PaintSingleFocusRing(GraphicsContext& context,
                          const Vector<IntRect>& rects,
                          float width,
                          int offset,
                          float corner_radius,
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
    context.DrawFocusRingRect(SkRect::Make(focus_ring_region.getBounds()),
                              color, width, corner_radius);
  } else {
    SkPath path;
    if (focus_ring_region.getBoundaryPath(&path))
      context.DrawFocusRingPath(path, color, width, corner_radius);
  }
}

void PaintFocusRing(GraphicsContext& context,
                    const Vector<IntRect>& rects,
                    const ComputedStyle& style,
                    float corner_radius) {
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
                       offset + std::ceil(inner_ring_width), corner_radius,
                       outer_color);
  // Draw the inner ring using |outer_ring_width| (which should be wider than
  // the additional offset of the outer ring) over the outer ring to ensure no
  // gaps or AA artifacts.
  DCHECK_GE(outer_ring_width, std::ceil(inner_ring_width));
  PaintSingleFocusRing(context, rects, outer_ring_width, offset, corner_radius,
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
    float corner_radius = GetFocusRingCornerRadius(style, outline_rects[0]);
    PaintFocusRing(context, pixel_snapped_outline_rects, style, corner_radius);
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
