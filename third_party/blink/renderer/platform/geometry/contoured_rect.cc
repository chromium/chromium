// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include <numbers>

#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "ui/gfx/geometry/line_f.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

using Corner = ContouredRect::Corner;
using CornerCurvature = ContouredRect::CornerCurvature;

namespace {
float CornerRectIntercept(float y,
                          const gfx::RectF& corner_rect,
                          float curvature) {
  DCHECK_GT(corner_rect.height(), 0);

  // Retain existing logic for rounded curvature, to keep backwards
  // compatibility. The general-case version has some floating point rounding
  // differences.
  if (curvature == CornerCurvature::kRound) {
    return corner_rect.width() *
           sqrt(1 - (y * y) / (corner_rect.height() * corner_rect.height()));
  }

  // A concave superellipse is a mirror image of the convex version, rather than
  // the direct superellipse.
  if (curvature < CornerCurvature::kBevel) {
    return corner_rect.width() - CornerRectIntercept(corner_rect.height() - y,
                                                     corner_rect,
                                                     1 / curvature);
  }
  return corner_rect.width() *
         std::pow(1 - std::pow(y / corner_rect.height(), curvature),
                  1 / curvature);
}
}  // namespace

String ContouredRect::CornerCurvature::ToString() const {
  return Format("tl:{:.2f}; tr:{:.2f}; bl:{:.2f}; br:{:.2f}", TopLeft(),
                TopRight(), BottomLeft(), BottomRight());
}

String ContouredRect::ToString() const {
  String rect_string = rect_.ToString();

  if (HasRoundCurvature()) {
    return rect_string;
  }

  return StrCat(
      {rect_string, " curvature:(", GetCornerCurvature().ToString(), ")"});
}

bool ContouredRect::IntersectsQuad(const gfx::QuadF& quad) const {
  return HasRoundCurvature() ? rect_.IntersectsQuad(quad)
                             : GetPath().Intersects(quad);
}

void ContouredRect::OutsetWithCornerCorrection(const gfx::OutsetsF& outsets) {
  rect_.OutsetWithCornerCorrection(outsets);
}

bool ContouredRect::XInterceptsAtY(float y,
                                   float& min_x_intercept,
                                   float& max_x_intercept) const {
  if (y < Rect().y() || y > Rect().bottom()) {
    return false;
  }

  if (!IsRounded()) {
    min_x_intercept = Rect().x();
    max_x_intercept = Rect().right();
    return true;
  }

  const gfx::RectF& top_left_rect = rect_.TopLeftCorner();
  const gfx::RectF& bottom_left_rect = rect_.BottomLeftCorner();

  if (!top_left_rect.IsEmpty() && y >= top_left_rect.y() &&
      y < top_left_rect.bottom()) {
    min_x_intercept =
        top_left_rect.right() -
        CornerRectIntercept(top_left_rect.bottom() - y, top_left_rect,
                            corner_curvature_.TopLeft());
  } else if (!bottom_left_rect.IsEmpty() && y >= bottom_left_rect.y() &&
             y <= bottom_left_rect.bottom()) {
    min_x_intercept =
        bottom_left_rect.right() -
        CornerRectIntercept(y - bottom_left_rect.y(), bottom_left_rect,
                            corner_curvature_.BottomLeft());
  } else {
    min_x_intercept = rect_.Rect().x();
  }

  const gfx::RectF& top_right_rect = rect_.TopRightCorner();
  const gfx::RectF& bottom_right_rect = rect_.BottomRightCorner();

  if (!top_right_rect.IsEmpty() && y >= top_right_rect.y() &&
      y <= top_right_rect.bottom()) {
    max_x_intercept =
        top_right_rect.x() + CornerRectIntercept(top_right_rect.bottom() - y,
                                                 top_right_rect,
                                                 corner_curvature_.TopRight());
  } else if (!bottom_right_rect.IsEmpty() && y >= bottom_right_rect.y() &&
             y <= bottom_right_rect.bottom()) {
    max_x_intercept =
        bottom_right_rect.x() +
        CornerRectIntercept(y - bottom_right_rect.y(), bottom_right_rect,
                            corner_curvature_.BottomRight());
  } else {
    max_x_intercept = rect_.Rect().right();
  }

  return true;
}

Path ContouredRect::GetPath() const {
  return Path::MakeContouredRect(*this);
}

String ContouredRect::Corner::ToString() const {
  return Format("Corner {{{}|{}|{}|{}}} k={:.2f}", Start().ToString().c_str(),
                Outer().ToString().c_str(), End().ToString().c_str(),
                Center().ToString().c_str(), curvature_);
}

gfx::PointF ContouredRect::Corner::QuadraticControlPoint() const {
  if (IsConcave()) {
    return Inverse().QuadraticControlPoint();
  }

  // For hyperellipses (round and above), there is no equivalent quadratic, so
  // we use the outer point.
  if (Curvature() >= CornerCurvature::kRound) {
    return Outer();
  }

  // For hypoellipses (between bevel and round), the quadratic curve is very
  // close to the superellipse. Given a point (P, P) at t=0.5, the quadratic
  // control point is at 2 * P - 0.5.
  const float normalized_control_point =
      2 * HalfCornerForCurvature(curvature_) - 0.5;
  return MapPoint(
      gfx::Vector2dF(normalized_control_point, normalized_control_point));
}

// This method creates a corner from a target (this) and an origin.
// The resulting "aligned" corner has its coordinates and curvature adjusted
// in such a way that it would have consistent thickness along its entire path.
Corner ContouredRect::Corner::AlignedToOrigin(const Corner& origin,
                                              float edge_inset_start,
                                              float edge_inset_end) const {
  if (origin.IsEmpty() || *this == origin) {
    return *this;
  }

  const float start_radius = origin.v2().Length();
  const float end_radius = origin.v3().Length();
  // Preserve insets beyond the radius because FloatRoundedRect clamps their
  // target radii to zero.
  const float start_inset = edge_inset_start >= 0 && !v2().Length()
                                ? edge_inset_start
                                : start_radius - v2().Length();
  const float end_inset = edge_inset_end >= 0 && !v3().Length()
                              ? edge_inset_end
                              : end_radius - v3().Length();
  const float superellipse_parameter = std::log2(origin.Curvature());
  const float exponent = std::exp2(std::abs(superellipse_parameter));
  const float convex_half_corner = std::pow(0.5f, 1.0f / exponent);
  const float half_corner = superellipse_parameter < 0
                                ? 1.0f - convex_half_corner
                                : convex_half_corner;
  const float control_point_x =
      std::clamp(half_corner / (std::numbers::sqrt2_v<float> - 1.0f) -
                     1.0f / std::numbers::sqrt2_v<float>,
                 0.0f, 1.0f);
  const float inset_difference =
      std::clamp(end_inset - start_inset, -start_radius, end_radius);

  float start_control_point_x = control_point_x;
  float end_control_point_x = control_point_x;
  if (inset_difference != 0) {
    const float bevel_normal_delta =
        std::sqrt(start_radius * start_radius + end_radius * end_radius -
                  inset_difference * inset_difference);
    const float bevel_normal_x =
        end_radius * inset_difference + start_radius * bevel_normal_delta;
    const float bevel_normal_y =
        -start_radius * inset_difference + end_radius * bevel_normal_delta;
    const float bevel_control_point_x =
        start_radius * bevel_normal_y /
        (start_radius * bevel_normal_y + end_radius * bevel_normal_x);
    start_control_point_x =
        superellipse_parameter < 0
            ? bevel_control_point_x * (2.0f * control_point_x)
            : 1.0f - (1.0f - bevel_control_point_x) *
                         (2.0f * (1.0f - control_point_x));
    end_control_point_x = 2.0f * control_point_x - start_control_point_x;
  }

  const gfx::Vector2dF unmapped_start_normal = gfx::NormalizeVector2d(
      gfx::Vector2dF((1.0f - start_control_point_x) * start_radius,
                     start_control_point_x * end_radius));
  const gfx::Vector2dF unmapped_end_normal = gfx::NormalizeVector2d(
      gfx::Vector2dF(end_control_point_x * start_radius,
                     (1.0f - end_control_point_x) * end_radius));
  const gfx::Vector2dF normalized_v3 = gfx::NormalizeVector2d(origin.v3());
  const gfx::Vector2dF normalized_v2 = gfx::NormalizeVector2d(origin.v2());
  const gfx::Vector2dF start_normal =
      gfx::ScaleVector2d(normalized_v3, unmapped_start_normal.x()) +
      gfx::ScaleVector2d(normalized_v2, unmapped_start_normal.y());
  const gfx::Vector2dF end_normal =
      gfx::ScaleVector2d(normalized_v3, unmapped_end_normal.x()) +
      gfx::ScaleVector2d(normalized_v2, unmapped_end_normal.y());

  const gfx::PointF original_outer =
      Outer() - gfx::ScaleVector2d(normalized_v3, end_inset) -
      gfx::ScaleVector2d(normalized_v2, start_inset);
  gfx::PointF adjusted_start = original_outer +
                               gfx::ScaleVector2d(normalized_v3, end_radius) +
                               gfx::ScaleVector2d(start_normal, start_inset);
  gfx::PointF adjusted_end = original_outer +
                             gfx::ScaleVector2d(normalized_v2, start_radius) +
                             gfx::ScaleVector2d(end_normal, end_inset);

  const gfx::Vector2dF start_tangent(-start_normal.y(), start_normal.x());
  const gfx::Vector2dF end_tangent(-end_normal.y(), end_normal.x());
  if (superellipse_parameter >= 0 && start_inset < 0) {
    adjusted_start =
        gfx::LineF(adjusted_start, adjusted_start - start_tangent)
            .IntersectionWith(gfx::LineF(Outer() + normalized_v3, Outer()))
            .value_or(adjusted_start);
  }
  if (superellipse_parameter >= 0 && end_inset < 0) {
    adjusted_end =
        gfx::LineF(adjusted_end, adjusted_end + end_tangent)
            .IntersectionWith(gfx::LineF(Outer() + normalized_v2, Outer()))
            .value_or(adjusted_end);
  }

  const float adjusted_height =
      gfx::DotProduct(adjusted_end - adjusted_start, normalized_v2);
  const gfx::PointF adjusted_outer =
      adjusted_end - gfx::ScaleVector2d(normalized_v2, adjusted_height);
  const gfx::PointF adjusted_center =
      adjusted_start + gfx::ScaleVector2d(normalized_v2, adjusted_height);
  if (superellipse_parameter <= -1 || superellipse_parameter >= 0) {
    return Corner(
        {adjusted_start, adjusted_outer, adjusted_end, adjusted_center},
        origin.Curvature());
  }

  const gfx::PointF tangent_intersection =
      gfx::LineF(adjusted_start, adjusted_start - start_tangent)
          .IntersectionWith(
              gfx::LineF(adjusted_end, adjusted_end + end_tangent))
          .value_or(adjusted_start);
  return Corner({adjusted_start, tangent_intersection, adjusted_end,
                 adjusted_start + (adjusted_end - tangent_intersection)},
                CornerCurvature::kRound);
}

// static
float ContouredRect::Corner::CurvatureForHalfCorner(float half_corner) {
  return half_corner >= 1   ? ContouredRect::CornerCurvature::kStraight
         : half_corner <= 0 ? ContouredRect::CornerCurvature::kNotch
                            : std::log(0.5) / std::log(half_corner);
}

}  // namespace blink
