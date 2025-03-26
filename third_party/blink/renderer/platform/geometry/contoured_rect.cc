// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include <numbers>

#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
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

  const bool originally_concave = origin.IsConcave();

  // For concave curves, flip the vertex and use the corresponding convex curve.
  if (originally_concave) {
    origin = origin.Inverse();
    offset.Scale(-1);
  }

  CHECK(!origin.IsConcave());

  // When 1<=curvature<2, the distance at the edge is greater than the border
  // thickness, and needs to be scaled by a number between 1 and sqrt(2).
  // This formula computes this number by computing the offset that would
  // result in a superellipse whose 45deg point has a distance of 1 from
  // this superellipse.
  if (origin.Curvature() < 2) {
    offset.Scale(std::pow(2, 1 / origin.Curvature() - 0.5));
  }

  const gfx::Vector2dF adjusted_offset_start =
      gfx::ScaleVector2d(gfx::NormalizeVector2d(origin.v4()), offset.x());
  const gfx::Vector2dF adjusted_offset_end =
      gfx::ScaleVector2d(gfx::NormalizeVector2d(origin.v1()), offset.y());

  Corner target_corner(
      {origin.Start() + adjusted_offset_start,
       origin.Outer() + adjusted_offset_start + adjusted_offset_end,
       origin.End() + adjusted_offset_end, origin.Center()},
      origin.Curvature());

  if (origin.Curvature() <= 2 || target_corner.IsStraight()) {
    return originally_concave ? target_corner.Inverse() : target_corner;
  }

  // For highly concave or convex curvatures (>2 or <0.5), we adjust the target
  // curvature to a value that would generate a half-corner point whose distance
  // from the origin half-corner point is consistent with the thickness.
  float origin_length = origin.DiagonalLength();
  float target_length = target_corner.DiagonalLength();
  const float adjusted_length =
      (target_length - origin_length) / std::numbers::sqrt2;
  target_corner.curvature_ = Corner::CurvatureForHalfCorner(
      (Corner::HalfCornerForCurvature(origin.Curvature()) * origin_length +
       adjusted_length) /
      target_length);

  return originally_concave ? target_corner.Inverse() : target_corner;
}

// static
float ContouredRect::Corner::CurvatureForHalfCorner(float half_corner) {
  return half_corner >= 1   ? ContouredRect::CornerCurvature::kStraight
         : half_corner <= 0 ? ContouredRect::CornerCurvature::kNotch
                            : std::log(0.5) / std::log(half_corner);
}

}  // namespace blink
