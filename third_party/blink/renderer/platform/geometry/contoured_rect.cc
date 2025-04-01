// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

#include <numbers>

#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

using Corner = ContouredRect::Corner;

namespace {

float AdjustCurvature(float curvature,
                      float origin_length,
                      float target_length) {
  if (curvature <= ContouredRect::CornerCurvature::kNotch) {
    return 0;
  }

  // The computation only works on concave corners. So inverse to convex and
  // inverse the result.
  if (curvature < 1) {
    return 1 / AdjustCurvature(1 / curvature, origin_length, target_length);
  }

  // Find the curvature whose half corner has the expected distance from the
  // origin's half corner.
  return Corner::CurvatureForHalfCorner(
      (Corner::HalfCornerForCurvature(curvature) * origin_length +
       (target_length - origin_length) / std::numbers::sqrt2) /
      target_length);
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

  return rect_string + " curvature:(" + GetCornerCurvature().ToString() + ")";
}

bool ContouredRect::IntersectsQuad(const gfx::QuadF& quad) const {
  return HasRoundCurvature() ? rect_.IntersectsQuad(quad)
                             : GetPath().Intersects(quad);
}

void ContouredRect::OutsetForMarginOrShadow(const gfx::OutsetsF& outsets) {
  // For ordinary rounded rects, we use the existing formula.
  if (HasRoundCurvature()) {
    rect_.OutsetForMarginOrShadow(outsets);
    return;
  }

  // For anything else, keep the same proportions between the original radii and
  // the original rect.
  gfx::RectF new_rect = rect_.Rect();
  FloatRoundedRect::Radii radii = rect_.GetRadii();
  new_rect.Outset(outsets);

  CHECK(!rect_.IsEmpty());
  float scale_x = new_rect.width() / rect_.Rect().width();
  float scale_y = new_rect.height() / rect_.Rect().height();
  radii.SetTopLeft(gfx::ScaleSize(radii.TopLeft(), scale_x, scale_y));
  radii.SetTopRight(gfx::ScaleSize(radii.TopRight(), scale_x, scale_y));
  radii.SetBottomRight(gfx::ScaleSize(radii.BottomRight(), scale_x, scale_y));
  radii.SetBottomLeft(gfx::ScaleSize(radii.BottomLeft(), scale_x, scale_y));
  rect_.SetRadii(radii);
  rect_.SetRect(new_rect);
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

// This method creates a corner from a target (this) and an origin.
// The resulting "aligned" corner has its coordinates and curvature adjusted
// in such a way that it would have consistent thickness along its entire path.
Corner ContouredRect::Corner::AlignedToOrigin(const Corner& origin) const {
  if (IsZero() || *this == origin) {
    return *this;
  }

  const float curvature = origin.Curvature();

  // The thickness is derived from the difference between the target and the
  // origin, in the v1 (start->outer) and v2 (outer->end) directions.
  const float thickness_start = (origin.v2().Length() - v2().Length());
  const float thickness_end = (origin.v1().Length() - v1().Length());

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

  const Corner adjusted_corner = {{origin.Start() + v1_offset + v2_offset,
                                   origin.Outer() + v2_offset + v3_offset,
                                   origin.End() + v3_offset + v4_offset,
                                   origin.Center() + v4_offset + v1_offset},
                                  curvature};

  // For curvatures greater than 2 or lesser than 0.5, it is no longer possible
  // to adjust using the offset, so instead we slightly adjust the curvature of
  // the target corner to have |thickness| distance between the origin and the
  // destination's half corners points.
  return {adjusted_corner.vertices_,
          AdjustCurvature(curvature, origin.DiagonalLength(),
                          adjusted_corner.DiagonalLength())};
}

// static
float ContouredRect::Corner::CurvatureForHalfCorner(float half_corner) {
  return half_corner >= 1   ? ContouredRect::CornerCurvature::kStraight
         : half_corner <= 0 ? ContouredRect::CornerCurvature::kNotch
                            : std::log(0.5) / std::log(half_corner);
}

}  // namespace blink
