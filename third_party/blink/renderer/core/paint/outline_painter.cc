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

// Construct a clockwise path along the outer edge of the region covered by
// |rects| expanded by |outline_offset| (which can be negative and clamped by
// the rect size) and |additional_outset| (which should be non-negative).
bool ComputeRightAnglePath(SkPath& path,
                           const Vector<IntRect>& rects,
                           int outline_offset,
                           int additional_outset) {
  DCHECK_GE(additional_outset, 0);
  SkRegion region;
  for (auto& r : rects) {
    IntRect rect = r;
    rect.InflateX(AdjustedOutlineOffsetX(rect, outline_offset));
    rect.InflateY(AdjustedOutlineOffsetY(rect, outline_offset));
    rect.Inflate(additional_outset);
    region.op(rect, SkRegion::kUnion_Op);
  }
  return region.getBoundaryPath(&path);
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

// Iterate a right angle |path| by running |contour_action| on each contour.
// The path contains one or more contours each of which is like (kMove_Verb,
// kLine_Verb, ..., kClose_Verb). Each line must be either horizontal or
// vertical. Each pair of adjacent lines (including the last and the first)
// should either create a right angle or be in the same straight line.
template <typename Action>
void IterateRightAnglePath(const SkPath& path, const Action& contour_action) {
  SkPath::Iter iter(path, /*forceClose*/ true);
  SkPoint points[4];
  Vector<Line> lines;
  for (SkPath::Verb verb = iter.next(points); verb != SkPath::kDone_Verb;
       verb = iter.next(points)) {
    switch (verb) {
      case SkPath::kMove_Verb:
        DCHECK(lines.IsEmpty());
        break;
      case SkPath::kLine_Verb: {
        Line new_line{points[0], points[1]};
        if (lines.IsEmpty() || !MergeLineIfPossible(lines.back(), new_line)) {
          lines.push_back(new_line);
          DCHECK(lines.size() == 1 ||
                 lines.back().start == lines[lines.size() - 2].end);
        }
        break;
      }
      case SkPath::kClose_Verb: {
        if (lines.size() >= 4u) {
          if (MergeLineIfPossible(lines.back(), lines.front())) {
            lines.front() = lines.back();
            lines.pop_back();
          }
          DCHECK(lines.front().start == lines.back().end);
          DCHECK_GE(lines.size(), 4u);
          contour_action(lines);
        }
        lines.clear();
        break;
      }
      default:
        NOTREACHED();
    }
  }
}

void PaintComplexRightAngleOutlineContour(GraphicsContext& context,
                                          const Vector<Line>& lines,
                                          const ComputedStyle& style,
                                          Color color) {
  int width = style.OutlineWidthInt();
  Vector<OutlineEdgeInfo> edges;
  edges.ReserveInitialCapacity(lines.size());
  for (auto& line : lines) {
    auto& edge = edges.emplace_back();
    edge.x1 = SkScalarTruncToInt(line.start.x());
    edge.y1 = SkScalarTruncToInt(line.start.y());
    edge.x2 = SkScalarTruncToInt(line.end.x());
    edge.y2 = SkScalarTruncToInt(line.end.y());
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

  int first_adjacent_width = AdjustJoint(width, edges.back(), edges.front());
  // The width of the angled part of starting and ending joint of the current
  // edge.
  int adjacent_width_start = first_adjacent_width;
  int adjacent_width_end;
  for (wtf_size_t i = 0; i < edges.size(); ++i) {
    OutlineEdgeInfo& edge = edges[i];
    adjacent_width_end = i == edges.size() - 1
                             ? first_adjacent_width
                             : AdjustJoint(width, edge, edges[i + 1]);
    int adjacent_width1 = adjacent_width_start;
    int adjacent_width2 = adjacent_width_end;
    if (edge.side == BoxSide::kLeft || edge.side == BoxSide::kBottom)
      std::swap(adjacent_width1, adjacent_width2);
    BoxBorderPainter::DrawLineForBoxSide(
        context, edge.x1, edge.y1, edge.x2, edge.y2, edge.side, color,
        style.OutlineStyle(), adjacent_width1, adjacent_width2,
        /*antialias*/ false);
    adjacent_width_start = adjacent_width_end;
  }
}

void PaintComplexRightAngleOutline(GraphicsContext& context,
                                   const Vector<IntRect>& rects,
                                   const ComputedStyle& style) {
  DCHECK(!style.OutlineStyleIsAuto());

  SkPath path;
  if (!ComputeRightAnglePath(path, rects, style.OutlineOffsetInt(),
                             style.OutlineWidthInt())) {
    return;
  }

  Color color = style.VisitedDependentColor(GetCSSPropertyOutlineColor());
  bool use_transparency_layer = color.HasAlpha();
  if (use_transparency_layer) {
    context.BeginLayer(static_cast<float>(color.Alpha()) / 255);
    color.SetRGB(color.Red(), color.Green(), color.Blue());
  }

  IterateRightAnglePath(path, [&](const Vector<Line>& lines) {
    PaintComplexRightAngleOutlineContour(context, lines, style, color);
  });

  if (use_transparency_layer)
    context.EndLayer();
}

// Given 3 points defining a right angle corner, returns |p2| shifted to make
// the containing path shrink by |inset|.
SkPoint ShrinkCorner(const SkPoint& p1,
                     const SkPoint& p2,
                     const SkPoint& p3,
                     int inset) {
  if (p1.x() == p2.x()) {
    if (p1.y() < p2.y()) {
      return p2.x() < p3.x() ? p2 + SkVector::Make(-inset, inset)
                             : p2 + SkVector::Make(-inset, -inset);
    }
    return p2.x() < p3.x() ? p2 + SkVector::Make(inset, inset)
                           : p2 + SkVector::Make(inset, -inset);
  }
  if (p1.x() < p2.x()) {
    return p2.y() < p3.y() ? p2 + SkVector::Make(-inset, inset)
                           : p2 + SkVector::Make(inset, inset);
  }
  return p2.y() < p3.y() ? p2 + SkVector::Make(-inset, -inset)
                         : p2 + SkVector::Make(inset, -inset);
}

void ShrinkRightAnglePath(SkPath& path, int inset) {
  SkPath input;
  std::swap(input, path);
  IterateRightAnglePath(input, [&path, inset](const Vector<Line>& lines) {
    for (wtf_size_t i = 0; i < lines.size(); i++) {
      const SkPoint& prev_point =
          lines[i == 0 ? lines.size() - 1 : i - 1].start;
      SkPoint new_point =
          ShrinkCorner(prev_point, lines[i].start, lines[i].end, inset);
      if (i == 0) {
        path.moveTo(new_point);
      } else {
        path.lineTo(new_point);
      }
    }
    path.close();
  });
}

FloatRoundedRect::Radii ComputeCornerRadii(
    const ComputedStyle& style,
    const PhysicalRect& reference_border_rect,
    float offset) {
  return RoundedBorderGeometry::PixelSnappedRoundedBorderWithOutsets(
             style, reference_border_rect,
             LayoutRectOutsets(offset, offset, offset, offset))
      .GetRadii();
}

// Given 3 points defining a right angle corner, returns the corresponding
// corner in |convex_radii| or |concave_radii|.
FloatSize GetRadiiCorner(const FloatRoundedRect::Radii& convex_radii,
                         const FloatRoundedRect::Radii& concave_radii,
                         const SkPoint& p1,
                         const SkPoint& p2,
                         const SkPoint& p3) {
  if (p1.x() == p2.x()) {
    if (p1.y() == p2.y() || p2.x() == p3.x())
      return FloatSize();
    DCHECK_EQ(p2.y(), p3.y());
    if (p1.y() < p2.y()) {
      return p2.x() < p3.x() ? concave_radii.BottomLeft()
                             : convex_radii.BottomRight();
    }
    return p2.x() < p3.x() ? convex_radii.TopLeft() : concave_radii.TopRight();
  }
  DCHECK_EQ(p1.y(), p2.y());
  if (p2.x() != p3.x() || p2.y() == p3.y())
    return FloatSize();
  if (p1.x() < p2.x()) {
    return p2.y() < p3.y() ? convex_radii.TopRight()
                           : concave_radii.BottomRight();
  }
  return p2.y() < p3.y() ? concave_radii.TopLeft() : convex_radii.BottomLeft();
}

// Shorten |line| between rounded corners.
void AdjustLineBetweenCorners(Line& line,
                              const FloatRoundedRect::Radii& convex_radii,
                              const FloatRoundedRect::Radii& concave_radii,
                              const SkPoint& prev_point,
                              const SkPoint& next_point) {
  FloatSize corner1 = GetRadiiCorner(convex_radii, concave_radii, prev_point,
                                     line.start, line.end);
  FloatSize corner2 = GetRadiiCorner(convex_radii, concave_radii, line.start,
                                     line.end, next_point);
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

// Create a rounded path from a right angle |path| by
// - inserting arc segments for corners;
// - adjusting length of the lines.
void AddCornerRadiiToPath(SkPath& path,
                          const FloatRoundedRect::Radii& convex_radii,
                          const FloatRoundedRect::Radii& concave_radii) {
  SkPath input;
  input.swap(path);
  IterateRightAnglePath(input, [&](const Vector<Line>& lines) {
    auto new_lines = lines;
    for (wtf_size_t i = 0; i < lines.size(); i++) {
      const SkPoint& prev_point =
          lines[i == 0 ? lines.size() - 1 : i - 1].start;
      const SkPoint& next_point = lines[i == lines.size() - 1 ? 0 : i + 1].end;
      AdjustLineBetweenCorners(new_lines[i], convex_radii, concave_radii,
                               prev_point, next_point);
    }
    // Generate the new contour into |path|.
    DCHECK_EQ(lines.size(), new_lines.size());
    path.moveTo(new_lines[0].start);
    for (wtf_size_t i = 0; i < new_lines.size(); i++) {
      const Line& line = new_lines[i];
      if (line.end != line.start)
        path.lineTo(line.end);
      const Line& next_line = new_lines[i == lines.size() - 1 ? 0 : i + 1];
      if (line.end != next_line.start) {
        constexpr float kCornerConicWeight = 0.707106781187;  // 1/sqrt(2)
        // This produces a 90 degree arc from line.end towards lines[i].end
        // to next_line.start.
        path.conicTo(lines[i].end, next_line.start, kCornerConicWeight);
      }
    }
    path.close();
  });
}

class ComplexRoundedOutlinePainter {
 public:
  ComplexRoundedOutlinePainter(GraphicsContext& context,
                               const Vector<IntRect>& rects,
                               const PhysicalRect& reference_border_rect,
                               const ComputedStyle& style)
      : context_(context),
        rects_(rects),
        reference_border_rect_(reference_border_rect),
        style_(style),
        outline_style_(style.OutlineStyle()),
        offset_(style.OutlineOffsetInt()),
        width_(style.OutlineWidthInt()),
        color_(style.VisitedDependentColor(GetCSSPropertyOutlineColor())) {
    DCHECK(!style.OutlineStyleIsAuto());
    if (width_ <= 2 && outline_style_ == EBorderStyle::kDouble) {
      outline_style_ = EBorderStyle::kSolid;
    } else if (width_ == 1 && (outline_style_ == EBorderStyle::kRidge ||
                               outline_style_ == EBorderStyle::kGroove)) {
      outline_style_ = EBorderStyle::kSolid;
      Color dark = color_.Dark();
      color_ = Color((color_.Red() + dark.Red()) / 2,
                     (color_.Green() + dark.Green()) / 2,
                     (color_.Blue() + dark.Blue()) / 2, color_.Alpha());
    }
  }

  bool Paint() {
    if (width_ == 0)
      return true;

    if (!ComputeRightAnglePath(right_angle_outer_path_, rects_, offset_,
                               width_)) {
      return true;
    }

    SkPath outer_path = right_angle_outer_path_;
    SkPath inner_path = right_angle_outer_path_;
    ShrinkRightAnglePath(inner_path, width_);
    auto inner_radii = ComputeRadii(0);
    auto outer_radii = ComputeRadii(width_);
    AddCornerRadiiToPath(outer_path, outer_radii, inner_radii);
    AddCornerRadiiToPath(inner_path, inner_radii, outer_radii);

    GraphicsContextStateSaver saver(context_);
    context_.ClipPath(outer_path, kAntiAliased);
    context_.ClipOut(inner_path);
    context_.SetFillColor(color_);

    switch (outline_style_) {
      case EBorderStyle::kSolid:
        context_.FillRect(outer_path.getBounds());
        break;
      case EBorderStyle::kDouble:
        PaintDoubleOutline();
        break;
      case EBorderStyle::kDotted:
      case EBorderStyle::kDashed:
        PaintDottedOrDashedOutline();
        break;
      default:
        // TODO(wangxianzhu): Draw kRidge, kGroove, kInset, kOutset by calling
        // BoxBorderPainter::DrawBoxSideFromPath() for each segment of the path.
        return false;
    }
    return true;
  }

 private:
  void PaintDoubleOutline() {
    SkPath inner_third_path = right_angle_outer_path_;
    SkPath outer_third_path = right_angle_outer_path_;
    int stroke_width = std::round(width_ / 3.0);
    ShrinkRightAnglePath(inner_third_path, width_ - stroke_width);
    ShrinkRightAnglePath(outer_third_path, stroke_width);
    auto inner_third_radii = ComputeRadii(stroke_width);
    auto outer_third_radii = ComputeRadii(width_ - stroke_width);
    AddCornerRadiiToPath(inner_third_path, inner_third_radii,
                         outer_third_radii);
    AddCornerRadiiToPath(outer_third_path, outer_third_radii,
                         inner_third_radii);
    {
      GraphicsContextStateSaver saver(context_);
      context_.ClipOut(outer_third_path);
      context_.FillRect(right_angle_outer_path_.getBounds());
    }
    context_.FillPath(inner_third_path);
  }

  void PaintDottedOrDashedOutline() {
    SkPath center_path = right_angle_outer_path_;
    int center_outset = width_ / 2;
    ShrinkRightAnglePath(center_path, width_ - center_outset);
    auto center_radii = ComputeRadii(center_outset);
    AddCornerRadiiToPath(center_path, center_radii, center_radii);
    context_.SetStrokeColor(color_);
    auto stroke_style =
        outline_style_ == EBorderStyle::kDashed ? kDashedStroke : kDottedStroke;
    context_.SetStrokeStyle(stroke_style);
    if (StrokeData::StrokeIsDashed(width_, stroke_style)) {
      // Draw wider to fill the clip area between inner_path_ and outer_path_,
      // to get smoother edges, and even stroke thickness when the outline is
      // thin.
      context_.SetStrokeThickness(width_ + 2);
    } else {
      context_.SetStrokeThickness(width_);
      context_.SetLineCap(kRoundCap);
    }
    context_.StrokePath(center_path, Path(center_path).length(), width_);
  }

  FloatRoundedRect::Radii ComputeRadii(int outset) const {
    return ComputeCornerRadii(style_, reference_border_rect_, offset_ + outset);
  }

  GraphicsContext& context_;
  const Vector<IntRect>& rects_;
  const PhysicalRect& reference_border_rect_;
  const ComputedStyle& style_;
  EBorderStyle outline_style_;
  int offset_;
  int width_;
  Color color_;
  SkPath right_angle_outer_path_;
};

float DefaultFocusRingCornerRadius(const ComputedStyle& style) {
  // Default style is corner radius equal to outline width.
  return style.FocusRingStrokeWidth();
}

FloatRoundedRect::Radii GetFocusRingCornerRadii(
    const ComputedStyle& style,
    const PhysicalRect& reference_border_rect) {
  if (style.HasBorderRadius() &&
      (!style.HasEffectiveAppearance() || style.HasAuthorBorderRadius())) {
    auto radii = ComputeCornerRadii(style, reference_border_rect,
                                    style.OutlineOffsetInt());
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

void PaintSingleFocusRing(GraphicsContext& context,
                          const Vector<IntRect>& rects,
                          float width,
                          int offset,
                          const FloatRoundedRect::Radii& corner_radii,
                          const Color& color) {
  DCHECK(!rects.IsEmpty());
  SkPath path;
  if (!ComputeRightAnglePath(path, rects, offset, 0))
    return;

  SkRect rect;
  if (path.isRect(&rect)) {
    context.DrawFocusRingRect(FloatRoundedRect(rect, corner_radii), color,
                              width);
    return;
  }

  absl::optional<float> corner_radius = corner_radii.UniformRadius();
  if (corner_radius.has_value()) {
    context.DrawFocusRingPath(path, color, width, *corner_radius);
    return;
  }

  // Bake non-uniform radii into the path, and draw the path with 0 corner
  // radius as the path already has rounded corners.
  AddCornerRadiiToPath(path, corner_radii, corner_radii);
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
  for (auto& r : outline_rects) {
    IntRect pixel_snapped_rect = PixelSnappedIntRect(r);
    // Keep empty rect for normal outline, but not for focus rings.
    if (!pixel_snapped_rect.IsEmpty() || !style.OutlineStyleIsAuto())
      pixel_snapped_outline_rects.push_back(pixel_snapped_rect);
  }
  if (pixel_snapped_outline_rects.IsEmpty())
    return;

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

  if (style.HasBorderRadius() &&
      ComplexRoundedOutlinePainter(context, pixel_snapped_outline_rects,
                                   outline_rects[0], style)
          .Paint()) {
    return;
  }

  PaintComplexRightAngleOutline(context, pixel_snapped_outline_rects, style);
}

void OutlinePainter::PaintFocusRingPath(GraphicsContext& context,
                                        const Path& focus_ring_path,
                                        const ComputedStyle& style) {
  // TODO(crbug/251206): Implement outline-offset and double focus rings like
  // right angle focus rings, which requires SkPathOps to support expanding and
  // shrinking generic paths.
  context.DrawFocusRingPath(
      focus_ring_path.GetSkPath(),
      style.VisitedDependentColor(GetCSSPropertyOutlineColor()),
      style.FocusRingStrokeWidth(), DefaultFocusRingCornerRadius(style));
}

}  // namespace blink
