// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include <numbers>

#include "third_party/blink/renderer/platform/geometry/path.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

using Corner = ContouredRect::Corner;

String ContouredRect::CornerCurvature::ToString() const {
  return String::Format("tl:%.2f; tr:%.2f; bl:%.2f; br:%.2f", TopLeft(),
                        TopRight(), BottomLeft(), BottomRight());
}

String ContouredRect::ToString() const {
  String rect_string = rect_.ToString();

  if (HasRoundCurvature()) {
    return rect_string;
  }

  return rect_string + " curvature:(" + GetCornerCurvature().ToString() + ")";
}

bool ContouredRect::IntersectsQuad(const gfx::QuadF& quad) const {
  return HasRoundCurvature() ? rect_.IntersectsQuad(quad)
                             : GetPath().Intersects(quad);
}

Path ContouredRect::GetPath() const {
  return Path::MakeContouredRect(*this);
}

Corner ContouredRect::Corner::AlignedToOrigin(Corner origin) const {
  if (IsZero() || *this == origin) {
    return *this;
  }

  gfx::Vector2dF offset(v2().Length() - origin.v2().Length(),
                        v1().Length() - origin.v1().Length());

  // For concave curves, flip the vertex and use the corresponding convex curve.
  if (origin.IsConcave()) {
    origin = origin.ToConvex();
    offset.Scale(-1);
  }

  float curvature = origin.Curvature();

  CHECK(!origin.IsConcave());

  if (curvature > 2 && !origin.IsStraight()) {
    // For high curvatures, we change the target curvature to match a
    // superellipse whose distance from the original corner's mid-point is the
    // desired offset.
    const float target_length = DiagonalLength();
    const float origin_length = origin.DiagonalLength();
    const float adjusted_length =
        (target_length - origin_length) / std::numbers::sqrt2;

    curvature = CurvatureForHalfCorner(
        (HalfCornerForCurvature(curvature) * origin_length + adjusted_length) /
        target_length);
  } else if (curvature < 2) {
    // When 1<=curvature<2, the distance at the edge is greater than the border
    // thickness, and needs to be scaled by a number between 1 and sqrt(2).
    // This formula computes this number by computing the offset that would
    // result in a superellipse whose 45deg point has a distance of 1 from
    // this superellipse.
    offset.Scale(std::pow(2, 1 / curvature - 0.5));
  }

  // For curvature === 2 (round) and straight there is no adjustment to be made.

  const gfx::Vector2dF adjusted_offset_start =
      gfx::ScaleVector2d(gfx::NormalizeVector2d(origin.v4()), offset.x());
  const gfx::Vector2dF adjusted_offset_end =
      gfx::ScaleVector2d(gfx::NormalizeVector2d(origin.v1()), offset.y());

  return Corner({origin.Start() + adjusted_offset_start,
                 origin.Outer() + adjusted_offset_start + adjusted_offset_end,
                 origin.End() + adjusted_offset_end, origin.Center()},
                curvature);
}

gfx::PointF ContouredRect::Corner::HullPoint() const {
  // This is the x of the hull of the superellipse.
  const float normalized_control_point =
      2 * Corner::HalfCornerForCurvature(curvature_) - 0.5;
  return MapPoint(
      gfx::Vector2dF(normalized_control_point, normalized_control_point));
}

}  // namespace blink
