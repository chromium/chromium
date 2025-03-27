// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/path_builder.h"

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/infinite_int_rect.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_types.h"
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
void AddCurvedCorner(SkPath& path, const Corner& corner) {
  if (corner.IsConcave()) {
    AddCurvedCorner(path, corner.Inverse());
    return;
  }

  CHECK_GE(corner.Curvature(), 1);
  // Start the path from the beginning of the curve.
  path.lineTo(gfx::PointFToSkPoint(corner.Start()));

  if (corner.IsStraight()) {
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

}  // anonymous namespace

PathBuilder::PathBuilder() = default;
PathBuilder::~PathBuilder() = default;

PathBuilder::PathBuilder(const Path& path) : builder_(path.GetSkPath()) {}

void PathBuilder::Reset() {
  builder_.reset();
  current_path_.reset();
}

Path PathBuilder::Finalize() {
  Path path(std::move(builder_));

  Reset();

  return path;
}

gfx::RectF PathBuilder::BoundingRect() const {
  return gfx::SkRectToRectF(builder_.getBounds());
}

const Path& PathBuilder::CurrentPath() const {
  if (!current_path_) {
    current_path_.emplace(builder_);
  }

  return current_path_.value();
}

PathBuilder& PathBuilder::Close() {
  builder_.close();

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::MoveTo(const gfx::PointF& pt) {
  builder_.moveTo(gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::LineTo(const gfx::PointF& pt) {
  builder_.lineTo(gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::QuadTo(const gfx::PointF& ctrl,
                                 const gfx::PointF& pt) {
  builder_.quadTo(gfx::PointFToSkPoint(ctrl), gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::CubicTo(const gfx::PointF& ctrl1,
                                  const gfx::PointF& ctrl2,
                                  const gfx::PointF& pt) {
  builder_.cubicTo(gfx::PointFToSkPoint(ctrl1), gfx::PointFToSkPoint(ctrl2),
                   gfx::PointFToSkPoint(pt));

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::ArcTo(const gfx::PointF& p,
                                float radius_x,
                                float radius_y,
                                float x_rotate,
                                bool large_arc,
                                bool sweep) {
  builder_.arcTo(radius_x, radius_y, x_rotate,
                 large_arc ? SkPath::kLarge_ArcSize : SkPath::kSmall_ArcSize,
                 sweep ? SkPathDirection::kCW : SkPathDirection::kCCW, p.x(),
                 p.y());

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddRect(const gfx::RectF& rect) {
  // Start at upper-left, add clock-wise.
  builder_.addRect(gfx::RectFToSkRect(rect), SkPathDirection::kCW, 0);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddRect(const gfx::PointF& origin,
                                  const gfx::PointF& opposite_point) {
  builder_.addRect(SkRect::MakeLTRB(origin.x(), origin.y(), opposite_point.x(),
                                    opposite_point.y()),
                   SkPathDirection::kCW, 0);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddPath(const Path& src,
                                  const AffineTransform& transform) {
  builder_.addPath(src.GetSkPath(), transform.ToSkMatrix());

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::AddRoundedRect(const FloatRoundedRect& rect,
                                         bool clockwise) {
  if (rect.IsEmpty()) {
    return *this;
  }

  builder_.addRRect(SkRRect(rect),
                    clockwise ? SkPathDirection::kCW : SkPathDirection::kCCW,
                    /* start at upper-left after corner radius */ 0);

  current_path_.reset();
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

  if (origin_rect == target_rect) {
    // A rect with no insets/outsets, we can draw all the corners and not worry
    // about intersections.
    AddCurvedCorner(builder_, contoured_rect.TopRightCorner());
    AddCurvedCorner(builder_, contoured_rect.BottomRightCorner());
    AddCurvedCorner(builder_, contoured_rect.BottomLeftCorner());
    AddCurvedCorner(builder_, contoured_rect.TopLeftCorner());
    current_path_.reset();
    return *this;
  }

  // This would happen when the target rect is an outset of the origin rect,
  // usually something like a shadow or margin.
  // Draw the adjusted corners, and then add axis-aligned lines to connect them
  // to the target (outset) rect.
  if (target_rect.Rect().Contains(origin_rect.Rect())) {
    const Corner top_right_corner = contoured_rect.TopRightCorner();
    const Corner bottom_right_corner = contoured_rect.BottomRightCorner();
    const Corner bottom_left_corner = contoured_rect.BottomLeftCorner();
    const Corner top_left_corner = contoured_rect.TopLeftCorner();
    AddCurvedCorner(builder_, top_right_corner);
    LineTo(gfx::PointF(target_rect.Rect().right(), top_right_corner.End().y()));
    LineTo(gfx::PointF(target_rect.Rect().right(),
                       bottom_right_corner.Start().y()));
    AddCurvedCorner(builder_, bottom_right_corner);
    LineTo(gfx::PointF(bottom_right_corner.End().x(),
                       target_rect.Rect().bottom()));
    LineTo(gfx::PointF(bottom_left_corner.Start().x(),
                       target_rect.Rect().bottom()));
    AddCurvedCorner(builder_, bottom_left_corner);
    LineTo(gfx::PointF(target_rect.Rect().x(), bottom_left_corner.End().y()));
    LineTo(gfx::PointF(target_rect.Rect().x(), top_left_corner.Start().y()));
    AddCurvedCorner(builder_, top_left_corner);
    LineTo(gfx::PointF(top_left_corner.End().x(), target_rect.Rect().y()));
    LineTo(gfx::PointF(top_right_corner.Start().x(), target_rect.Rect().y()));
    Close();
    return *this;
  }

  // To generate curves that have constant thickness, we compute the
  // superellipse based on the same center and an increased radius. Since the
  // resulting path segments don't start/end at the target rect, we use
  // path-intersection logic, and intersect 3 paths: (1) the target rect (2) the
  // top-left & bottom-right corners together with the bottom-left and top-right
  // of the infinite rect (3) the top-right & bottom-left corners together with
  // the top-left and bottom-right corners of the infinite rect.
  // This generates a path that corresponds to the inset/outset rect but has the
  // corners carved out.
  SkOpBuilder op_builder;

  const SkRect infinite_rect =
      gfx::RectFToSkRect(gfx::RectF(InfiniteIntRect()));

  // Start with the target rect
  op_builder.add(SkPath::Rect(gfx::RectFToSkRect(target_rect.Rect())),
                 kUnion_SkPathOp);

  // Intersect with a path that includes the top-right + bottom-left corners,
  // stretching the other corners to infinity.
  SkPath diagonal_corner_path_1;
  ContouredRect origin_contoured_rect(origin_rect,
                                      contoured_rect.GetCornerCurvature());
  diagonal_corner_path_1.moveTo(infinite_rect.left(), infinite_rect.top());
  AddCurvedCorner(diagonal_corner_path_1, contoured_rect.TopRightCorner());
  diagonal_corner_path_1.lineTo(infinite_rect.right(), infinite_rect.bottom());
  AddCurvedCorner(diagonal_corner_path_1, contoured_rect.BottomLeftCorner());
  diagonal_corner_path_1.close();
  op_builder.add(diagonal_corner_path_1, kIntersect_SkPathOp);

  // Intersect with a path that includes the top-left + bottom-right corners,
  // stretching the other corners to infinity.
  SkPath diagonal_corner_path_2;
  diagonal_corner_path_2.moveTo(infinite_rect.right(), infinite_rect.top());
  AddCurvedCorner(diagonal_corner_path_2, contoured_rect.BottomRightCorner());
  diagonal_corner_path_2.lineTo(infinite_rect.left(), infinite_rect.bottom());
  AddCurvedCorner(diagonal_corner_path_2, contoured_rect.TopLeftCorner());
  diagonal_corner_path_2.close();
  op_builder.add(diagonal_corner_path_2, kIntersect_SkPathOp);
  // Resolve the path-ops and append to this path.
  SkPath result;
  CHECK(op_builder.resolve(&result));
  builder_.addPath(result);
  current_path_.reset();
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

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::SetWindRule(WindRule rule) {
  const SkPathFillType fill_type = WebCoreWindRuleToSkFillType(rule);

  if (fill_type == builder_.getFillType()) {
    return *this;
  }

  builder_.setFillType(fill_type);

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::Translate(const gfx::Vector2dF& offset) {
  builder_.offset(offset.x(), offset.y());

  current_path_.reset();
  return *this;
}

PathBuilder& PathBuilder::Transform(const AffineTransform& xform) {
  builder_.transform(xform.ToSkMatrix());

  current_path_.reset();
  return *this;
}

}  // namespace blink
