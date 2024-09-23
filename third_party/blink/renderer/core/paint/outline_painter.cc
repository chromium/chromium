// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/outline_painter.h"

#include <optional>

#include "build/build_config.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/paint/box_border_painter.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/rounded_border_geometry.h"
#include "third_party/blink/renderer/core/style/border_edge.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

namespace {

float FocusRingStrokeWidth(const ComputedStyle& style) {
  DCHECK(style.OutlineStyleIsAuto());
  // Draw focus ring with thickness in proportion to the zoom level, but never
  // so narrow that it becomes invisible.
  float width = 3.f;
  if (style.EffectiveZoom() >= 1.0f) {
    width = ui::NativeTheme::GetInstanceForWeb()->AdjustBorderWidthByZoom(
        width, style.EffectiveZoom());
    DCHECK_GE(width, 3.f);
  }
  return std::max(style.EffectiveZoom(), width);
}

float FocusRingOuterStrokeWidth(const ComputedStyle& style) {
  // The focus ring is made of two rings which have a 2:1 ratio.
  return FocusRingStrokeWidth(style) / 3.f * 2;
}

float FocusRingInnerStrokeWidth(const ComputedStyle& style) {
  return FocusRingStrokeWidth(style) / 3.f;
}

int FocusRingOffset(const ComputedStyle& style,
                    const LayoutObject::OutlineInfo& info) {
  DCHECK(style.OutlineStyleIsAuto());
  // How much space the focus ring would like to take from the actual border.
  const float max_inside_border_width =
      ui::NativeTheme::GetInstanceForWeb()->AdjustBorderWidthByZoom(
          1.0f, style.EffectiveZoom());
  int offset = info.offset;
  // Focus ring is dependent on whether the border is large enough to have an
  // inset outline. Use the smallest border edge for that test.
  float min_border_width =
      std::min({style.BorderTopWidth(), style.BorderBottomWidth(),
                style.BorderLeftWidth(), style.BorderRightWidth()});
  if (min_border_width >= max_inside_border_width)
    offset -= max_inside_border_width;
  return offset;
}

// A negative outline-offset should not cause the rendered outline shape to
// become smaller than twice the computed value of the outline-width, in each
// direction separately. See: https://drafts.csswg.org/css-ui/#outline-offset
gfx::Outsets AdjustedOutlineOffset(const gfx::Rect& rect, int offset) {
  return gfx::Outsets::VH(std::max(offset, -rect.height() / 2),
                          std::max(offset, -rect.width() / 2));
}

// Construct a clockwise path along the outer edge of the region covered by
// |rects| expanded by |outline_offset| (which can be negative and clamped by
// the rect size) and |additional_outset| (which should be non-negative).
bool ComputeRightAnglePath(SkPath& path,
                           const Vector<gfx::Rect>& rects,
                           int outline_offset,
                           int additional_outset) {
  DCHECK_GE(additional_outset, 0);
  SkRegion region;
  for (auto& r : rects) {
    gfx::Rect rect = r;
    rect.Outset(AdjustedOutlineOffset(rect, outline_offset));
    rect.Outset(additional_outset);
    region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
  }
  return region.getBoundaryPath(&path);
}

using Line = OutlinePainter::Line;

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
        DCHECK(lines.empty());
        break;
      case SkPath::kLine_Verb: {
        Line new_line{points[0], points[1]};
        if (lines.empty() || !MergeLineIfPossible(lines.back(), new_line)) {
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
          // lines.size() < 4 means that the contour is collapsed (i.e. the area
          // in the contour is empty). Ignore it.
          if (lines.size() >= 4u)
            contour_action(lines);
        }
        lines.clear();
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

// Given 3 points defining a right angle corner, returns |p2| shifted to make
// the containing path shrunk by |inset|.
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
             style, reference_border_rect, PhysicalBoxStrut(LayoutUnit(offset)))
      .GetRadii();
}

// Given 3 points defining a right angle corner, returns the corresponding
// corner in |convex_radii| or |concave_radii|.
gfx::SizeF GetRadiiCorner(const FloatRoundedRect::Radii& convex_radii,
                          const FloatRoundedRect::Radii& concave_radii,
                          const SkPoint& p1,
                          const SkPoint& p2,
                          const SkPoint& p3) {
  if (p1.x() == p2.x()) {
    if (p1.y() == p2.y() || p2.x() == p3.x())
      return gfx::SizeF();
    DCHECK_EQ(p2.y(), p3.y());
    if (p1.y() < p2.y()) {
      return p2.x() < p3.x() ? concave_radii.BottomLeft()
                             : convex_radii.BottomRight();
    }
    return p2.x() < p3.x() ? convex_radii.TopLeft() : concave_radii.TopRight();
  }
  DCHECK_EQ(p1.y(), p2.y());
  if (p2.x() != p3.x() || p2.y() == p3.y())
    return gfx::SizeF();
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
  gfx::SizeF corner1 = GetRadiiCorner(convex_radii, concave_radii, prev_point,
                                      line.start, line.end);
  gfx::SizeF corner2 = GetRadiiCorner(convex_radii, concave_radii, line.start,
                                      line.end, next_point);
  if (line.start.x() == line.end.x()) {
    // |line| is vertical, and adjacent lines are horizontal.
    float height = std::abs(line.end.y() - line.start.y());
    float corner1_height = corner1.height();
    float corner2_height = corner2.height();
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
    float corner1_width = corner1.width();
    float corner2_width = corner2.width();
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

// The weight of SkPath::conicTo() to create a 90deg rounded corner arc.
constexpr float kCornerConicWeight = 0.707106781187;  // 1/sqrt(2)

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
    path.moveTo(new_lines.back().end);
    for (wtf_size_t i = 0; i < new_lines.size(); i++) {
      // Keep empty arcs and lines to allow RoundedEdgePathIterator to match
      // edges. Produce a 90 degree arc from the current point (end of the
      // previous line) towards lines[i].start to new_lines[i].start.
      path.conicTo(lines[i].start, new_lines[i].start, kCornerConicWeight);
      path.lineTo(new_lines[i].end);
    }
    path.close();
  });
}

// Move |point| so that the length of the line to |other| will be extended by
// |offset|.
void ExtendLineAtEndpoint(SkPoint& point, const SkPoint& other, int offset) {
  if (point.x() == other.x()) {
    point.offset(0, point.y() < other.y() ? -offset : offset);
  } else {
    DCHECK_EQ(point.y(), other.y());
    point.offset(point.x() < other.x() ? -offset : offset, 0);
  }
}

// Iterates a rounded outline center path, and for each edge [1] returns the
// path that can be used to stroke the edge.
// [1] An "edge" means a segment of the path, including a horizontal or vertical
// line and approximate halves of its adjacent arcs if any.
class RoundedEdgePathIterator {
  STACK_ALLOCATED();

 public:
  RoundedEdgePathIterator(const SkPath& rounded_center_path, int center_inset)
      : iter_(rounded_center_path, /*forceClose*/ true),
        center_inset_(center_inset) {}

  SkPath Next() {
    SkPath edge_stroke_path;
    while (true) {
      SkPoint points[4];
      switch (iter_.next(points)) {
        case SkPath::kConic_Verb:
          if (is_new_contour_) {
            std::copy_n(points, kArcPointCount, prev_arc_points_);
            std::copy_n(points, kArcPointCount, first_arc_points_);
            is_new_contour_ = false;
            continue;
          }
          GenerateEdgeStrokePath(edge_stroke_path, prev_arc_points_, points);
          std::copy_n(points, kArcPointCount, prev_arc_points_);
          return edge_stroke_path;
        case SkPath::kClose_Verb:
          DCHECK(!is_new_contour_);
          GenerateEdgeStrokePath(edge_stroke_path, prev_arc_points_,
                                 first_arc_points_);
          is_new_contour_ = true;
          return edge_stroke_path;
        case SkPath::kDone_Verb:
          return edge_stroke_path;
        default:
          continue;
      }
    }
  }

 private:
  // An example of an edge stroke path:
  // |             Short extension before the starting arc (see code comment)
  //  \            Starting arc
  //   \______     Line
  //          \    Ending arc
  //           |   Short extension after the ending arc (see code comment)
  // The edge will drawn with a clip to remove the first half of the starting
  // arc and the second half of the ending arc.
  void GenerateEdgeStrokePath(SkPath& edge_stroke_path,
                              base::span<const SkPoint> starting_arc_points,
                              base::span<const SkPoint> ending_arc_points) {
    SkPoint line_start = starting_arc_points[2];
    SkPoint line_end = ending_arc_points[0];
    if (starting_arc_points[0] == line_start) {
      // No starting arc. Extend the line to fill the miter.
      ExtendLineAtEndpoint(line_start, ending_arc_points[1], center_inset_);
      edge_stroke_path.moveTo(line_start);
    } else {
      SkPoint start = starting_arc_points[0];
      // Add a short line before the arc in case the starting arc is too short
      // to fill the miter.
      ExtendLineAtEndpoint(start, starting_arc_points[1], center_inset_);
      edge_stroke_path.moveTo(start);
      edge_stroke_path.lineTo(starting_arc_points[0]);
      edge_stroke_path.conicTo(starting_arc_points[1], line_start,
                               kCornerConicWeight);
    }
    if (line_end == ending_arc_points[2]) {
      // No ending arc. Extend the line to fill the miter.
      ExtendLineAtEndpoint(line_end, starting_arc_points[1], center_inset_);
      edge_stroke_path.lineTo(line_end);
    } else {
      edge_stroke_path.lineTo(line_end);
      SkPoint end = ending_arc_points[2];
      edge_stroke_path.conicTo(ending_arc_points[1], end, kCornerConicWeight);
      // Add a short line after the ending arc in case the arc is too short to
      // fill the miter.
      ExtendLineAtEndpoint(end, ending_arc_points[1], center_inset_);
      edge_stroke_path.lineTo(end);
    }
  }

  SkPath::Iter iter_;
  const int center_inset_;
  bool is_new_contour_ = true;
  // The three points are: start, control (the right-angle corner), end.
  static constexpr size_t kArcPointCount = 3;
  SkPoint first_arc_points_[kArcPointCount];
  SkPoint prev_arc_points_[kArcPointCount];
};

class ComplexOutlinePainter {
  STACK_ALLOCATED();

 public:
  ComplexOutlinePainter(GraphicsContext& context,
                        const Vector<gfx::Rect>& rects,
                        const PhysicalRect& reference_border_rect,
                        const ComputedStyle& style,
                        const LayoutObject::OutlineInfo& info)
      : context_(context),
        rects_(rects),
        reference_border_rect_(reference_border_rect),
        style_(style),
        outline_style_(style.OutlineStyle()),
        offset_(info.offset),
        width_(info.width),
        color_(style.VisitedDependentColor(GetCSSPropertyOutlineColor())),
        is_rounded_(style.HasBorderRadius()) {
    DCHECK(!style.OutlineStyleIsAuto());
    DCHECK_NE(width_, 0);
    if (width_ <= 2 && outline_style_ == EBorderStyle::kDouble) {
      outline_style_ = EBorderStyle::kSolid;
    } else if (width_ == 1 && (outline_style_ == EBorderStyle::kRidge ||
                               outline_style_ == EBorderStyle::kGroove)) {
      outline_style_ = EBorderStyle::kSolid;
      Color dark = color_.Dark();
      color_ = Color(
          (color_.Red() + dark.Red()) / 2, (color_.Green() + dark.Green()) / 2,
          (color_.Blue() + dark.Blue()) / 2, color_.AlphaAsInteger());
    }
  }

  void Paint() {
    if (!ComputeRightAnglePath(right_angle_outer_path_, rects_, offset_,
                               width_)) {
      return;
    }

    bool use_alpha_layer = !color_.IsOpaque() &&
                           outline_style_ != EBorderStyle::kSolid &&
                           outline_style_ != EBorderStyle::kDouble;
    if (use_alpha_layer) {
      context_.BeginLayer(color_.Alpha());
      color_ = Color::FromRGB(color_.Red(), color_.Green(), color_.Blue());
    }

    SkPath outer_path = right_angle_outer_path_;
    SkPath inner_path = right_angle_outer_path_;
    ShrinkRightAnglePath(inner_path, width_);
    if (is_rounded_) {
      auto inner_radii = ComputeRadii(0);
      auto outer_radii = ComputeRadii(width_);
      AddCornerRadiiToPath(outer_path, outer_radii, inner_radii);
      AddCornerRadiiToPath(inner_path, inner_radii, outer_radii);
    }

    GraphicsContextStateSaver saver(context_);
    context_.ClipPath(outer_path, kAntiAliased);
    MakeClipOutPath(inner_path);
    context_.ClipPath(inner_path, kAntiAliased);
    context_.SetFillColor(color_);

    switch (outline_style_) {
      case EBorderStyle::kSolid:
        context_.FillRect(
            gfx::SkRectToRectF(outer_path.getBounds()),
            PaintAutoDarkMode(style_,
                              DarkModeFilter::ElementRole::kBackground));
        break;
      case EBorderStyle::kDouble:
        PaintDoubleOutline();
        break;
      case EBorderStyle::kDotted:
      case EBorderStyle::kDashed:
        PaintDottedOrDashedOutline();
        break;
      case EBorderStyle::kGroove:
      case EBorderStyle::kRidge:
        PaintGrooveOrRidgeOutline();
        break;
      case EBorderStyle::kInset:
      case EBorderStyle::kOutset:
        PaintInsetOrOutsetOutline(CenterPath(),
                                  outline_style_ == EBorderStyle::kInset);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }

    if (use_alpha_layer)
      context_.EndLayer();
  }

 private:
  void PaintDoubleOutline() {
    SkPath inner_third_path = right_angle_outer_path_;
    SkPath outer_third_path = right_angle_outer_path_;
    int stroke_width = std::round(width_ / 3.0);
    ShrinkRightAnglePath(inner_third_path, width_ - stroke_width);
    ShrinkRightAnglePath(outer_third_path, stroke_width);
    if (is_rounded_) {
      auto inner_third_radii = ComputeRadii(stroke_width);
      auto outer_third_radii = ComputeRadii(width_ - stroke_width);
      AddCornerRadiiToPath(inner_third_path, inner_third_radii,
                           outer_third_radii);
      AddCornerRadiiToPath(outer_third_path, outer_third_radii,
                           inner_third_radii);
    }
    AutoDarkMode auto_dark_mode(
        PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));
    context_.FillPath(inner_third_path, auto_dark_mode);
    MakeClipOutPath(outer_third_path);
    context_.ClipPath(outer_third_path, kAntiAliased);
    context_.FillRect(gfx::SkRectToRectF(right_angle_outer_path_.getBounds()),
                      auto_dark_mode);
  }

  void PaintDottedOrDashedOutline() {
    auto stroke_style =
        outline_style_ == EBorderStyle::kDashed ? kDashedStroke : kDottedStroke;
    StyledStrokeData styled_stroke;
    styled_stroke.SetStyle(stroke_style);
    if ((width_ % 2) &&
        StyledStrokeData::StrokeIsDashed(width_, stroke_style)) {
      // If width_ is odd, draw wider to fill the clip area.
      styled_stroke.SetThickness(width_ + 2);
    } else {
      styled_stroke.SetThickness(width_);
    }
    context_.SetStrokeColor(color_);

    SkPath center_path = CenterPath();
    AutoDarkMode auto_dark_mode(
        PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));
    if (is_rounded_) {
      const Path path(center_path);
      const StrokeData stroke_data = styled_stroke.ConvertToStrokeData(
          {static_cast<int>(path.length()), width_, path.IsClosed()});
      context_.SetStroke(stroke_data);
      context_.StrokePath(path, auto_dark_mode);
    } else {
      // Draw edges one by one instead of the whole path to let the corners
      // have starting/ending dots/dashes.
      IterateRightAnglePath(
          center_path,
          [this, &styled_stroke, &auto_dark_mode](const Vector<Line>& lines) {
            for (const auto& line : lines) {
              PaintStraightEdge(line, styled_stroke, auto_dark_mode);
            }
          });
    }
  }

  void PaintGrooveOrRidgeOutline() {
    SkPath center_path = CenterPath();
    // Paint the whole outline, treating kGroove as kInset.
    PaintInsetOrOutsetOutline(center_path,
                              outline_style_ == EBorderStyle::kGroove);
    // Paint dark color in the inner half.
    context_.ClipPath(center_path, kAntiAliased);
    context_.SetStrokeColor(color_.Dark());
    PaintTopLeftOrBottomRight(center_path,
                              outline_style_ == EBorderStyle::kRidge);
    // Paint light color in the inner half. If width_ is odd, draw thinner
    // (by preferring outer half) because light color looks wider.
    if (width_ % 2) {
      SkPath center_path_prefer_outer = CenterPath(/*prefer_outer*/ true);
      context_.ClipPath(center_path_prefer_outer, kAntiAliased);
    }
    context_.SetStrokeColor(color_);
    PaintTopLeftOrBottomRight(center_path,
                              outline_style_ == EBorderStyle::kGroove);
  }

  void PaintInsetOrOutsetOutline(const SkPath& center_path, bool is_inset) {
    context_.SetStrokeColor(color_);
    PaintTopLeftOrBottomRight(center_path, !is_inset);
    context_.SetStrokeColor(color_.Dark());
    PaintTopLeftOrBottomRight(center_path, is_inset);
  }

  void PaintTopLeftOrBottomRight(const SkPath& center_path,
                                 bool top_left_or_bottom_right) {
    StyledStrokeData styled_stroke;
    // If width_ is odd, draw wider to fill the clip area.
    styled_stroke.SetThickness(width_ % 2 ? width_ + 2 : width_);
    std::optional<RoundedEdgePathIterator> rounded_edge_path_iterator;
    if (is_rounded_)
      rounded_edge_path_iterator.emplace(center_path, (width_ + 1) / 2);
    AutoDarkMode auto_dark_mode(
        PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));
    IterateRightAnglePath(
        is_rounded_ ? right_angle_outer_path_ : center_path,
        [this, top_left_or_bottom_right, &rounded_edge_path_iterator,
         &styled_stroke, &auto_dark_mode](const Vector<Line>& lines) {
          for (wtf_size_t i = 0; i < lines.size(); i++) {
            const Line& line = lines[i];
            std::optional<SkPath> rounded_edge_path;
            if (rounded_edge_path_iterator)
              rounded_edge_path = rounded_edge_path_iterator->Next();
            bool is_top_or_left =
                line.start.x() < line.end.x() || line.start.y() > line.end.y();
            if (is_top_or_left != top_left_or_bottom_right)
              continue;
            const Line& prev_line = lines[i == 0 ? lines.size() - 1 : i - 1];
            const Line& next_line = lines[i == lines.size() - 1 ? 0 : i + 1];
            GraphicsContextStateSaver clip_saver(context_);
            context_.ClipPath(
                MiterClipPath(prev_line.start, line, next_line.end),
                kNotAntiAliased);
            if (is_rounded_) {
              context_.SetStrokeThickness(styled_stroke.Thickness());
              context_.StrokePath(*rounded_edge_path, auto_dark_mode);
            } else {
              PaintStraightEdge(line, styled_stroke, auto_dark_mode);
            }
          }
        });
  }

  void MakeClipOutPath(SkPath& path) const {
    // Add a counter-clockwise rect around the path, so that with kWinding fill
    // type:
    // 1. the areas enclosed in clockwise boundaries become "out",
    // 2. the areas outside of the original path become "in", and
    // 3. the areas enclosed in counter-clockwise boundaries are still "in".
    // This is different from kInverseWinding or GraphicsContext::ClipOut()
    // in #3, which is important not to clip out the areas enclosed by crossing
    // edges produced when shrinking from the outer path.
    DCHECK_EQ(path.getFillType(), SkPathFillType::kWinding);
    path.addRect(right_angle_outer_path_.getBounds(), SkPathDirection::kCCW);
  }

  FloatRoundedRect::Radii ComputeRadii(int outset) const {
    DCHECK(is_rounded_);
    return ComputeCornerRadii(style_, reference_border_rect_, offset_ + outset);
  }

  SkPath CenterPath(bool prefer_outer_half = false) const {
    SkPath center_path = right_angle_outer_path_;
    // If |prefer_outer_half| and width_ is odd_, give the outer half 1 more
    // pixel than the inner half.
    int outset_from_inner = prefer_outer_half ? width_ / 2 : (width_ + 1) / 2;
    ShrinkRightAnglePath(center_path, width_ - outset_from_inner);
    if (is_rounded_) {
      auto center_radii = ComputeRadii(outset_from_inner);
      AddCornerRadiiToPath(center_path, center_radii, center_radii);
    }
    return center_path;
  }

  static int MiterSlope(const SkPoint& p1,
                        const SkPoint& p2,
                        const SkPoint& p3) {
    if (p1.x() == p2.x())
      return (p3.x() > p2.x()) == (p2.y() > p1.y()) ? 1 : -1;
    return (p3.y() > p2.y()) == (p2.x() > p1.x()) ? 1 : -1;
  }

  // Apply clip to remove the extra part of an edge exceeding the miters
  // (formed by 45deg divisions between edges, across the rounded or right-angle
  // corners). The clip should be big enough to include rounded corners within
  // the miters.
  SkPath MiterClipPath(const SkPoint& prev_point,
                       const Line& line,
                       const SkPoint& next_point) const {
    SkRect bounds = right_angle_outer_path_.getBounds();
    int start_miter_slope = MiterSlope(prev_point, line.start, line.end);
    int end_miter_slope = MiterSlope(line.start, line.end, next_point);
    SkPoint p1 = SkPoint::Make(
        line.start.x() + start_miter_slope * (line.start.y() - bounds.top()),
        bounds.top());
    SkPoint p2 = SkPoint::Make(
        line.end.x() + end_miter_slope * (line.end.y() - bounds.top()),
        bounds.top());
    SkPoint p3 = SkPoint::Make(
        line.end.x() - end_miter_slope * (bounds.bottom() - line.end.y()),
        bounds.bottom());
    SkPoint p4 = SkPoint::Make(
        line.start.x() - start_miter_slope * (bounds.bottom() - line.start.y()),
        bounds.bottom());
    // If start_miter_slope == end_miter_slope, the clip path is a parallelogram
    // which is good for both horizontal and vertical edges. Otherwise the path
    // is a trapezoid or a butterfly quadrilateral, and a vertical edge is
    // outside of the path.
    auto path = SkPath::Polygon({p1, p2, p3, p4}, /*isClosed*/ true);
    if (start_miter_slope != end_miter_slope && line.start.x() == line.end.x())
      path.setFillType(SkPathFillType::kInverseWinding);
    return path;
  }

  void PaintStraightEdge(const Line& line,
                         const StyledStrokeData& styled_stroke,
                         const AutoDarkMode& auto_dark_mode) {
    Line adjusted_line = line;
    // GraphicsContext::DrawLine requires the line to be top-to-down or
    // left-to-right get correct interval among dots/dashes.
    if (line.start.x() > line.end.x() || line.start.y() > line.end.y())
      std::swap(adjusted_line.start, adjusted_line.end);
    // Extend the line to fully cover the corners at both endpoints.
    int joint_offset = (width_ + 1) / 2;
    ExtendLineAtEndpoint(adjusted_line.start, adjusted_line.end, joint_offset);
    ExtendLineAtEndpoint(adjusted_line.end, adjusted_line.start, joint_offset);
    context_.DrawLine(
        gfx::ToRoundedPoint(gfx::SkPointToPointF(adjusted_line.start)),
        gfx::ToRoundedPoint(gfx::SkPointToPointF(adjusted_line.end)),
        styled_stroke, auto_dark_mode);
  }

  GraphicsContext& context_;
  const Vector<gfx::Rect>& rects_;
  const PhysicalRect& reference_border_rect_;
  const ComputedStyle& style_;
  EBorderStyle outline_style_;
  int offset_;
  int width_;
  Color color_;
  bool is_rounded_;
  SkPath right_angle_outer_path_;
};

float DefaultFocusRingCornerRadius(const ComputedStyle& style) {
  // Default style is corner radius equal to outline width.
  return FocusRingStrokeWidth(style);
}

FloatRoundedRect::Radii GetFocusRingCornerRadii(
    const ComputedStyle& style,
    const PhysicalRect& reference_border_rect,
    const LayoutObject::OutlineInfo& info) {
  if (style.HasBorderRadius() &&
      (!style.HasEffectiveAppearance() || style.HasAuthorBorderRadius())) {
    auto radii = ComputeCornerRadii(style, reference_border_rect, info.offset);
    radii.SetMinimumRadius(DefaultFocusRingCornerRadius(style));
    return radii;
  }

  if (!style.HasAuthorBorder() && style.HasEffectiveAppearance()) {
    // For the elements that have not been styled and that have an appearance,
    // the focus ring should use the same border radius as the one used for
    // drawing the element.
    std::optional<ui::NativeTheme::Part> part;
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
              part.value(), reference_border_rect.size.width,
              reference_border_rect.size.height);
      corner_radius =
          ui::NativeTheme::GetInstanceForWeb()->AdjustBorderRadiusByZoom(
              part.value(), corner_radius, style.EffectiveZoom());
      return FloatRoundedRect::Radii(corner_radius);
    }
  }

  return FloatRoundedRect::Radii(DefaultFocusRingCornerRadius(style));
}

void PaintSingleFocusRing(GraphicsContext& context,
                          const Vector<gfx::Rect>& rects,
                          float width,
                          int offset,
                          const FloatRoundedRect::Radii& corner_radii,
                          const Color& color,
                          const AutoDarkMode& auto_dark_mode) {
  DCHECK(!rects.empty());
  SkPath path;
  if (!ComputeRightAnglePath(path, rects, offset, 0))
    return;

  SkRect rect;
  if (path.isRect(&rect)) {
    context.DrawFocusRingRect(
        SkRRect(FloatRoundedRect(gfx::SkRectToRectF(rect), corner_radii)),
        color, width, auto_dark_mode);
    return;
  }

  std::optional<float> corner_radius = corner_radii.UniformRadius();
  if (corner_radius.has_value()) {
    context.DrawFocusRingPath(path, color, width, *corner_radius,
                              auto_dark_mode);
    return;
  }

  // Bake non-uniform radii into the path, and draw the path with 0 corner
  // radius as the path already has rounded corners.
  AddCornerRadiiToPath(path, corner_radii, corner_radii);
  context.DrawFocusRingPath(path, color, width, 0, auto_dark_mode);
}

void PaintFocusRing(GraphicsContext& context,
                    const Vector<gfx::Rect>& rects,
                    const ComputedStyle& style,
                    const FloatRoundedRect::Radii& corner_radii,
                    const LayoutObject::OutlineInfo& info) {
  Color inner_color = style.VisitedDependentColor(GetCSSPropertyOutlineColor());
#if !BUILDFLAG(IS_MAC)
  if (style.DarkColorScheme()) {
    inner_color = Color::kWhite;
  }
#endif

  const float outer_ring_width = FocusRingOuterStrokeWidth(style);
  const float inner_ring_width = FocusRingInnerStrokeWidth(style);
  const int offset = FocusRingOffset(style, info);

  Color outer_color =
      style.DarkColorScheme() ? Color(0x10, 0x10, 0x10) : Color::kWhite;
  PaintSingleFocusRing(context, rects, outer_ring_width,
                       offset + std::ceil(inner_ring_width), corner_radii,
                       outer_color, AutoDarkMode::Disabled());
  // Draw the inner ring using |outer_ring_width| (which should be wider than
  // the additional offset of the outer ring) over the outer ring to ensure no
  // gaps or AA artifacts.
  DCHECK_GE(outer_ring_width, std::ceil(inner_ring_width));
  PaintSingleFocusRing(context, rects, outer_ring_width, offset, corner_radii,
                       inner_color, AutoDarkMode::Disabled());
}

}  // anonymous namespace

void OutlinePainter::PaintOutlineRects(
    const PaintInfo& paint_info,
    const DisplayItemClient& client,
    const Vector<PhysicalRect>& outline_rects,
    const LayoutObject::OutlineInfo& info,
    const ComputedStyle& style) {
  DCHECK(style.HasOutline());
  DCHECK(!outline_rects.empty());

  if (DrawingRecorder::UseCachedDrawingIfPossible(paint_info.context, client,
                                                  paint_info.phase))
    return;

  Vector<gfx::Rect> pixel_snapped_outline_rects;
  std::optional<gfx::Rect> united_outline_rect;
  for (auto& r : outline_rects) {
    gfx::Rect pixel_snapped_rect = ToPixelSnappedRect(r);
    // Keep empty rect for normal outline, but not for focus rings.
    if (!pixel_snapped_rect.IsEmpty() || !style.OutlineStyleIsAuto()) {
      pixel_snapped_outline_rects.push_back(pixel_snapped_rect);
      if (!united_outline_rect)
        united_outline_rect = pixel_snapped_rect;
      else
        united_outline_rect->UnionEvenIfEmpty(pixel_snapped_rect);
    }
  }
  if (pixel_snapped_outline_rects.empty())
    return;

  gfx::Rect visual_rect = *united_outline_rect;
  visual_rect.Outset(OutlineOutsetExtent(style, info));
  DrawingRecorder recorder(paint_info.context, client, paint_info.phase,
                           visual_rect);

  if (style.OutlineStyleIsAuto()) {
    auto corner_radii = GetFocusRingCornerRadii(style, outline_rects[0], info);
    PaintFocusRing(paint_info.context, pixel_snapped_outline_rects, style,
                   corner_radii, info);
    return;
  }

  if (*united_outline_rect == pixel_snapped_outline_rects[0]) {
    gfx::Outsets offset =
        AdjustedOutlineOffset(*united_outline_rect, info.offset);
    BoxBorderPainter::PaintSingleRectOutline(
        paint_info.context, style, outline_rects[0], info.width,
        PhysicalBoxStrut(offset.top(), offset.right(), offset.bottom(),
                         offset.left()));
    return;
  }

  ComplexOutlinePainter(paint_info.context, pixel_snapped_outline_rects,
                        outline_rects[0], style, info)
      .Paint();
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
      FocusRingStrokeWidth(style), DefaultFocusRingCornerRadius(style),
      PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
}

int OutlinePainter::OutlineOutsetExtent(const ComputedStyle& style,
                                        const LayoutObject::OutlineInfo& info) {
  if (!style.HasOutline())
    return 0;
  if (style.OutlineStyleIsAuto()) {
    // Unlike normal outlines (whole width is outside of the offset), focus
    // rings are drawn with only part of it outside of the offset.
    return FocusRingOffset(style, info) +
           std::ceil(FocusRingStrokeWidth(style) / 3.f) * 2;
  }
  return base::ClampAdd(info.width, info.offset).Max(0);
}

void OutlinePainter::IterateRightAnglePathForTesting(
    const SkPath& path,
    const base::RepeatingCallback<void(const Vector<Line>&)>& contour_action) {
  IterateRightAnglePath(path, [contour_action](const Vector<Line>& lines) {
    contour_action.Run(lines);
  });
}

}  // namespace blink
