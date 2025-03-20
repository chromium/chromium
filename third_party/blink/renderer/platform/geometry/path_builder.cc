// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/path_builder.h"

#include <numbers>

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

constexpr float superellipse_mid_point(float curvature) {
  return std::pow(0.5, 1 / curvature);
}

// Given a superellipse with the supplied curvature in the coordinate space
// -1,-1,1,1, returns 3 vectors (2 control points and the end point)
// of a bezier curve, going from t=0 (0, 1) clockwise to t=0.5 (45 degrees),
// and following the path of the superellipse with a small margin of error.
// TODO(fserb) document how this works.
std::array<gfx::Vector2dF, 3> ApproximateSuperellipseOctantAsBezierCurve(
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
  const float mid_point = superellipse_mid_point(curvature);

  const gfx::Vector2dF P1(a, 1);
  const gfx::Vector2dF P2(mid_point - b, mid_point + b);
  const gfx::Vector2dF P3(mid_point, mid_point);
  return {P1, P2, P3};
}

// 4 vertices in clockwise order, representing a rect with directionality.
using RectVertices = std::array<gfx::PointF, 4>;
enum VertexPoint : size_t { kStart, kOuter, kEnd, kCenter };

constexpr RectVertices FlipCenter(const RectVertices& vertex) {
  return RectVertices{
      vertex.at(VertexPoint::kStart), vertex.at(VertexPoint::kCenter),
      vertex.at(VertexPoint::kEnd), vertex.at(VertexPoint::kOuter)};
}

// Returns the vertices of a corner rect, rotated so they start at the
// innermost corner, and continue clockwise.
RectVertices RotatedRect(const gfx::RectF& rect, size_t rotate) {
  RectVertices points{rect.origin(), rect.top_right(), rect.bottom_right(),
                      rect.bottom_left()};
  std::rotate(points.begin(), points.begin() + rotate, points.end());
  return points;
}

void AddConvexCurvedCorner(SkPath& path,
                           const RectVertices& vertex,
                           float curvature) {
  CHECK_GE(curvature, 1);
  // Start the path from the beginning of the curve.
  path.lineTo(gfx::PointFToSkPoint(vertex.at(VertexPoint::kStart)));

  if (curvature >= 1000) {
    // Straight or very close to it, draw two lines.
    path.lineTo(gfx::PointFToSkPoint(vertex.at(VertexPoint::kOuter)));
    path.lineTo(gfx::PointFToSkPoint(vertex.at(VertexPoint::kEnd)));
  } else if (curvature <= ContouredRect::CornerCurvature::kBevel) {
    path.lineTo(gfx::PointFToSkPoint(vertex.at(VertexPoint::kEnd)));
  } else if (curvature == ContouredRect::CornerCurvature::kRound) {
    path.conicTo(gfx::PointFToSkPoint(vertex.at(VertexPoint::kOuter)),
                 gfx::PointFToSkPoint(vertex.at(VertexPoint::kEnd)),
                 SK_ScalarRoot2Over2);
  } else {
    // Approximate one octant (1/8th) of the superellipse as a
    // cubic bezier curve, and draw it twice, transposed, meeting at the t=0.5
    // (45 degrees) point.
    std::array<gfx::Vector2dF, 3> control_points =
        ApproximateSuperellipseOctantAsBezierCurve(curvature);

    auto map_point = [&](gfx::Vector2dF v) {
      return gfx::PointFToSkPoint(
          vertex.at(VertexPoint::kCenter) +
          gfx::ScaleVector2d(
              vertex.at(VertexPoint::kOuter) - vertex.at(VertexPoint::kStart),
              v.x()) +
          gfx::ScaleVector2d(
              vertex.at(VertexPoint::kOuter) - vertex.at(VertexPoint::kEnd),
              v.y()));
    };

    path.cubicTo(map_point(control_points.at(0)),
                 map_point(control_points.at(1)),
                 map_point(control_points.at(2)));

    path.cubicTo(map_point(TransposeVector2d(control_points.at(1))),
                 map_point(TransposeVector2d(control_points.at(0))),
                 gfx::PointFToSkPoint(vertex.at(VertexPoint::kEnd)));
  }
}

// Adds a curved corner to a path. The vertex argument is the 4 points
// of the corner rectangle, starting from the beginning of the corner
// and continuing clockwise.
void AddCurvedCorner(SkPath& path,
                     const RectVertices& vertex,
                     float curvature) {
  const bool convex = curvature >= 1;
  AddConvexCurvedCorner(
      path, convex ? vertex : FlipCenter(vertex),
      convex ? curvature
             : 1 / std::max(curvature, ContouredRect::CornerCurvature::kNotch));
}

// Add a superellipse curve to the path, considering its origin rect.
// It would render the curve with consistent or gradual distance from
// the corresponding curve on the origin rect.
// This is done by keeping the same center for the superellipse,
// changing the radius and potentially adjusting the curvature.
void AddCurvedCornerFromOrigin(SkPath& path,
                               const RectVertices& target_vertex,
                               RectVertices origin_vertex,
                               float curvature) {
  gfx::Vector2dF offset(((target_vertex.at(VertexPoint::kStart) -
                          origin_vertex.at(VertexPoint::kCenter))
                             .Length() -
                         (origin_vertex.at(VertexPoint::kStart) -
                          origin_vertex.at(VertexPoint::kCenter))
                             .Length()),
                        ((target_vertex.at(VertexPoint::kEnd) -
                          origin_vertex.at(VertexPoint::kCenter))
                             .Length() -
                         (origin_vertex.at(VertexPoint::kEnd) -
                          origin_vertex.at(VertexPoint::kCenter))
                             .Length()));

  // For concave curves, flip the vertex and use the corresponding convex curve.
  if (curvature < 1) {
    curvature = 1 / curvature;
    offset = -offset;
    origin_vertex = FlipCenter(origin_vertex);
  }

  CHECK_GE(curvature, 1);

  if (curvature > 2) {
    // For high curvatures, we change the target curvature to match a
    // superellipse whose distance from the original corner's mid-point is the
    // desired offset.
    const float target_length = (target_vertex.at(VertexPoint::kEnd) -
                                 target_vertex.at(VertexPoint::kStart))
                                    .Length();
    const float origin_length = (origin_vertex.at(VertexPoint::kEnd) -
                                 origin_vertex.at(VertexPoint::kStart))
                                    .Length();
    const auto adjusted_length =
        (target_length - origin_length) / std::numbers::sqrt2;
    const float mid_point = superellipse_mid_point(curvature);
    curvature =
        std::log(0.5) /
        std::log((mid_point * origin_length + adjusted_length) / target_length);
  } else if (curvature < 2) {
    // When 1<=curvature<2, the distance at the edge is greater than the border
    // thickness, and needs to be scaled by a number between 1 and sqrt(2).
    // This formula computes this number by computing the offset that would
    // result in a superellipse whose 45deg point has a distance of 1 from
    // this superellipse.
    offset.Scale(std::pow(2, 1 / curvature - 0.5));
  }

  // For curvature === 2 (round) there is no adjustment to be made.

  const gfx::Vector2dF adjusted_offset_start = gfx::ScaleVector2d(
      gfx::NormalizeVector2d(origin_vertex.at(VertexPoint::kStart) -
                             origin_vertex.at(VertexPoint::kCenter)),
      offset.x());
  const gfx::Vector2dF adjusted_offset_end = gfx::ScaleVector2d(
      gfx::NormalizeVector2d(origin_vertex.at(VertexPoint::kOuter) -
                             origin_vertex.at(VertexPoint::kStart)),
      offset.y());

  const RectVertices adjusted_vertex{
      origin_vertex.at(VertexPoint::kStart) + adjusted_offset_start,
      origin_vertex.at(VertexPoint::kOuter) + adjusted_offset_start +
          adjusted_offset_end,
      origin_vertex.at(VertexPoint::kEnd) + adjusted_offset_end,
      origin_vertex.at(VertexPoint::kCenter)};

  AddConvexCurvedCorner(path, adjusted_vertex, curvature);
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

  const ContouredRect::CornerCurvature& curvature =
      contoured_rect.GetCornerCurvature();
  enum CornerRotation : size_t { TopRight, BottomRight, BottomLeft, TopLeft };
  const FloatRoundedRect origin_rect = contoured_rect.GetOriginRect();

  if (origin_rect == target_rect) {
    // A rect with no insets/outsets, we can draw all the corners and not worry
    // about intersections.
    MoveTo(target_rect.TopLeftCorner().top_right());

    AddCurvedCorner(
        builder_,
        RotatedRect(target_rect.TopRightCorner(), CornerRotation::TopRight),
        curvature.TopRight());
    AddCurvedCorner(builder_,
                    RotatedRect(target_rect.BottomRightCorner(),
                                CornerRotation::BottomRight),
                    curvature.BottomRight());
    AddCurvedCorner(
        builder_,
        RotatedRect(target_rect.BottomLeftCorner(), CornerRotation::BottomLeft),
        curvature.BottomLeft());
    AddCurvedCorner(
        builder_,
        RotatedRect(target_rect.TopLeftCorner(), CornerRotation::TopLeft),
        curvature.TopLeft());
    current_path_.reset();
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
  diagonal_corner_path_1.moveTo(infinite_rect.left(), infinite_rect.top());
  AddCurvedCornerFromOrigin(
      diagonal_corner_path_1,
      RotatedRect(target_rect.TopRightCorner(), CornerRotation::TopRight),
      RotatedRect(origin_rect.TopRightCorner(), CornerRotation::TopRight),
      curvature.TopRight());
  diagonal_corner_path_1.lineTo(infinite_rect.right(), infinite_rect.bottom());
  AddCurvedCornerFromOrigin(
      diagonal_corner_path_1,
      RotatedRect(target_rect.BottomLeftCorner(), CornerRotation::BottomLeft),
      RotatedRect(origin_rect.BottomLeftCorner(), CornerRotation::BottomLeft),
      curvature.BottomLeft());
  diagonal_corner_path_1.close();
  op_builder.add(diagonal_corner_path_1, kIntersect_SkPathOp);

  // Intersect with a path that includes the top-left + bottom-right corners,
  // stretching the other corners to infinity.
  SkPath diagonal_corner_path_2;
  diagonal_corner_path_2.moveTo(infinite_rect.right(), infinite_rect.top());
  AddCurvedCornerFromOrigin(
      diagonal_corner_path_2,
      RotatedRect(target_rect.BottomRightCorner(), CornerRotation::BottomRight),
      RotatedRect(origin_rect.BottomRightCorner(), CornerRotation::BottomRight),
      curvature.BottomRight());
  diagonal_corner_path_2.lineTo(infinite_rect.left(), infinite_rect.bottom());
  AddCurvedCornerFromOrigin(
      diagonal_corner_path_2,
      RotatedRect(target_rect.TopLeftCorner(), CornerRotation::TopLeft),
      RotatedRect(origin_rect.TopLeftCorner(), CornerRotation::TopLeft),
      curvature.TopLeft());
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

}  // namespace blink
