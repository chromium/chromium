// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include <numbers>

#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
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

void ApplyOutsetAsTransform(FloatRoundedRect& rect,
                            const gfx::OutsetsF& outsets) {
  if (rect.IsEmpty()) {
    return;
  }

  // For anything else, keep the same proportions between the original radii and
  // the original rect.
  gfx::RectF new_rect = rect.Rect();
  FloatRoundedRect::Radii radii = rect.GetRadii();
  new_rect.Outset(outsets);

  float scale_x = new_rect.width() / rect.Rect().width();
  float scale_y = new_rect.height() / rect.Rect().height();
  radii.SetTopLeft(gfx::ScaleSize(radii.TopLeft(), scale_x, scale_y));
  radii.SetTopRight(gfx::ScaleSize(radii.TopRight(), scale_x, scale_y));
  radii.SetBottomRight(gfx::ScaleSize(radii.BottomRight(), scale_x, scale_y));
  radii.SetBottomLeft(gfx::ScaleSize(radii.BottomLeft(), scale_x, scale_y));
  rect.SetRadii(radii);
  rect.SetRect(new_rect);
}
}  // namespace

String ContouredRect::CornerCurvature::ToString() const {
  return String::Format("tl:%.2f; tr:%.2f; bl:%.2f; br:%.2f", TopLeft(),
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
  if (RuntimeEnabledFeatures::ShadowContourFollowsBorderEnabled()) {
    rect_.OutsetWithCornerCorrection(outsets);
    return;
  }

  if (HasRoundCurvature()) {
    rect_.OutsetWithCornerCorrection(outsets);
    return;
  }

  ApplyOutsetAsTransform(rect_, outsets);
  if (origin_rect_) {
    ApplyOutsetAsTransform(*origin_rect_, outsets);
  }
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
  return String::Format("Corner {%s|%s|%s|%s} k=%.2f",
                        Start().ToString().c_str(), Outer().ToString().c_str(),
                        End().ToString().c_str(), Center().ToString().c_str(),
                        curvature_);
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
                                              float thickness_start,
                                              float thickness_end) const {
  if (origin.IsEmpty() || *this == origin) {
    return *this;
  }

  const float curvature = origin.Curvature();
  // The curve should start at a position perpendicular to the curve, with the
  // thickness as the distance. We use the hull of the superellipse (x*2 - 1/2,
  // y*2 - 1/2), normalize a vector in that direction, and find its
  // perpendicular.
  const float clamped_half_corner =
      Corner::HalfCornerForCurvature(ClampTo<float>(curvature, 0.5, 2));
  const gfx::Vector2dF normalized_hull_vector = gfx::NormalizeVector2d(
      gfx::ScaleVector2d(
          gfx::Vector2dF(clamped_half_corner, 1 - clamped_half_corner), 2) -
      gfx::Vector2dF(.5, .5));
  const gfx::Vector2dF adjusted_offset{normalized_hull_vector.x(),
                                       -normalized_hull_vector.y()};

  // Adjust the corner based on the offset & the thickness.
  const gfx::Vector2dF v1_offset =
      gfx::ScaleVector2d(gfx::NormalizeVector2d(origin.v1()),
                         thickness_start * adjusted_offset.y());
  const gfx::Vector2dF v2_offset =
      gfx::ScaleVector2d(gfx::NormalizeVector2d(origin.v2()),
                         thickness_start * adjusted_offset.x());
  const gfx::Vector2dF v3_offset = gfx::ScaleVector2d(
      gfx::NormalizeVector2d(origin.v3()), thickness_end * adjusted_offset.x());
  const gfx::Vector2dF v4_offset = gfx::ScaleVector2d(
      gfx::NormalizeVector2d(origin.v4()), thickness_end * adjusted_offset.y());

  return Corner{{origin.Start() + v1_offset + v2_offset,
                 origin.Outer() + v2_offset + v3_offset,
                 origin.End() + v3_offset + v4_offset,
                 origin.Center() + v4_offset + v1_offset},
                curvature};
}

// static
float ContouredRect::Corner::CurvatureForHalfCorner(float half_corner) {
  return half_corner >= 1   ? ContouredRect::CornerCurvature::kStraight
         : half_corner <= 0 ? ContouredRect::CornerCurvature::kNotch
                            : std::log(0.5) / std::log(half_corner);
}

}  // namespace blink
