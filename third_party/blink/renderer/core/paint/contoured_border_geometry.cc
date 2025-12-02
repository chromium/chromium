// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/contoured_border_geometry.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/line_f.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

namespace {

FloatRoundedRect::Radii CalcRadiiFor(const ComputedStyle& style,
                                     gfx::SizeF size,
                                     PhysicalBoxSides sides_to_include) {
  return FloatRoundedRect::Radii(
      sides_to_include.top && sides_to_include.left &&
              !style.CornerTopLeftShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderTopLeftRadius(), size)
          : gfx::SizeF(),
      sides_to_include.top && sides_to_include.right &&
              !style.CornerTopRightShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderTopRightRadius(), size)
          : gfx::SizeF(),
      sides_to_include.bottom && sides_to_include.left &&
              !style.CornerBottomLeftShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderBottomLeftRadius(), size)
          : gfx::SizeF(),
      sides_to_include.bottom && sides_to_include.right &&
              !style.CornerBottomRightShape().IsDegenerate()
          ? SizeForLengthSize(style.BorderBottomRightRadius(), size)
          : gfx::SizeF());
}

float EffectiveCurvature(Superellipse superellipse, const gfx::SizeF& radius) {
  return radius.IsEmpty() ? ContouredRect::CornerCurvature::kRound
                          : superellipse.Exponent();
}

gfx::QuadF ComputeHullQuad(const ContouredRect::Corner& corner) {
  const gfx::PointF half_corner = corner.HalfCorner();
  const gfx::PointF perpendicular_line =
      half_corner + gfx::LineF(corner.Outer(), half_corner).Normal();
  const gfx::LineF tangent_line(half_corner, perpendicular_line);
  const gfx::PointF intersection_point_1 =
      tangent_line.IntersectionWith({corner.Start(), corner.Center()})
          .value_or(corner.Center());
  const gfx::PointF intersection_point_2 =
      tangent_line.IntersectionWith({corner.End(), corner.Center()})
          .value_or(corner.Center());

  return gfx::QuadF(corner.Start(), intersection_point_1, intersection_point_2,
                    corner.End());
}

gfx::QuadF ScaleQuadFromOrigin(const gfx::QuadF& quad,
                               const gfx::PointF& origin,
                               float scale) {
  return AffineTransform()
      .Translate(origin.x(), origin.y())
      .Scale(scale)
      .Translate(-origin.x(), -origin.y())
      .MapQuad(quad);
}

// The "optimal" hull scale is the scale where the hulls touch but do not
// intersect. It is computed by attempting to scale both quads and seeing if
// they intersect, binary-searching until the difference between an intersecting
// and a non-intersecting scale is below a certain threshold, thus "nearly
// touching" but not intersecting.
float SolveOptimalHullScale(const gfx::QuadF& hull_a,
                            const gfx::PointF& origin_a,
                            const gfx::QuadF& hull_b,
                            const gfx::PointF& origin_b,
                            float min_scale,
                            float max_scale) {
  static const float kEpsilon = 0.05;
  const float check_scale = (min_scale + max_scale) / 2;
  if (max_scale - min_scale <= kEpsilon) {
    return min_scale;
  }

  const gfx::QuadF scaled_hull_a =
      ScaleQuadFromOrigin(hull_a, origin_a, check_scale);
  const gfx::QuadF scaled_hull_b =
      ScaleQuadFromOrigin(hull_b, origin_b, check_scale);
  if (scaled_hull_a.IntersectsQuad(scaled_hull_b)) {
    return SolveOptimalHullScale(hull_a, origin_a, hull_b, origin_b, min_scale,
                                 check_scale);
  } else {
    return SolveOptimalHullScale(hull_a, origin_a, hull_b, origin_b,
                                 check_scale, max_scale);
  }
}

// Constrain the radii so that the opposite hulls don't overlap.
// If they do overlap, compute an optimal scale, which if applied to both quads
// would make them touch but not intersect.
float RadiiConstraintFactorForOppositeCorners(const ContouredRect::Corner& a,
                                              const ContouredRect::Corner& b) {
  const gfx::RectF bbox_a = a.BoundingBox();
  const gfx::RectF bbox_b = b.BoundingBox();
  if (!bbox_a.Intersects(bbox_b)) {
    return 1;
  }
  const gfx::QuadF hull_a = ComputeHullQuad(a);
  const gfx::QuadF hull_b = ComputeHullQuad(b);
  if (!hull_a.IntersectsQuad(hull_b)) {
    return 1;
  }

  // TODO(nrosenthal): we can optimize it by pre-calculating a better min_scale
  // or max_scale, but it's unclear yet how helpful that would be.
  return SolveOptimalHullScale(hull_a, a.Outer(), hull_b, b.Outer(), 0, 1);
}

ContouredRect ComputeContouredBorderFromStyle(
    const ComputedStyle& style,
    const gfx::RectF& border_rect,
    const PhysicalSize& physical_size_for_radii,
    PhysicalBoxSides sides_to_include) {
  FloatRoundedRect rounded_rect(border_rect);
  if (style.HasBorderRadius()) {
    rounded_rect.SetRadii(CalcRadiiFor(
        style, gfx::SizeF(physical_size_for_radii), sides_to_include));
    rounded_rect.ConstrainRadii();
  }

  ContouredRect result(rounded_rect);
  if (!rounded_rect.IsRounded()) {
    return result;
  }

  FloatRoundedRect::Radii radii = rounded_rect.GetRadii();
  const ContouredRect::CornerCurvature curvature(
      EffectiveCurvature(style.CornerTopLeftShape(), radii.TopLeft()),
      EffectiveCurvature(style.CornerTopRightShape(), radii.TopRight()),
      EffectiveCurvature(style.CornerBottomRightShape(), radii.BottomRight()),
      EffectiveCurvature(style.CornerBottomLeftShape(), radii.BottomLeft()));
  if (curvature.IsRound()) {
    return result;
  }

  result.SetCornerCurvature(curvature);
  if (!curvature.IsConvex()) {
    const float radii_constraint_factor_for_opposite_corners = std::min<float>(
        1, std::min(RadiiConstraintFactorForOppositeCorners(
                        result.TopLeftCorner(), result.BottomRightCorner()),
                    RadiiConstraintFactorForOppositeCorners(
                        result.BottomLeftCorner(), result.TopRightCorner())));
    CHECK_LE(radii_constraint_factor_for_opposite_corners, 1);
    if (radii_constraint_factor_for_opposite_corners != 1) {
      radii.Scale(radii_constraint_factor_for_opposite_corners);
      result.SetRadii(radii);
    }
  }

  result.SetOriginRect(result.AsRoundedRect());
  return result;
}

}  // anonymous namespace

ContouredRect ContouredBorderGeometry::PixelSnappedContouredBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  return ComputeContouredBorderFromStyle(
      style, gfx::RectF(ToPixelSnappedRect(border_rect)), border_rect.size,
      sides_to_include);
}

ContouredRect ContouredBorderGeometry::ContouredBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  return ComputeContouredBorderFromStyle(style, gfx::RectF(border_rect),
                                         border_rect.size, sides_to_include);
}

ContouredRect ContouredBorderGeometry::ContouredInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect) {
  ContouredRect rounded_border = ContouredBorder(style, border_rect);
  rounded_border.Inset(gfx::InsetsF()
                           .set_top(style.BorderTopWidth())
                           .set_right(style.BorderRightWidth())
                           .set_bottom(style.BorderBottomWidth())
                           .set_left(style.BorderLeftWidth()));
  return rounded_border;
}

ContouredRect ContouredBorderGeometry::PixelSnappedContouredInnerBorder(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    PhysicalBoxSides sides_to_include) {
  return PixelSnappedContouredBorderWithOutsets(
      style, border_rect,
      PhysicalBoxStrut::FromInts(
          -style.BorderTopWidth(), -style.BorderRightWidth(),
          -style.BorderBottomWidth(), -style.BorderLeftWidth()),
      sides_to_include);
}

ContouredRect ContouredBorderGeometry::PixelSnappedContouredBorderWithOutsets(
    const ComputedStyle& style,
    const PhysicalRect& border_rect,
    const PhysicalBoxStrut& outsets,
    PhysicalBoxSides sides_to_include) {
  PhysicalBoxStrut adjusted_outsets(
      sides_to_include.top ? outsets.top : LayoutUnit(),
      sides_to_include.right ? outsets.right : LayoutUnit(),
      sides_to_include.bottom ? outsets.bottom : LayoutUnit(),
      sides_to_include.left ? outsets.left : LayoutUnit());
  PhysicalRect rect_with_outsets = border_rect;
  rect_with_outsets.Expand(adjusted_outsets);
  rect_with_outsets.size.ClampNegativeToZero();

  // The standard ToPixelSnappedRect(const PhysicalRect&) will not
  // let small sizes snap to zero, but that has the side effect here of
  // preventing an inner border for a very thin element from snapping to
  // zero size as occurs when a unit width border is applied to a sub-pixel
  // sized element. So round without forcing non-near-zero sizes to one.
  ContouredRect contoured_rect(FloatRoundedRect(gfx::Rect(
      ToRoundedPoint(rect_with_outsets.offset),
      gfx::Size(SnapSizeToPixelAllowingZero(rect_with_outsets.Width(),
                                            rect_with_outsets.X()),
                SnapSizeToPixelAllowingZero(rect_with_outsets.Height(),
                                            rect_with_outsets.Y())))));

  if (style.HasBorderRadius()) {
    ContouredRect pixel_snapped_rounded_border =
        PixelSnappedContouredBorder(style, border_rect, sides_to_include);
    pixel_snapped_rounded_border.Outset(gfx::OutsetsF(adjusted_outsets));
    contoured_rect.SetRadii(pixel_snapped_rounded_border.GetRadii());
    contoured_rect.SetCornerCurvature(
        pixel_snapped_rounded_border.GetCornerCurvature());
    contoured_rect.SetOriginRect(pixel_snapped_rounded_border.GetOriginRect());
  }
  return contoured_rect;
}

}  // namespace blink
