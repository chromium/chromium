// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/path_builder.h"

#include <array>
#include <numbers>
#include <optional>
#include <utility>

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_types.h"
#include "third_party/blink/renderer/platform/geometry/skia_geometry_utils.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {

using Corner = ContouredRect::Corner;

// Given a superellipse with the supplied curvature in the coordinate space
// -1,-1,1,1, returns 3 vectors (2 control points and the end point)
// of a bezier curve, going from t=0 (0, 1) clockwise to t=0.5 (45 degrees),
// and following the path of the superellipse with a small margin of error.
// TODO(fserb) document how this works.
std::array<gfx::Vector2dF, 3> ApproximateSuperellipseHalfCornerAsBezierCurve(
    float curvature) {
  static constexpr std::array<double, 7> p = {
      1.2430920942724248, 2.010479023614843,  0.32922901179443753,
      0.2823023142212073, 1.3473704261055421, 2.9149468637949814,
      0.9106507102917086};

  // This formula only works with convex superellipses. To apply to a concave
  // superellipse, flip the center and the outer point and apply the
  // equivalent convex formula (1/curvature).
  DCHECK_GE(curvature, 1);
  const float s = std::log2(curvature);
  const float slope =
      p[0] + (p[6] - p[0]) * 0.5 * (1 + std::tanh(p[5] * (s - p[1])));
  const float base = 1 / (1 + std::exp(-slope * (0 - p[1])));
  const float logistic = 1 / (1 + std::exp(-slope * (s - p[1])));

  const float a = (logistic - base) / (1 - base);
  const float b = p[2] * std::exp(-p[3] * std::pow(s, p[4]));

  // This is the superellipse formula at t=0.5 (45 degrees),
  // the middle of the corner.
  const float half_corner = Corner::HalfCornerForCurvature(curvature);

  const gfx::Vector2dF P1(a, 1);
  const gfx::Vector2dF P2(half_corner - b, half_corner + b);
  const gfx::Vector2dF P3(half_corner, half_corner);
  return {P1, P2, P3};
}

// Adds a curved corner to a path. The vertex argument is the 4 points
// of the corner rectangle, starting from the beginning of the corner
// and continuing clockwise.
void AddCurvedCorner(SkPathBuilder& path, const Corner& corner) {
  if (corner.IsConcave()) {
    AddCurvedCorner(path, corner.Inverse());
    return;
  }

  CHECK_GE(corner.Curvature(), 1);
  // Start the path from the beginning of the curve.
  path.lineTo(gfx::PointFToSkPoint(corner.Start()));

  if (corner.IsStraight() || corner.IsEmpty()) {
    // Straight or very close to it, draw two lines.
    path.lineTo(gfx::PointFToSkPoint(corner.Outer()));
    path.lineTo(gfx::PointFToSkPoint(corner.End()));
  } else if (corner.IsBevel()) {
    path.lineTo(gfx::PointFToSkPoint(corner.End()));
  } else if (corner.IsRound()) {
    path.conicTo(gfx::PointFToSkPoint(corner.Outer()),
                 gfx::PointFToSkPoint(corner.End()), SK_ScalarRoot2Over2);
  } else {
    // Approximate 1/2 corner (45 degrees) of the superellipse as a
    // cubic bezier curve, and draw it twice, transposed, meeting at the t=0.5
    // (45 degrees) point.
    std::array<gfx::Vector2dF, 3> control_points =
        ApproximateSuperellipseHalfCornerAsBezierCurve(corner.Curvature());

    path.cubicTo(gfx::PointFToSkPoint(corner.MapPoint(control_points.at(0))),
                 gfx::PointFToSkPoint(corner.MapPoint(control_points.at(1))),
                 gfx::PointFToSkPoint(corner.MapPoint(control_points.at(2))));

    path.cubicTo(gfx::PointFToSkPoint(
                     corner.MapPoint(TransposeVector2d(control_points.at(1)))),
                 gfx::PointFToSkPoint(
                     corner.MapPoint(TransposeVector2d(control_points.at(0)))),
                 gfx::PointFToSkPoint(corner.End()));
  }
}

std::optional<gfx::PointF> LineIntersection(const gfx::PointF& a0,
                                            const gfx::PointF& a1,
                                            const gfx::PointF& b0,
                                            const gfx::PointF& b1) {
  const gfx::Vector2dF a_length = a1 - a0;
  const gfx::Vector2dF b_length = b1 - b0;
  const double denominator = gfx::CrossProduct(a_length, b_length);
  if (std::abs(denominator) < 1e-6) {
    return std::nullopt;
  }
  const double scale = gfx::CrossProduct(b0 - a0, b_length) / denominator;
  return a0 + gfx::ScaleVector2d(a_length, scale);
}

std::optional<gfx::PointF> SegmentLineIntersection(const gfx::PointF& a0,
                                                   const gfx::PointF& a1,
                                                   const gfx::PointF& b0,
                                                   const gfx::PointF& b1) {
  const gfx::Vector2dF a_length = a1 - a0;
  const gfx::Vector2dF b_length = b1 - b0;
  const double denominator = gfx::CrossProduct(a_length, b_length);
  if (std::abs(denominator) < 1e-6) {
    return std::nullopt;
  }

  const gfx::Vector2dF offset = b0 - a0;
  const double a_scale = gfx::CrossProduct(offset, b_length) / denominator;
  if (a_scale >= 0 && a_scale <= 1) {
    return a0 + gfx::ScaleVector2d(a_length, a_scale);
  }

  const double b_scale = gfx::CrossProduct(offset, a_length) / denominator;
  if (b_scale >= 0 && b_scale <= 1) {
    return b0 + gfx::ScaleVector2d(b_length, b_scale);
  }
  return std::nullopt;
}

// Builds the border-aligned corner clip-out path from CSS Borders 4:
// https://drafts.csswg.org/css-borders-4/#border-aligned-corner-clip-out-path
std::optional<SkPath> CornerClipOutPath(const Corner& origin,
                                        const Corner& target,
                                        float edge_inset_start,
                                        float edge_inset_end) {
  const float start_radius = origin.v2().Length();
  const float end_radius = origin.v3().Length();
  if (!start_radius || !end_radius || origin.IsStraight()) {
    return std::nullopt;
  }

  // Preserve insets beyond the radius because FloatRoundedRect clamps their
  // target radii to zero.
  const float start_inset = edge_inset_start >= 0 && !target.v2().Length()
                                ? edge_inset_start
                                : start_radius - target.v2().Length();
  const float end_inset = edge_inset_end >= 0 && !target.v3().Length()
                              ? edge_inset_end
                              : end_radius - target.v3().Length();
  const float superellipse_parameter = std::log2(origin.Curvature());
  const float inset_difference =
      std::clamp(end_inset - start_inset, -start_radius, end_radius);
  if (superellipse_parameter <= 0 &&
      (inset_difference == -start_radius || inset_difference == end_radius)) {
    return std::nullopt;
  }

  const Corner aligned =
      target.AlignedToOrigin(origin, edge_inset_start, edge_inset_end);
  const gfx::Vector2dF normalized_v3 = gfx::NormalizeVector2d(origin.v3());
  const gfx::Vector2dF normalized_v2 = gfx::NormalizeVector2d(origin.v2());
  const gfx::PointF original_outer =
      target.Outer() - gfx::ScaleVector2d(normalized_v3, end_inset) -
      gfx::ScaleVector2d(normalized_v2, start_inset);
  const gfx::PointF original_start =
      original_outer + gfx::ScaleVector2d(normalized_v3, end_radius);
  const gfx::PointF original_end =
      original_outer + gfx::ScaleVector2d(normalized_v2, start_radius);

  gfx::PointF miter_start = aligned.Start();
  gfx::PointF miter_end = aligned.End();
  if (superellipse_parameter < 0 && start_inset < 0) {
    const gfx::Vector2dF start_normal =
        gfx::NormalizeVector2d(gfx::ScaleVector2d(
            aligned.Start() - original_start, 1.0f / start_inset));
    const gfx::Vector2dF start_tangent(-start_normal.y(), start_normal.x());
    miter_start =
        LineIntersection(aligned.Start(), aligned.Start() - start_tangent,
                         target.Outer() + normalized_v3, target.Outer())
            .value_or(aligned.Start());
  }
  if (superellipse_parameter < 0 && end_inset < 0) {
    const gfx::Vector2dF end_normal = gfx::NormalizeVector2d(
        gfx::ScaleVector2d(aligned.End() - original_end, 1.0f / end_inset));
    const gfx::Vector2dF end_tangent(-end_normal.y(), end_normal.x());
    miter_end = LineIntersection(aligned.End(), aligned.End() + end_tangent,
                                 target.Outer() + normalized_v2, target.Outer())
                    .value_or(aligned.End());
  }

  std::optional<gfx::PointF> self_intersection;
  if (superellipse_parameter < 0 && start_inset < 0 && end_inset < 0 &&
      (-end_inset >= start_radius || -start_inset >= end_radius)) {
    self_intersection = SegmentLineIntersection(miter_start, aligned.Start(),
                                                miter_end, aligned.End());
  }

  SkPathBuilder path;
  path.moveTo(gfx::PointFToSkPoint(miter_start));
  if (self_intersection) {
    path.lineTo(gfx::PointFToSkPoint(*self_intersection));
  } else if (origin.IsNotch()) {
    path.lineTo(gfx::PointFToSkPoint(aligned.Center()));
  } else {
    AddCurvedCorner(path, aligned);
  }
  path.lineTo(gfx::PointFToSkPoint(miter_end));
  path.lineTo(gfx::PointFToSkPoint(target.Outer()));
  path.close();
  return path.detach();
}

}  // anonymous namespace

PathBuilder::PathBuilder() = default;
PathBuilder::~PathBuilder() = default;

PathBuilder::PathBuilder(const Path& path) : builder_(path.GetSkPath()) {}

void PathBuilder::ClearCachedData() {
  current_path_.reset();
  current_bounds_.reset();
}

void PathBuilder::Reset() {
  builder_.reset();
  ClearCachedData();
}

Path PathBuilder::Finalize() {
  ClearCachedData();
  return builder_.detach();
}

gfx::RectF PathBuilder::BoundingRect() const {
  if (!current_bounds_) {
    current_bounds_.emplace(gfx::SkRectToRectF(builder_.computeBounds()));
  }
  return current_bounds_.value();
}

Path PathBuilder::CurrentPath() const {
  if (!current_path_) {
    current_path_.emplace(builder_.snapshot());
  }

  return current_path_.value();
}

std::optional<gfx::PointF> PathBuilder::CurrentPoint() const {
  if (auto point = builder_.getLastPt()) {
    return gfx::SkPointToPointF(*point);
  }
  return std::nullopt;
}

PathBuilder& PathBuilder::Close() {
  builder_.close();

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::MoveTo(const gfx::PointF& pt) {
  builder_.moveTo(gfx::PointFToSkPoint(pt));

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::LineTo(const gfx::PointF& pt) {
  builder_.lineTo(gfx::PointFToSkPoint(pt));

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::QuadTo(const gfx::PointF& ctrl,
                                 const gfx::PointF& pt) {
  builder_.quadTo(gfx::PointFToSkPoint(ctrl), gfx::PointFToSkPoint(pt));

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::CubicTo(const gfx::PointF& ctrl1,
                                  const gfx::PointF& ctrl2,
                                  const gfx::PointF& pt) {
  builder_.cubicTo(gfx::PointFToSkPoint(ctrl1), gfx::PointFToSkPoint(ctrl2),
                   gfx::PointFToSkPoint(pt));

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::ArcTo(const gfx::PointF& p,
                                float radius_x,
                                float radius_y,
                                float x_rotate,
                                bool large_arc,
                                bool sweep) {
  builder_.arcTo(
      SkVector{radius_x, radius_y}, x_rotate,
      large_arc ? SkPathBuilder::kLarge_ArcSize : SkPathBuilder::kSmall_ArcSize,
      sweep ? SkPathDirection::kCW : SkPathDirection::kCCW,
      gfx::PointFToSkPoint(p));

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::ArcTo(const gfx::PointF& p1,
                                const gfx::PointF& p2,
                                float radius) {
  builder_.arcTo(gfx::PointFToSkPoint(p1), gfx::PointFToSkPoint(p2), radius);

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddRect(const gfx::PointF& origin,
                                  const gfx::PointF& opposite_point) {
  builder_.addRect(SkRect::MakeLTRB(origin.x(), origin.y(), opposite_point.x(),
                                    opposite_point.y()),
                   SkPathDirection::kCW, 0);

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddPath(const Path& src,
                                  const AffineTransform& transform) {
  builder_.addPath(src.GetSkPath(), transform.ToSkMatrix());

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddRoundedRect(const FloatRoundedRect& rect,
                                         bool clockwise) {
  builder_.addRRect(SkRRect(rect),
                    clockwise ? SkPathDirection::kCW : SkPathDirection::kCCW,
                    /* start at upper-left after corner radius */ 0);

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddCorner(const ContouredRect::Corner& corner) {
  AddCurvedCorner(builder_, corner);

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddContouredRect(
    const ContouredRect& contoured_rect) {
  const FloatRoundedRect& target_rect = contoured_rect.AsRoundedRect();

  if (contoured_rect.HasRoundCurvature()) {
    AddRoundedRect(target_rect);
    return *this;
  }
  const FloatRoundedRect& origin_rect = contoured_rect.GetOriginRect();

  auto draw_as_single_path = [&]() {
    const Corner top_right_corner = contoured_rect.TopRightCorner();
    MoveTo(top_right_corner.Start());
    AddCurvedCorner(builder_, top_right_corner);
    AddCurvedCorner(builder_, contoured_rect.BottomRightCorner());
    AddCurvedCorner(builder_, contoured_rect.BottomLeftCorner());
    AddCurvedCorner(builder_, contoured_rect.TopLeftCorner());
    Close();
    ClearCachedData();
  };

  // The identity case needs no corner clipping; draw the contour directly to
  // keep exact vertices and avoid path-op antialiasing artifacts.
  if (origin_rect == target_rect) {
    draw_as_single_path();
    return *this;
  }

  const ContouredRect::CornerCurvature& curvature =
      contoured_rect.GetCornerCurvature();
  const std::array<Corner, 4> origin_corners = {
      Corner(origin_rect.TopRightCorner(), 0, curvature.TopRight()),
      Corner(origin_rect.BottomRightCorner(), 1, curvature.BottomRight()),
      Corner(origin_rect.BottomLeftCorner(), 2, curvature.BottomLeft()),
      Corner(origin_rect.TopLeftCorner(), 3, curvature.TopLeft())};
  const std::array<Corner, 4> target_corners = {
      Corner(target_rect.TopRightCorner(), 0, curvature.TopRight()),
      Corner(target_rect.BottomRightCorner(), 1, curvature.BottomRight()),
      Corner(target_rect.BottomLeftCorner(), 2, curvature.BottomLeft()),
      Corner(target_rect.TopLeftCorner(), 3, curvature.TopLeft())};
  const std::array<std::pair<float, float>, 4> edge_insets = {{
      {target_rect.Rect().y() - origin_rect.Rect().y(),
       origin_rect.Rect().right() - target_rect.Rect().right()},
      {origin_rect.Rect().right() - target_rect.Rect().right(),
       origin_rect.Rect().bottom() - target_rect.Rect().bottom()},
      {origin_rect.Rect().bottom() - target_rect.Rect().bottom(),
       target_rect.Rect().x() - origin_rect.Rect().x()},
      {target_rect.Rect().x() - origin_rect.Rect().x(),
       target_rect.Rect().y() - origin_rect.Rect().y()},
  }};

  SkPath result = SkPath::Rect(gfx::RectFToSkRect(target_rect.Rect()));
  bool success = true;
  for (size_t i = 0; i < origin_corners.size(); ++i) {
    if (std::optional<SkPath> clip_out =
            CornerClipOutPath(origin_corners[i], target_corners[i],
                              edge_insets[i].first, edge_insets[i].second)) {
      SkPath clipped;
      if (!Op(result, *clip_out, kDifference_SkPathOp, &clipped)) {
        success = false;
        break;
      }
      result = std::move(clipped);
    }
  }

  if (success) {
    builder_.addPath(result);
  } else {
    draw_as_single_path();
  }
  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddEllipse(const gfx::PointF& p,
                                     float radius_x,
                                     float radius_y,
                                     float start_angle,
                                     float end_angle) {
  DCHECK(EllipseIsRenderable(start_angle, end_angle));
  DCHECK_GE(start_angle, 0);
  DCHECK_LT(start_angle, kTwoPiFloat);

  const SkRect oval = SkRect::MakeLTRB(p.x() - radius_x, p.y() - radius_y,
                                       p.x() + radius_x, p.y() + radius_y);

  const float start_degrees = Rad2deg(start_angle);
  const float sweep_degrees = Rad2deg(end_angle - start_angle);

  // We can't use SkPath::addOval(), because addOval() makes a new sub-path.
  // addOval() calls moveTo() and close() internally.

  // Use 180, not 360, because SkPath::arcTo(oval, angle, 360, false) draws
  // nothing.
  // TODO(fmalita): we should fix that in Skia.
  if (WebCoreFloatNearlyEqual(std::abs(sweep_degrees), 360)) {
    // incReserve() results in a single allocation instead of multiple as is
    // done by multiple calls to arcTo().
    builder_.incReserve(10, 5, 4);
    // // SkPath::arcTo can't handle the sweepAngle that is equal to or greater
    // // than 2Pi.
    const float sweep180 = std::copysign(180, sweep_degrees);
    builder_.arcTo(oval, start_degrees, sweep180, false);
    builder_.arcTo(oval, start_degrees + sweep180, sweep180, false);
  } else {
    builder_.arcTo(oval, start_degrees, sweep_degrees, false);
  }
  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddEllipse(const gfx::PointF& p,
                                     float radius_x,
                                     float radius_y,
                                     float rotation,
                                     float start_angle,
                                     float end_angle) {
  DCHECK(EllipseIsRenderable(start_angle, end_angle));
  DCHECK_GE(start_angle, 0);
  DCHECK_LT(start_angle, kTwoPiFloat);

  if (!rotation) {
    return AddEllipse(p, radius_x, radius_y, start_angle, end_angle);
  }

  // Add an arc after the relevant transform.
  AffineTransform ellipse_transform =
      AffineTransform::Translation(p.x(), p.y()).RotateRadians(rotation);
  DCHECK(ellipse_transform.IsInvertible());
  AffineTransform inverse_ellipse_transform = ellipse_transform.Inverse();
  Transform(inverse_ellipse_transform);
  AddEllipse(gfx::PointF(), radius_x, radius_y, start_angle, end_angle);
  return Transform(ellipse_transform);
}

PathBuilder& PathBuilder::AddRect(const gfx::RectF& rect) {
  // Start at upper-left, add clock-wise.
  builder_.addRect(gfx::RectFToSkRect(rect), SkPathDirection::kCW, 0);

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::AddEllipse(const gfx::PointF& center,
                                     float radius_x,
                                     float radius_y) {
  // Start at 3 o'clock, add clock-wise.
  builder_.addOval(
      SkRect::MakeLTRB(center.x() - radius_x, center.y() - radius_y,
                       center.x() + radius_x, center.y() + radius_y),
      SkPathDirection::kCW, 1);

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::SetWindRule(WindRule rule) {
  const SkPathFillType fill_type = WebCoreWindRuleToSkFillType(rule);

  if (fill_type == builder_.fillType()) {
    return *this;
  }

  builder_.setFillType(fill_type);

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::Translate(const gfx::Vector2dF& offset) {
  builder_.offset(offset.x(), offset.y());

  ClearCachedData();
  return *this;
}

PathBuilder& PathBuilder::Transform(const AffineTransform& xform) {
  builder_.transform(xform.ToSkMatrix());

  ClearCachedData();
  return *this;
}

}  // namespace blink
